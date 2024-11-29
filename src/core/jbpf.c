// Copyright (c) Microsoft Corporation. All rights reserved.

#define _GNU_SOURCE
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <string.h>
#include <elf.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <errno.h>
#include <stdio.h>

#include "ubpf.h"

#include "jbpf_memory.h"
#include "jbpf_config.h"
#include "jbpf_hook.h"
#include "jbpf_logging.h"
#include "jbpf_version.h"
#include "jbpf_hook_defs.h"
#include "jbpf_agent_hooks.h"
#include "jbpf.h"
#include "jbpf_memory.h"
#include "jbpf_int.h"
#include "jbpf_io_channel.h"
#include "jbpf_io_hash.h"
#include "jbpf_bpf_hashmap.h"
#include "jbpf_bpf_spsc_hashmap.h"
#include "jbpf_bpf_array.h"
#include "jbpf_helper_impl.h"
#include "jbpf_common_types.h"

DEFINE_JBPF_AGENT_HOOK(periodic_call);

struct jbpf_hook_list jbpf_hook_list;

#ifdef JBPF_DISABLED
struct jbpf_hook __start___hook_list[0] = {{NULL}};
struct jbpf_hook __stop___hook_list[0] = {{NULL}};
#endif

#ifdef JBPF_SHARED_LIB

struct jbpf_hook __start___hook_list[1] __attribute__((weak)) = {{NULL}};
extern struct jbpf_hook __stop___hook_list[1] __attribute__((weak, alias("__start___hook_list")));

#else // STATIC LIBRARY

DECLARE_JBPF_HOOK(default, void* p, p, HOOK_PROTO(void), HOOK_ASSIGN())
DEFINE_JBPF_HOOK(default)

#endif

#ifdef __cplusplus
thread_local int __thread_id = -1;
#else
_Thread_local int __thread_id = -1;
#endif

static struct jbpf_config* jbpf_default_config = NULL;
static struct jbpf_config* jbpf_current_config = NULL;

static sem_t jmaintenance_thread_sem;
static sem_t jio_thread_sem;
static sem_t jagent_thread_sem;

static pthread_mutex_t lcm_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_t jbpf_maintenance_thread;
static pthread_t jbpf_io_thread;
static pthread_t jbpf_agent_thread;

#ifdef __cplusplus
thread_local bool __jbpf_registered_thread = false;
thread_local bool __jbpf_unregistered_thread = false;
thread_local ck_epoch_record_t* e_record;
#else
_Thread_local bool __jbpf_registered_thread = false;
_Thread_local bool __jbpf_unregistered_thread = false;
_Thread_local ck_epoch_record_t* e_record;
#endif

static void
ht_free(void* p, size_t m, bool d)
{
    (void)m;
    (void)d;

    jbpf_free_mem(p);
    return;
}

static void*
ht_malloc(size_t b)
{
    return jbpf_alloc_mem(b);
}

static void*
ht_realloc(void* r, size_t a, size_t b, bool d)
{
    (void)a;
    (void)d;

    return jbpf_realloc_mem(r, b);
}

static struct ck_malloc ht_allocator = {.malloc = ht_malloc, .free = ht_free, .realloc = ht_realloc};

static void
ht_hash_wrapper(struct ck_ht_hash* h, const void* key, size_t length, uint64_t seed)
{
    h->value = (unsigned long)MurmurHash64A(key, length, seed);
}

static void
_jbpf_default_handle_output(struct jbpf_io_stream_id* stream_id, void** bufs, int nbuf, void* ctx)
{
    return;
}
jbpf_io_output_handler_cb _jbpf_handle_output_data_cb = _jbpf_default_handle_output;

ck_epoch_record_t epoch_record_list[JBPF_MAX_NUM_REG_THREADS] = {0};

static struct jbpf_ctx_t jbpf_ctx = {0};
static jbpf_mempool_ctx_t* io_mempool;

int
_jbpf_get_num_reg_jbpf_maps(void)
{
    return jbpf_ctx.nmap_reg;
}

static int
jbpf_codelet_register_map(struct jbpf_codelet* codelet, struct jbpf_map* map)
{
    ck_ht_entry_t map_entry;
    ck_ht_hash_t map_hash;

    if (!codelet || !map) {
        return -1;
    }

    ck_ht_hash(&map_hash, &codelet->maps, map->name, JBPF_MAP_NAME_LEN);
    ck_ht_entry_set(&map_entry, map_hash, map->name, JBPF_MAP_NAME_LEN, map);
    if (ck_ht_put_spmc(&codelet->maps, map_hash, &map_entry)) {
        jbpf_logger(JBPF_DEBUG, "Map %s registered successfully for codelet %s\n", map->name, codelet->name);
        return 0;
    }

    return -1;
}

static void
jbpf_codelet_deregister_map(struct jbpf_codelet* codelet, struct jbpf_map* map)
{
    ck_ht_entry_t map_entry;
    ck_ht_hash_t map_hash;

    if (!codelet || !map) {
        return;
    }

    ck_ht_hash(&map_hash, &codelet->maps, map->name, JBPF_MAP_NAME_LEN);
    ck_ht_entry_key_set(&map_entry, map->name, JBPF_MAP_NAME_LEN);
    if (ck_ht_remove_spmc(&codelet->maps, map_hash, &map_entry)) {
        jbpf_logger(JBPF_DEBUG, "Map %s was removed!\n", map->name);
    }
}

static struct jbpf_map*
jbpf_codelet_lookup_map(struct jbpf_codelet* codelet, const char* map_name)
{
    ck_ht_entry_t map_entry;
    ck_ht_hash_t map_hash;
    char map_name_key[JBPF_MAP_NAME_LEN] = {0};
    strncpy(map_name_key, map_name, JBPF_MAP_NAME_LEN - 1);
    map_name_key[JBPF_MAP_NAME_LEN - 1] = '\0';

    if (!codelet) {
        return NULL;
    }

    ck_ht_hash(&map_hash, &codelet->maps, map_name_key, JBPF_MAP_NAME_LEN);
    ck_ht_entry_key_set(&map_entry, map_name_key, JBPF_MAP_NAME_LEN);

    if (ck_ht_get_spmc(&codelet->maps, map_hash, &map_entry) == true) {
        return ck_ht_entry_value(&map_entry);
    }
    return NULL;
}

static int
_jbpf_create_run_dir(struct jbpf_ctx_t* ctx, char* run_path)
{
    struct stat st;
    if (stat(run_path, &st) == -1) {

        if (mkdir(run_path, S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
            jbpf_logger(JBPF_ERROR, "Error creating directory %s\n", run_path);
            return -1;
        }
        jbpf_logger(JBPF_INFO, "Directory '%s' created.\n", run_path);
    } else {
        jbpf_logger(JBPF_INFO, "Directory '%s' already exists.\n", run_path);
    }

    strncpy(ctx->jbpf_run_path, run_path, JBPF_RUN_PATH_LEN);
    ctx->jbpf_run_path[JBPF_RUN_PATH_LEN - 1] = '\0';

    return 0;
}

static bool
jbpf_hook_exists(const char* hook_name)
{

    const struct jbpf_hook* hook;
    for (int i = 0; i < jbpf_hook_list.num_hooks; i++) {
        hook = jbpf_hook_list.jbpf_hook_p[i];
        if (strncmp(hook->name, hook_name, JBPF_HOOK_NAME_LEN) == 0)
            return true;
    }
    return false;
}

// Function to check that a string parameter is valid
// It checks that the string is not NULL, not empty and does not exceed the maximum length
// Parameters:
//   param_name: name of the parameter
//   param: the string parameter
//   param_maxlen: maximum length of the string
//   err: error structure to store error message
// Return value:
//   1 if the string is valid
//   -1 if the param_name, param or param_maxlen is null or zero
//   0 if the string is not valid (is not set or exceeds maximum length)
int
validate_string_param(char* param_name, char* param, uint32_t param_maxlen, jbpf_codeletset_load_error_s* err)
{
    char msg[JBPF_MAX_ERR_MSG_SIZE];
    msg[0] = '\0';
    int valid = 1;
    if ((!param_name) || (!param) || (param_maxlen == 0)) {
        return -1;
    }

    // get length of param by iterating through the string
    const char* str = (param);
    int len = 0;
    while ((*str != '\0') && (len <= param_maxlen)) {
        str++;
        len++;
    }
    if (len == 0) {
        valid = 0;
        sprintf(msg, "%s is not set\n", param_name);
    } else if (len >= param_maxlen) {
        valid = 0;
        sprintf(msg, "%s exceeds maximum length %u\n", param_name, param_maxlen);
    }

    if (!valid) {
        jbpf_logger(JBPF_ERROR, "%s", msg);
        if (err) {
            strcpy(err->err_msg, msg);
        }
    }
    return valid;
}

static int
validate_codeletset(struct jbpf_codeletset_load_req* load_req, jbpf_codeletset_load_error_s* err)
{

    if (!load_req) {
        char msg[JBPF_MAX_ERR_MSG_SIZE];
        sprintf(msg, "load_req is NULL\n");
        jbpf_logger(JBPF_ERROR, "%s", msg);
        if (err) {
            strcpy(err->err_msg, msg);
        }
        return JBPF_CODELET_LOAD_FAIL;
    }

    if (validate_string_param("codeletset_id.name", load_req->codeletset_id.name, JBPF_CODELETSET_NAME_LEN, err) != 1) {
        return JBPF_CODELET_PARAM_INVALID;
    }

    if (load_req->num_codelet_descriptors == 0) {
        char msg[JBPF_MAX_ERR_MSG_SIZE];
        sprintf(msg, "Number of codelets in codeletset is zero\n");
        jbpf_logger(JBPF_ERROR, "%s", msg);
        if (err) {
            strcpy(err->err_msg, msg);
        }
        return JBPF_CODELET_PARAM_INVALID;
    }

    if (load_req->num_codelet_descriptors > JBPF_MAX_CODELETS_IN_CODELETSET) {
        char msg[JBPF_MAX_ERR_MSG_SIZE];
        sprintf(msg, "Number of codelets in codeletset exceeds maximum allowed\n");
        jbpf_logger(JBPF_ERROR, "%s", msg);
        if (err) {
            strcpy(err->err_msg, msg);
        }
        return JBPF_CODELET_PARAM_INVALID;
    }

    for (int i = 0; i < load_req->num_codelet_descriptors; ++i) {

        // validate codelet_name
        if (validate_string_param(
                "codelet_name", load_req->codelet_descriptor[i].codelet_name, JBPF_CODELET_NAME_LEN, err) != 1) {
            return JBPF_CODELET_PARAM_INVALID;
        }

        // validate hook_name
        if (validate_string_param("hook_name", load_req->codelet_descriptor[i].hook_name, JBPF_HOOK_NAME_LEN, err) !=
            1) {
            return JBPF_CODELET_PARAM_INVALID;
        }

        // validate codelet_path
        if (validate_string_param("codelet_path", load_req->codelet_descriptor[i].codelet_path, JBPF_PATH_LEN, err) !=
            1) {
            return JBPF_CODELET_PARAM_INVALID;
        }

        // validate in_io_channel
        for (int ch = 0; ch < load_req->codelet_descriptor[i].num_in_io_channel; ch++) {
            jbpf_io_channel_desc_s* chan = &load_req->codelet_descriptor[i].in_io_channel[ch];
            if (validate_string_param("in_io_channel.name ", chan->name, JBPF_IO_CHANNEL_NAME_LEN, err) != 1) {
                return JBPF_CODELET_PARAM_INVALID;
            }
#ifdef JBPF_EXPERIMENTAL_FEATURES
            if (chan->has_serde) {
                if (validate_string_param("in_io_channel.serde ", chan->serde.file_path, JBPF_PATH_LEN, err) != 1) {
                    return JBPF_CODELET_PARAM_INVALID;
                }
                // check Serde file exists
                FILE* file = fopen(chan->serde.file_path, "rb");
                if (!file) {
                    char msg[JBPF_MAX_ERR_MSG_SIZE];
                    sprintf(msg, "in_io_channel.serde does not exist : %s \n", chan->serde.file_path);
                    jbpf_logger(JBPF_ERROR, "%s", msg);
                    if (err) {
                        strcpy(err->err_msg, msg);
                    }
                    return JBPF_CODELET_PARAM_INVALID;
                }
                fclose(file);
            }
#endif // JBPF_EXPERIMENTAL_FEATURES
        }

        // validate out_io_channel
        for (int ch = 0; ch < load_req->codelet_descriptor[i].num_out_io_channel; ch++) {
            jbpf_io_channel_desc_s* chan = &load_req->codelet_descriptor[i].out_io_channel[ch];
            if (validate_string_param("out_io_channel.name ", chan->name, JBPF_IO_CHANNEL_NAME_LEN, err) != 1) {
                return JBPF_CODELET_PARAM_INVALID;
            }
#ifdef JBPF_EXPERIMENTAL_FEATURES
            if (chan->has_serde) {
                if (validate_string_param("out_io_channel.serde ", chan->serde.file_path, JBPF_PATH_LEN, err) != 1) {
                    return JBPF_CODELET_PARAM_INVALID;
                }
                // check Serde file exists
                FILE* file = fopen(chan->serde.file_path, "rb");
                if (!file) {
                    char msg[JBPF_MAX_ERR_MSG_SIZE];
                    sprintf(msg, "out_io_channel.serde does not exist : %s \n", chan->serde.file_path);
                    jbpf_logger(JBPF_ERROR, "%s", msg);
                    if (err) {
                        strcpy(err->err_msg, msg);
                    }
                    return JBPF_CODELET_PARAM_INVALID;
                }
                fclose(file);
            }
#endif // JBPF_EXPERIMENTAL_FEATURES
        }

        // validate linked_maps
        for (int m = 0; m < load_req->codelet_descriptor[i].num_linked_maps; m++) {
            jbpf_linked_map_descriptor_s* map = &load_req->codelet_descriptor[i].linked_maps[m];
            if (validate_string_param("linked_maps.map_name ", map->map_name, JBPF_MAP_NAME_LEN, err) != 1) {
                return JBPF_CODELET_PARAM_INVALID;
            }
            if (validate_string_param(
                    "linked_maps.linked_codelet_name ", map->linked_codelet_name, JBPF_CODELET_NAME_LEN, err) != 1) {
                return JBPF_CODELET_PARAM_INVALID;
            }

            if (validate_string_param("linked_maps.linked_map_name ", map->linked_map_name, JBPF_MAP_NAME_LEN, err) !=
                1) {
                return JBPF_CODELET_PARAM_INVALID;
            }
        }
        // check that a codelet does not link to itself
        for (int m = 0; m < load_req->codelet_descriptor[i].num_linked_maps; m++) {
            if (strcmp(
                    load_req->codelet_descriptor[i].linked_maps[m].linked_codelet_name,
                    load_req->codelet_descriptor[i].codelet_name) == 0) {
                char msg[JBPF_MAX_ERR_MSG_SIZE];
                sprintf(
                    msg,
                    "codelet %s has a linked map in the same codelet (%s -> %s) \n",
                    load_req->codelet_descriptor[i].linked_maps[m].linked_codelet_name,
                    load_req->codelet_descriptor[i].linked_maps[m].linked_map_name,
                    load_req->codelet_descriptor[i].linked_maps[m].map_name);
                jbpf_logger(JBPF_ERROR, "%s", msg);
                if (err) {
                    strcpy(err->err_msg, msg);
                }
                return JBPF_CODELET_PARAM_INVALID;
            }
        }
        // check that map_name is unique
        for (int m = 0; m < load_req->codelet_descriptor[i].num_linked_maps; m++) {
            for (int j = m + 1; j < load_req->codelet_descriptor[i].num_linked_maps; ++j) {
                if (strcmp(
                        load_req->codelet_descriptor[i].linked_maps[m].map_name,
                        load_req->codelet_descriptor[i].linked_maps[j].map_name) == 0) {
                    char msg[JBPF_MAX_ERR_MSG_SIZE];
                    sprintf(
                        msg, "map_name %s is not unique. \n", load_req->codelet_descriptor[i].linked_maps[m].map_name);
                    jbpf_logger(JBPF_ERROR, "%s", msg);
                    if (err) {
                        strcpy(err->err_msg, msg);
                    }
                    return JBPF_CODELET_PARAM_INVALID;
                }
            }
        }
        // check that linked_codelet_name and linked_map_name tuple are unique
        for (int m = 0; m < load_req->codelet_descriptor[i].num_linked_maps; m++) {
            for (int j = m + 1; j < load_req->codelet_descriptor[i].num_linked_maps; ++j) {
                if (strcmp(
                        load_req->codelet_descriptor[i].linked_maps[m].linked_codelet_name,
                        load_req->codelet_descriptor[i].linked_maps[j].linked_codelet_name) == 0 &&
                    strcmp(
                        load_req->codelet_descriptor[i].linked_maps[m].linked_map_name,
                        load_req->codelet_descriptor[i].linked_maps[j].linked_map_name) == 0) {
                    char msg[JBPF_MAX_ERR_MSG_SIZE];
                    sprintf(
                        msg,
                        "linked_codelet_name %s and linked_map_name %s tuple is not unique. \n",
                        load_req->codelet_descriptor[i].linked_maps[m].linked_codelet_name,
                        load_req->codelet_descriptor[i].linked_maps[m].linked_map_name);
                    jbpf_logger(JBPF_ERROR, "%s", msg);
                    if (err) {
                        strcpy(err->err_msg, msg);
                    }
                    return JBPF_CODELET_PARAM_INVALID;
                }
            }
        }
    }

    // check that the codelet_name fields are unique
    for (int i = 0; i < load_req->num_codelet_descriptors; ++i) {
        for (int j = i + 1; j < load_req->num_codelet_descriptors; ++j) {
            if (strcmp(load_req->codelet_descriptor[i].codelet_name, load_req->codelet_descriptor[j].codelet_name) ==
                0) {
                char msg[JBPF_MAX_ERR_MSG_SIZE];
                sprintf(
                    msg,
                    "codelet_name %s is not unique. Unloading codeletset %s\n",
                    load_req->codelet_descriptor[i].codelet_name,
                    load_req->codeletset_id.name);
                jbpf_logger(JBPF_ERROR, "%s", msg);
                if (err) {
                    strcpy(err->err_msg, msg);
                }
                return JBPF_CODELET_PARAM_INVALID;
            }
        }
    }

    return JBPF_CODELET_LOAD_SUCCESS;
}

static void*
_jbpf_read_binary_file(char* path, size_t* size)
{

    if (!path) {
        *size = 0;
        return NULL;
    }

    FILE* file = fopen(path, "rb");
    if (!file) {
        jbpf_logger(JBPF_ERROR, "Failed to find the file %s\n", path);
        *size = 0;
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        jbpf_logger(JBPF_ERROR, "Failed to seek end of file %s\n", path);
        fclose(file);
        *size = 0;
        return NULL;
    }

    long file_size = ftell(file);
    if (file_size == -1) {
        jbpf_logger(JBPF_ERROR, "Failed to determine file size for %s\n", path);
        fclose(file);
        *size = 0;
        return NULL;
    }

    rewind(file);

    void* buffer = jbpf_calloc_mem(1, file_size);

    if (!buffer) {
        jbpf_logger(JBPF_ERROR, "Failed to allocate memory for file %s\n", path);
        fclose(file);
        *size = 0;
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != file_size) {
        jbpf_logger(JBPF_ERROR, "Failed to read file content for file %s\n", path);
        jbpf_free_mem(buffer);
        fclose(file);
        *size = 0;
        return NULL;
    }

    fclose(file);
    *size = bytes_read;
    return buffer;
}

static struct jbpf_map*
jbpf_create_map(const char* name, const struct jbpf_load_map_def* map_def, const struct jbpf_map_io_def* io_def)
{
    struct jbpf_map* map;
    struct jbpf_map* perthread_maps;
    struct jbpf_ctx_t* __jbpf_ctx = jbpf_get_ctx();

    if (_jbpf_get_num_reg_jbpf_maps() == MAX_NUM_MAPS) {
        jbpf_logger(JBPF_ERROR, "get_num_reg_jbpf_maps() returns MAX_NUM_MAPS %d\n", MAX_NUM_MAPS);
        return NULL;
    }

    map = jbpf_calloc_mem(1, sizeof(struct jbpf_map));

    if (!map)
        return NULL;

    map->type = map_def->type;
    map->key_size = map_def->key_size;
    map->value_size = map_def->value_size;
    map->max_entries = map_def->max_entries;
    strncpy(map->name, name, JBPF_MAP_NAME_LEN - 1);
    map->name[JBPF_MAP_NAME_LEN - 1] = '\0';

    switch (map_def->type) {
    case JBPF_MAP_TYPE_ARRAY:
        map->data = jbpf_bpf_array_create(map_def);
        break;
    case JBPF_MAP_TYPE_HASHMAP:
        map->data = jbpf_bpf_hashmap_create(map_def);
        break;
    case JBPF_MAP_TYPE_PER_THREAD_ARRAY:
        /* Create an array of maps (one per thread)*/
        perthread_maps = jbpf_calloc_mem(sizeof(struct jbpf_map), JBPF_MAX_NUM_REG_THREADS);
        for (int map_id = 0; map_id < JBPF_MAX_NUM_REG_THREADS; map_id++) {
            memcpy(&perthread_maps[map_id], map, sizeof(struct jbpf_map));
            perthread_maps[map_id].data = jbpf_bpf_array_create(map_def);
        }
        map->data = perthread_maps;
        break;
    case JBPF_MAP_TYPE_PER_THREAD_HASHMAP:
        /* Create an array of maps (one per thread)*/
        perthread_maps = jbpf_calloc_mem(sizeof(struct jbpf_map), JBPF_MAX_NUM_REG_THREADS);
        for (int map_id = 0; map_id < JBPF_MAX_NUM_REG_THREADS; map_id++) {
            memcpy(&perthread_maps[map_id], map, sizeof(struct jbpf_map));
            perthread_maps[map_id].data = jbpf_bpf_spsc_hashmap_create(map_def);
        }
        map->data = perthread_maps;
        break;
    case JBPF_MAP_TYPE_RINGBUF:
    case JBPF_MAP_TYPE_OUTPUT:
    case JBPF_MAP_TYPE_CONTROL_INPUT:
        if (!io_def) {
            goto map_not_created;
        }
        int direction;
        if (map_def->type == JBPF_MAP_TYPE_CONTROL_INPUT) {
            direction = JBPF_IO_CHANNEL_INPUT;
        } else {
            direction = JBPF_IO_CHANNEL_OUTPUT;
        }
        map->data = jbpf_io_create_channel(
            __jbpf_ctx->io_ctx,
            direction,
            JBPF_IO_CHANNEL_QUEUE,
            map->max_entries,
            map->value_size,
            io_def->io_desc->stream_id,
            io_def->obj_files->has_serde ? io_def->obj_files->serde_obj : NULL,
            io_def->obj_files->has_serde ? io_def->obj_files->serde_obj_size : 0);
        if (map->data) {
            goto map_created;
        } else {
            goto map_not_created;
        }
        break;
    default:
        goto map_not_created;
    }
    if (!map->data) {
        jbpf_logger(JBPF_ERROR, "The map->data is NULL.\n");
        goto map_not_created;
    }

map_created:
    __jbpf_ctx->nmap_reg++;
    return map;

map_not_created:
    jbpf_free_mem(map);
    return NULL;
}

static void
jbpf_destroy_map(struct jbpf_map* map)
{

    struct jbpf_map* perthread_maps;
    struct jbpf_ctx_t* __jbpf_ctx = jbpf_get_ctx();

    jbpf_logger(JBPF_DEBUG, "jbpf_destroy_map(%s) with type %d\n", map->name, map->type);

    /* Free the map memory */
    switch (map->type) {
    case JBPF_MAP_TYPE_ARRAY:
        jbpf_bpf_array_destroy(map);
        break;
    case JBPF_MAP_TYPE_HASHMAP:
        jbpf_bpf_hashmap_destroy(map);
        break;
    case JBPF_MAP_TYPE_PER_THREAD_ARRAY:
        perthread_maps = map->data;
        for (int map_id = 0; map_id < JBPF_MAX_NUM_REG_THREADS; map_id++) {
            jbpf_bpf_array_destroy(&perthread_maps[map_id]);
        }
        jbpf_free_mem(perthread_maps);
        break;
    case JBPF_MAP_TYPE_PER_THREAD_HASHMAP:
        perthread_maps = map->data;
        for (int map_id = 0; map_id < JBPF_MAX_NUM_REG_THREADS; map_id++) {
            jbpf_bpf_spsc_hashmap_destroy(&perthread_maps[map_id]);
        }
        jbpf_free_mem(perthread_maps);
        break;
    case JBPF_MAP_TYPE_RINGBUF:
    case JBPF_MAP_TYPE_CONTROL_INPUT:
    case JBPF_MAP_TYPE_OUTPUT:
        jbpf_io_destroy_channel(__jbpf_ctx->io_ctx, map->data);
        break;
    default:
        jbpf_logger(JBPF_CRITICAL, "Error: Unsupported map type: %d. This shouldn't happen.\n", map->type);
        exit(EXIT_FATAL);
    }

    jbpf_free_mem(map);
}

void
jbpf_release_map(struct jbpf_map* map)
{
    struct jbpf_ctx_t* __jbpf_ctx = jbpf_get_ctx();
    __jbpf_ctx->nmap_reg--;

    if (!map)
        return;

    jbpf_destroy_map(map);
}

static uint64_t
jbpf_do_map_relocation(
    void* user_context,
    const uint8_t* map_data,
    uint64_t map_data_size,
    const char* symbol_name,
    uint64_t symbol_offset,
    uint64_t symbol_size)
{
    struct jbpf_map* map = NULL;
    struct jbpf_codelet_ctx* codelet_ctx = user_context;
    ck_ht_entry_t alias_entry;
    ck_ht_hash_t alias_hash;
    struct jbpf_linked_map* linked_map = NULL;
    struct jbpf_codelet* codelet = codelet_ctx->codelet;

    struct jbpf_load_map_def map_def = *(struct jbpf_load_map_def*)(map_data + symbol_offset);

    if (symbol_size < sizeof(struct jbpf_load_map_def)) {
        jbpf_logger(JBPF_ERROR, "Invalid map size %d for map %s\n", (int)symbol_size, symbol_name);
        return 0;
    }

    map = jbpf_codelet_lookup_map(codelet, symbol_name);

    if (map) {
        jbpf_logger(JBPF_DEBUG, "Map %s is already registered!\n", symbol_name);
        return (uint64_t)map;
    }

    /* Check if this is a linked map*/
    char alias[JBPF_LINKED_MAP_ALIAS_NAME_LEN] = {0};
    snprintf(alias, JBPF_LINKED_MAP_ALIAS_NAME_LEN - 1, "%s_%s", codelet->name, symbol_name);
    alias[JBPF_LINKED_MAP_ALIAS_NAME_LEN - 1] = '\0';

    ck_ht_hash(&alias_hash, &codelet->codeletset->linked_map_entries.table, alias, JBPF_LINKED_MAP_ALIAS_NAME_LEN);
    ck_ht_entry_key_set(&alias_entry, alias, JBPF_LINKED_MAP_ALIAS_NAME_LEN);

    if (ck_ht_get_spmc(&codelet->codeletset->linked_map_entries.table, alias_hash, &alias_entry) == true) {
        linked_map = (struct jbpf_linked_map*)ck_ht_entry_value(&alias_entry);
    }

    if (!linked_map) {
        struct jbpf_map_io_def io_def = {0};

        if (map_def.type == JBPF_MAP_TYPE_RINGBUF || map_def.type == JBPF_MAP_TYPE_OUTPUT) {
            for (int channel_idx = 0; channel_idx < codelet_ctx->codelet_desc->num_out_io_channel; channel_idx++) {
                if (strncmp(
                        symbol_name, codelet_ctx->codelet_desc->out_io_channel[channel_idx].name, JBPF_MAP_NAME_LEN) ==
                    0) {
                    io_def.io_desc = &codelet_ctx->codelet_desc->out_io_channel[channel_idx];
                    io_def.obj_files = &codelet_ctx->obj_files->out_io_obj_files[channel_idx];
                    map = jbpf_create_map(symbol_name, &map_def, &io_def);
                    break;
                }
            }
        } else if (map_def.type == JBPF_MAP_TYPE_CONTROL_INPUT) {
            for (int channel_idx = 0; channel_idx < codelet_ctx->codelet_desc->num_in_io_channel; channel_idx++) {
                if (strncmp(
                        symbol_name, codelet_ctx->codelet_desc->in_io_channel[channel_idx].name, JBPF_MAP_NAME_LEN) ==
                    0) {
                    io_def.io_desc = &codelet_ctx->codelet_desc->in_io_channel[channel_idx];
                    io_def.obj_files = &codelet_ctx->obj_files->in_io_obj_files[channel_idx];
                    map = jbpf_create_map(symbol_name, &map_def, &io_def);
                    break;
                }
            }
        } else {
            map = jbpf_create_map(symbol_name, &map_def, NULL);
        }

        if (!map) {
            jbpf_logger(JBPF_ERROR, "jbpf map '%s' could not be created for codelet %s\n", symbol_name, codelet->name);
            codelet->relocation_error = true;
            return 0;
        }

        int result = jbpf_codelet_register_map(codelet, map);
        if (result == -1) {
            jbpf_logger(JBPF_ERROR, "Failed to register map %s\n", symbol_name);
            jbpf_destroy_map(map);
            codelet->relocation_error = true;
            return 0;
        } else {
            jbpf_logger(JBPF_INFO, "Registered map %s to codelet %s\n", symbol_name, codelet->name);
        }
    } else {
        /* IO maps cannot be shared */
        if (map_def.type == JBPF_MAP_TYPE_RINGBUF || map_def.type == JBPF_MAP_TYPE_CONTROL_INPUT ||
            map_def.type == JBPF_MAP_TYPE_OUTPUT) {

            jbpf_logger(JBPF_ERROR, "Error on map %s. IO maps cannot be shared\n", symbol_name);
            codelet->relocation_error = true;
            return 0;
        }
        // Check the ref_count. If it is 0, then create the map.
        // Otherwise, just reference it
        if (linked_map->ref_count == 0) {
            jbpf_logger(JBPF_DEBUG, "First reference to shared map %s, so let's create it\n", symbol_name);
            map = jbpf_create_map(symbol_name, &map_def, NULL);

            if (!map) {
                jbpf_logger(JBPF_ERROR, "jbpf shared map '%s' could not be created\n", symbol_name);
                codelet->relocation_error = true;
                return 0;
            }

            linked_map->map = map;

            int result = jbpf_codelet_register_map(codelet, map);
            if (result == -1) {
                jbpf_logger(JBPF_ERROR, "Failed to register map %s\n", symbol_name);

                jbpf_release_map(map);
                codelet->relocation_error = true;
                return 0;
            } else {
                jbpf_logger(JBPF_INFO, "Registered map %s to codelet %s\n", symbol_name, codelet->name);
            }
        } else {
            /* Next we need to make sure that the map is actually of the same type and size */
            if ((linked_map->map->type != map_def.type) || (linked_map->map->key_size != map_def.key_size) ||
                (linked_map->map->value_size != map_def.value_size) ||
                (linked_map->map->max_entries != map_def.max_entries)) {

                jbpf_logger(
                    JBPF_ERROR, "jbpf map '%s' definition is not the same as of the registered map\n", symbol_name);
                codelet->relocation_error = true;
                return 0;
            }

            jbpf_logger(JBPF_DEBUG, "Shared map %s has already been created. Referencing only\n", symbol_name);

            map = jbpf_calloc_mem(1, sizeof(struct jbpf_map));
            map->data = linked_map->map->data;
            map->key_size = linked_map->map->key_size;
            map->value_size = linked_map->map->value_size;
            map->max_entries = linked_map->map->max_entries;
            map->type = linked_map->map->type;
            strncpy(map->name, symbol_name, JBPF_MAP_NAME_LEN - 1);
            map->name[JBPF_MAP_NAME_LEN - 1] = '\0';

            int result = jbpf_codelet_register_map(codelet, map);
            if (result == -1) {
                jbpf_logger(JBPF_ERROR, "Failed to register map %s\n", symbol_name);
                codelet->relocation_error = true;
                return 0;
            } else {
                jbpf_logger(JBPF_DEBUG, "Registered copy of map %s\n", symbol_name);
            }
        }
        linked_map->ref_count++;
    }

    return (uint64_t)map;
}

static void
jbpf_destroy_codelet_maps(struct jbpf_codelet* codelet)
{
    ck_ht_iterator_t iterator;
    ck_ht_entry_t* cursor;
    ck_ht_entry_t alias_entry;
    ck_ht_hash_t alias_hash;
    struct jbpf_map* registered_maps[JBPF_MAX_CODELET_MAPS];
    int num_maps = 0;

    if (!codelet) {
        return;
    }

    ck_ht_iterator_init(&iterator);

    // Go over the maps of the codelet, destroy them and remove them
    while (ck_ht_next(&codelet->maps, &iterator, &cursor) == true) {
        struct jbpf_map* map;
        map = (struct jbpf_map*)ck_ht_entry_value(cursor);
        registered_maps[num_maps++] = map;
    }

    for (int map_idx = 0; map_idx < num_maps; map_idx++) {
        jbpf_logger(JBPF_DEBUG, "Destroying map %s\n", registered_maps[map_idx]->name);
        jbpf_codelet_deregister_map(codelet, registered_maps[map_idx]);
        // Check if this is a shared map and only destroy if this is the last reference
        /* Check if this is a linked map*/
        char alias[JBPF_LINKED_MAP_ALIAS_NAME_LEN] = {0};
        snprintf(alias, JBPF_LINKED_MAP_ALIAS_NAME_LEN - 1, "%s_%s", codelet->name, registered_maps[map_idx]->name);
        alias[JBPF_LINKED_MAP_ALIAS_NAME_LEN - 1] = '\0';

        ck_ht_hash(&alias_hash, &codelet->codeletset->linked_map_entries.table, alias, JBPF_LINKED_MAP_ALIAS_NAME_LEN);
        ck_ht_entry_key_set(&alias_entry, alias, JBPF_LINKED_MAP_ALIAS_NAME_LEN);

        if (ck_ht_remove_spmc(&codelet->codeletset->linked_map_entries.table, alias_hash, &alias_entry) == true) {
            struct jbpf_linked_map* linked_map = (struct jbpf_linked_map*)ck_ht_entry_value(&alias_entry);
            char* stored_alias = (char*)ck_ht_entry_key(&alias_entry);
            jbpf_free_mem(stored_alias);
            linked_map->ref_count--;
            // Decrease the total_refs count, since we are removing the actual link
            linked_map->total_refs--;
            if (linked_map->ref_count == 0) {
                jbpf_logger(JBPF_DEBUG, "Ref count is now 0, so releasing map %s\n", registered_maps[map_idx]->name);
                jbpf_release_map(registered_maps[map_idx]);
                if (linked_map->total_refs == 0) {
                    jbpf_free_mem(linked_map);
                }
            } else {
                jbpf_logger(
                    JBPF_DEBUG,
                    "Ref count is %d, so not deleting yet map %s\n",
                    linked_map->ref_count,
                    registered_maps[map_idx]->name);
                jbpf_free_mem(registered_maps[map_idx]);
            }
        } else {
            // No shared map. We can destroy safely
            jbpf_release_map(registered_maps[map_idx]);
        }
    }
}

static struct jbpf_codelet*
jbpf_create_codelet(
    struct jbpf_ctx_t* jbpf_ctx,
    struct jbpf_codeletset* codeletset,
    jbpf_codelet_descriptor_s* codelet_desc,
    struct jbpf_codelet_obj_files* obj_files)
{
    struct jbpf_codelet* codelet;
    struct jbpf_codelet_ctx codelet_ctx;
    char* errmsg;

    if (!jbpf_ctx || !codelet_desc || !codeletset || !obj_files) {
        return NULL;
    }

    jbpf_logger(JBPF_DEBUG, "Creating codelet %s\n", codelet_desc->codelet_name);

    codelet = jbpf_calloc_mem(1, sizeof(struct jbpf_codelet));

    if (!codelet) {
        return NULL;
    }

    codelet->codeletset = codeletset;
    codelet->e_runtime_threshold = codelet_desc->runtime_threshold;
    codelet->priority = codelet_desc->priority;
    strncpy(codelet->name, codelet_desc->codelet_name, JBPF_CODELET_NAME_LEN - 1);
    codelet->name[JBPF_CODELET_NAME_LEN - 1] = '\0';
    strncpy(codelet->hook_name, codelet_desc->hook_name, JBPF_HOOK_NAME_LEN - 1);
    codelet->hook_name[JBPF_HOOK_NAME_LEN - 1] = '\0';

    if (!ck_ht_init(
            &codelet->maps, CK_HT_MODE_BYTESTRING, ht_hash_wrapper, &ht_allocator, JBPF_MAX_CODELET_MAPS, 6602834)) {
        goto error;
    }

    codelet->vm = ubpf_create();

    if (!codelet->vm) {
        jbpf_logger(JBPF_ERROR, "Failed to create UBPF VM\n");
        goto error_map;
    }

    // Stack-based codelet context. Need to call ubpf_load_elf_ex in the same function
    codelet_ctx.codelet = codelet;
    codelet_ctx.jbpf_ctx = jbpf_ctx;
    codelet_ctx.obj_files = obj_files;
    codelet_ctx.codelet_desc = codelet_desc;

    ubpf_register_data_relocation(codelet->vm, &codelet_ctx, jbpf_do_map_relocation);

    jbpf_register_helper_functions(codelet->vm);

    int rv = ubpf_load_elf_ex(
        codelet->vm, obj_files->codelet_obj, obj_files->codelet_obj_size, JBPF_MAIN_FUNCTION_NAME, &errmsg);
    if (rv < 0 || codelet->relocation_error) {
        jbpf_logger(JBPF_ERROR, "Unable to load ELF: %s\n", errmsg);
        free(errmsg);
        goto error_vm;
    }

    codelet->codelet_fn = ubpf_compile(codelet->vm, &errmsg);
    if (codelet->codelet_fn == NULL) {
        jbpf_logger(JBPF_ERROR, "Unable to Jit compile: %s\n", errmsg);
        free(errmsg);
        goto error_vm;
    }

    return codelet;
error_vm:
    // Remove maps that were already added
    jbpf_destroy_codelet_maps(codelet);
    ubpf_destroy(codelet->vm);
error_map:
    ck_ht_destroy(&codelet->maps);
error:
    jbpf_free_mem(codelet);
    return NULL;
}

static void
jbpf_destroy_codelet(struct jbpf_codelet* codelet)
{
    if (!codelet) {
        return;
    }

    jbpf_destroy_codelet_maps(codelet);

    ubpf_destroy(codelet->vm);
    ck_ht_destroy(&codelet->maps);
    jbpf_free_mem(codelet);
}

static int
jbpf_load_codelet(struct jbpf_codelet* codelet)
{
    int ret = -1;

    if (!codelet) {
        jbpf_logger(JBPF_INFO, "jbpf_codelet_load: Make sure that the requested program exists\n");
        return ret;
    }

    struct jbpf_hook* hook;

    jbpf_logger(JBPF_INFO, "The number of hooks %d\n", jbpf_hook_list.num_hooks);
    for (int i = 0; i < jbpf_hook_list.num_hooks; i++) {
        hook = jbpf_hook_list.jbpf_hook_p[i];
        if (hook == NULL)
            continue;
        jbpf_logger(JBPF_INFO, "Hook name: %s\n", hook->name);
    }
    for (int i = 0; i < jbpf_hook_list.num_hooks; i++) {
        hook = jbpf_hook_list.jbpf_hook_p[i];
        if (hook == NULL)
            continue;
        if (strcmp(codelet->hook_name, hook->name) == 0) {
            ret =
                jbpf_register_codelet_hook(hook, codelet->codelet_fn, codelet->e_runtime_threshold, codelet->priority);
            if (ret != 0) {
                jbpf_logger(JBPF_INFO, "Failed to register codelet %s to hook %s\n", codelet->name, hook->name);
            } else {
                jbpf_logger(JBPF_INFO, "Registered codelet %s to hook %s\n", codelet->name, hook->name);
                jbpf_logger(JBPF_INFO, "Codelet %s loaded successfully: %p\n", codelet->name, (void*)hook->codelets);
                jbpf_logger(JBPF_INFO, "Codelet %s loaded successfully: %p\n", codelet->name, (void*)&hook->codelets);
                codelet->loaded = true;
            }
            break;
        }
    }

    return ret;
}

static int
jbpf_unload_codelet(struct jbpf_codelet* codelet)
{
    int ret = -1;

    if (!codelet) {
        return ret;
    }

    struct jbpf_hook* hook;

    for (int i = 0; i < jbpf_hook_list.num_hooks; i++) {
        hook = jbpf_hook_list.jbpf_hook_p[i];
        if (strncmp(codelet->hook_name, hook->name, JBPF_HOOK_NAME_LEN) == 0) {
            ret = jbpf_remove_codelet_hook(hook, codelet->codelet_fn);
            break;
        }
    }

    if (ret == 0)
        jbpf_logger(JBPF_INFO, "Codelet %s removed successfully\n", codelet->name);
    else
        jbpf_logger(JBPF_INFO, "Codelet %s is not registered for hook %s\n", codelet->name, codelet->hook_name);
    return ret;
}

static struct jbpf_codelet*
jbpf_create_codelet_from_files(
    struct jbpf_ctx_t* jbpf_ctx, struct jbpf_codeletset* codeletset, jbpf_codelet_descriptor_s* codelet_desc)
{
    struct jbpf_codelet* codelet = NULL;
    size_t size;

    struct jbpf_codelet_obj_files obj_files = {0};

    if (!codelet_desc) {
        return NULL;
    }

    if (codelet_desc->num_in_io_channel > JBPF_MAX_IO_CHANNEL ||
        codelet_desc->num_out_io_channel > JBPF_MAX_IO_CHANNEL) {
        return NULL;
    }

    obj_files.codelet_obj = _jbpf_read_binary_file(codelet_desc->codelet_path, &size);
    obj_files.codelet_obj_size = size;

    if (!obj_files.codelet_obj) {
        return NULL;
    }

    obj_files.num_in_io_channels = codelet_desc->num_in_io_channel;
    obj_files.num_out_io_channels = codelet_desc->num_out_io_channel;

    int i, j;
    for (i = 0; i < codelet_desc->num_in_io_channel; i++) {
#ifdef JBPF_EXPERIMENTAL_FEATURES
        if (codelet_desc->in_io_channel[i].has_serde) {
            obj_files.in_io_obj_files[i].has_serde = true;
            obj_files.in_io_obj_files[i].serde_obj =
                _jbpf_read_binary_file(codelet_desc->in_io_channel[i].serde.file_path, &size);
            obj_files.in_io_obj_files[i].serde_obj_size = size;
            if (!obj_files.in_io_obj_files[i].serde_obj) {
                printf("Error with in serde\n");
                goto free_in_serde;
            }
        } else {
#endif // JBPF_EXPERIMENTAL_FEATURES
            obj_files.in_io_obj_files[i].has_serde = false;
#ifdef JBPF_EXPERIMENTAL_FEATURES
        }
#endif // JBPF_EXPERIMENTAL_FEATURES
    }

    for (j = 0; j < codelet_desc->num_out_io_channel; j++) {
#ifdef JBPF_EXPERIMENTAL_FEATURES
        if (codelet_desc->out_io_channel[j].has_serde) {
            obj_files.out_io_obj_files[j].has_serde = true;
            obj_files.out_io_obj_files[j].serde_obj =
                _jbpf_read_binary_file(codelet_desc->out_io_channel[j].serde.file_path, &size);
            obj_files.out_io_obj_files[j].serde_obj_size = size;
            if (!obj_files.out_io_obj_files[j].serde_obj) {
                printf("Error with out serde\n");
                goto free_out_serde;
            }
        } else {
#endif // JBPF_EXPERIMENTAL_FEATURES
            obj_files.out_io_obj_files[j].has_serde = false;
#ifdef JBPF_EXPERIMENTAL_FEATURES
        }
#endif // JBPF_EXPERIMENTAL_FEATURES
    }

    codelet = jbpf_create_codelet(jbpf_ctx, codeletset, codelet_desc, &obj_files);

#ifdef JBPF_EXPERIMENTAL_FEATURES
free_out_serde:
    for (int k = 0; k < j; k++) {
        if (obj_files.in_io_obj_files[k].has_serde) {
            jbpf_free_mem(obj_files.in_io_obj_files[k].serde_obj);
        }
    }
free_in_serde:
    for (int k = 0; k < i; k++) {
        if (obj_files.out_io_obj_files[k].has_serde) {
            jbpf_free_mem(obj_files.out_io_obj_files[k].serde_obj);
        }
    }
#endif // JBPF_EXPERIMENTAL_FEATURES
    jbpf_free_mem(obj_files.codelet_obj);
    return codelet;
}

static bool
jbpf_add_codelet_to_codeletset(
    struct jbpf_ctx_t* jbpf_ctx, struct jbpf_codeletset* codeletset, jbpf_codelet_descriptor_s* codelet_desc)
{
    ck_ht_entry_t codelet_entry;
    ck_ht_hash_t codelet_hash;

    struct jbpf_codelet* codelet;

    codelet = jbpf_create_codelet_from_files(jbpf_ctx, codeletset, codelet_desc);

    if (!codelet) {
        return false;
    }

    jbpf_logger(JBPF_DEBUG, "Created codelet %s\n", codelet->name);

    // Add codelet to codeletset
    codelet->codeletset = codeletset;
    ck_ht_hash(&codelet_hash, &codeletset->codelets, codelet->name, JBPF_CODELET_NAME_LEN);
    ck_ht_entry_set(&codelet_entry, codelet_hash, codelet->name, JBPF_CODELET_NAME_LEN, codelet);
    ck_ht_put_spmc(&codeletset->codelets, codelet_hash, &codelet_entry);

    return true;
}

static void
jbpf_populate_linked_map_entries(struct jbpf_linked_map_entries* entries, struct jbpf_codeletset_load_req* load_req)
{
    ck_ht_init(&entries->table, CK_HT_MODE_BYTESTRING, ht_hash_wrapper, &ht_allocator, JBPF_MAX_CODELET_MAPS, 6602834);

    ck_ht_entry_t alias_entry;
    ck_ht_hash_t alias_hash;
    // Populate structure for linked maps
    for (int i = 0; i < load_req->num_codelet_descriptors; i++) {
        // If there exists a linked map, add its aliases to the table (if not already present)
        for (int j = 0; j < load_req->codelet_descriptor[i].num_linked_maps; j++) {
            // Generate aliases
            char* alias1 = jbpf_calloc_mem(1, JBPF_LINKED_MAP_ALIAS_NAME_LEN);
            char* alias2 = jbpf_calloc_mem(1, JBPF_LINKED_MAP_ALIAS_NAME_LEN);
            snprintf(
                alias1,
                JBPF_LINKED_MAP_ALIAS_NAME_LEN - 1,
                "%s_%s",
                load_req->codelet_descriptor[i].codelet_name,
                load_req->codelet_descriptor[i].linked_maps[j].map_name);
            snprintf(
                alias2,
                JBPF_LINKED_MAP_ALIAS_NAME_LEN - 1,
                "%s_%s",
                load_req->codelet_descriptor[i].linked_maps[j].linked_codelet_name,
                load_req->codelet_descriptor[i].linked_maps[j].linked_map_name);
            alias1[JBPF_LINKED_MAP_ALIAS_NAME_LEN - 1] = '\0';
            alias2[JBPF_LINKED_MAP_ALIAS_NAME_LEN - 1] = '\0';

            struct jbpf_linked_map* linked_map1 = NULL;
            struct jbpf_linked_map* linked_map2 = NULL;

            int missing_refs = 0;

            // Check if any of those aliases exists already
            ck_ht_hash(&alias_hash, &entries->table, alias1, JBPF_LINKED_MAP_ALIAS_NAME_LEN);
            ck_ht_entry_key_set(&alias_entry, alias1, JBPF_LINKED_MAP_ALIAS_NAME_LEN);

            if (ck_ht_get_spmc(&entries->table, alias_hash, &alias_entry) == true) {
                linked_map1 = (struct jbpf_linked_map*)ck_ht_entry_value(&alias_entry);
                if (!linked_map1) {
                    jbpf_logger(JBPF_DEBUG, "linked_map1 was not found\n");
                    missing_refs++;
                }
            } else {
                missing_refs++;
            }

            ck_ht_hash(&alias_hash, &entries->table, alias2, JBPF_LINKED_MAP_ALIAS_NAME_LEN);
            ck_ht_entry_key_set(&alias_entry, alias2, JBPF_LINKED_MAP_ALIAS_NAME_LEN);

            if (ck_ht_get_spmc(&entries->table, alias_hash, &alias_entry) == true) {
                linked_map2 = (struct jbpf_linked_map*)ck_ht_entry_value(&alias_entry);
                if (!linked_map2) {
                    jbpf_logger(JBPF_DEBUG, "linked_map2 was not found\n");
                    missing_refs++;
                }
            } else {
                missing_refs++;
            }

            // If none of the two aliases had a reference already, create a new entry
            if (missing_refs == 2) {
                linked_map1 = jbpf_calloc_mem(1, sizeof(struct jbpf_linked_map));
                linked_map2 = linked_map1;
            } else {
                if (linked_map1 && linked_map2) {
                    jbpf_free_mem(alias1);
                    alias1 = NULL;
                    jbpf_free_mem(alias2);
                    alias2 = NULL;
                } else {
                    if (linked_map1) {
                        // alias1 was already added, so let's free the memory
                        jbpf_free_mem(alias1);
                        alias1 = NULL;
                        linked_map2 = linked_map1;
                    } else if (linked_map2) {
                        // alias2 was already added, so let's free the memory
                        jbpf_free_mem(alias2);
                        alias2 = NULL;
                        linked_map1 = linked_map2;
                    }
                }
            }

            // Add the missing references to the total number of references.
            linked_map1->total_refs += missing_refs;

            // If alias1 did not exist, update the linked map table
            if (alias1) {
                ck_ht_hash(&alias_hash, &entries->table, alias1, JBPF_LINKED_MAP_ALIAS_NAME_LEN);
                ck_ht_entry_set(&alias_entry, alias_hash, alias1, JBPF_LINKED_MAP_ALIAS_NAME_LEN, linked_map1);
                if (ck_ht_put_spmc(&entries->table, alias_hash, &alias_entry)) {
                    jbpf_logger(JBPF_DEBUG, "Added shared map alias %s\n", alias1);
                }
            }
            // If alias2 did not exist, update the linked map table
            if (alias2) {
                ck_ht_hash(&alias_hash, &entries->table, alias2, JBPF_LINKED_MAP_ALIAS_NAME_LEN);
                ck_ht_entry_set(&alias_entry, alias_hash, alias2, JBPF_LINKED_MAP_ALIAS_NAME_LEN, linked_map2);
                if (ck_ht_put_spmc(&entries->table, alias_hash, &alias_entry)) {
                    jbpf_logger(JBPF_DEBUG, "Added shared map alias %s\n", alias2);
                }
            }
        }
    }
}

int
jbpf_codeletset_load(struct jbpf_codeletset_load_req* load_req, jbpf_codeletset_load_error_s* err)
{
    int status;
    struct jbpf_codeletset* new_codeletset;
    ck_ht_hash_t codeletset_hash;
    ck_ht_entry_t codeletset_entry;
    int outcome = JBPF_CODELET_LOAD_SUCCESS;

    pthread_mutex_lock(&lcm_mutex);

    outcome = validate_codeletset(load_req, err);
    if (outcome != JBPF_CODELET_LOAD_SUCCESS) {
        pthread_mutex_unlock(&lcm_mutex);
        return outcome;
    }

    // Check if any of the hooks does not exist and if not, exit
    for (int i = 0; i < load_req->num_codelet_descriptors; ++i) {

        if (!jbpf_hook_exists(load_req->codelet_descriptor[i].hook_name)) {
            char msg[JBPF_MAX_ERR_MSG_SIZE];
            sprintf(
                msg,
                "hook %s does not exist. Unloading codeletset %s\n",
                load_req->codelet_descriptor[i].hook_name,
                load_req->codeletset_id.name);
            jbpf_logger(JBPF_WARN, "%s\n", msg);
            if (err) {
                strcpy(err->err_msg, msg);
            }

            pthread_mutex_unlock(&lcm_mutex);
            return JBPF_CODELET_HOOK_NOT_EXIST;
        }
    }

    struct jbpf_ctx_t* __jbpf_ctx = jbpf_get_ctx();

    // Check if we can load more codeletsets
    if (ck_ht_count(&__jbpf_ctx->codeletset_registry) >= JBPF_MAX_LOADED_CODELETSETS) {
        char msg[JBPF_MAX_ERR_MSG_SIZE];
        sprintf(msg, "Max number of codeletsets exceeded\n");
        jbpf_logger(JBPF_WARN, "%s\n", msg);
        if (err) {
            strcpy(err->err_msg, msg);
        }
        pthread_mutex_unlock(&lcm_mutex);
        return JBPF_CODELET_CREATION_FAIL;
    }

    // if this point is reached, initial validation checks are successful so try loading the codeletSet

    // if the codeletset is loaded before, we just send a reply to say we have it.
    ck_ht_hash(
        &codeletset_hash, &__jbpf_ctx->codeletset_registry, load_req->codeletset_id.name, JBPF_CODELETSET_NAME_LEN);
    ck_ht_entry_key_set(&codeletset_entry, load_req->codeletset_id.name, JBPF_CODELETSET_NAME_LEN);
    if (ck_ht_get_spmc(&__jbpf_ctx->codeletset_registry, codeletset_hash, &codeletset_entry) == true) {
        char msg[JBPF_MAX_ERR_MSG_SIZE];
        sprintf(msg, "Request ignored as Codeletset (id = %s) is *ALREADY* loaded", load_req->codeletset_id.name);
        jbpf_logger(JBPF_WARN, "%s", msg);
        if (err) {
            strcpy(err->err_msg, msg);
        }
        pthread_mutex_unlock(&lcm_mutex);
        return JBPF_CODELET_LOAD_SUCCESS;
    }

    // check if the latest load will exceed the total number of allowed codelets
    if (__jbpf_ctx->total_num_codelets + load_req->num_codelet_descriptors > JBPF_MAX_LOADED_CODELETS ||
        load_req->num_codelet_descriptors > JBPF_MAX_CODELETS_IN_CODELETSET) {
        char msg[JBPF_MAX_ERR_MSG_SIZE];
        sprintf(msg, "Max number of codelets exceeded. Unloading codeletset %s\n", load_req->codeletset_id.name);
        jbpf_logger(JBPF_WARN, "%s\n", msg);
        if (err) {
            strcpy(err->err_msg, msg);
        }
        pthread_mutex_unlock(&lcm_mutex);
        return JBPF_CODELET_CREATION_FAIL;
    }

    new_codeletset = jbpf_calloc_mem(1, sizeof(struct jbpf_codeletset));

    if (!new_codeletset) {
        char msg[JBPF_MAX_ERR_MSG_SIZE];
        sprintf(msg, "Out of memory. Unloading codeletset %s\n", load_req->codeletset_id.name);
        jbpf_logger(JBPF_WARN, "%s\n", msg);
        if (err) {
            strcpy(err->err_msg, msg);
        }
        pthread_mutex_unlock(&lcm_mutex);
        return JBPF_CODELET_LOAD_FAIL;
    }

    new_codeletset->codeletset_id = load_req->codeletset_id;

    // Initialize codelet data structures
    ck_ht_init(
        &new_codeletset->codelets,
        CK_HT_MODE_BYTESTRING,
        ht_hash_wrapper,
        &ht_allocator,
        JBPF_MAX_CODELETS_IN_CODELETSET,
        6602834);

    jbpf_populate_linked_map_entries(&new_codeletset->linked_map_entries, load_req);

    // Add codelets to the codeletset
    for (int i = 0; i < load_req->num_codelet_descriptors; i++) {
        bool load_outcome;
        load_outcome = jbpf_add_codelet_to_codeletset(__jbpf_ctx, new_codeletset, &load_req->codelet_descriptor[i]);

        if (!load_outcome) {
            char msg[JBPF_MAX_ERR_MSG_SIZE];
            sprintf(
                msg,
                "Failed to create codelet %s of codeletset %s",
                load_req->codelet_descriptor[i].codelet_name,
                new_codeletset->codeletset_id.name);
            jbpf_logger(JBPF_ERROR, "%s\n", msg);
            outcome = JBPF_CODELET_CREATION_FAIL;
            if (err) {
                strcpy(err->err_msg, msg);
            }
            goto load_req_out;
        }
        __jbpf_ctx->total_num_codelets++;
    }

    // Check that all linked maps are indeed linked (i.e., ref_count == total_refs)
    // If not, this is an error, and we need to destroy the codeletset
    ck_ht_iterator_t iterator;
    ck_ht_entry_t* cursor;

    ck_ht_iterator_init(&iterator);

    while (ck_ht_next(&new_codeletset->linked_map_entries.table, &iterator, &cursor) == true) {
        struct jbpf_linked_map* linked_map;
        linked_map = (struct jbpf_linked_map*)ck_ht_entry_value(cursor);
        if (linked_map->ref_count != linked_map->total_refs) {
            char msg[JBPF_MAX_ERR_MSG_SIZE];
            sprintf(msg, "Validation error in linked map");
            jbpf_logger(JBPF_ERROR, "%s\n", msg);
            outcome = JBPF_CODELET_LOAD_FAIL;
            if (err) {
                strcpy(err->err_msg, msg);
            }
            goto load_req_out;
        }
    }

    // Load codelets to hook
    ck_ht_iterator_init(&iterator);
    while (ck_ht_next(&new_codeletset->codelets, &iterator, &cursor) == true) {
        struct jbpf_codelet* codelet;
        codelet = (struct jbpf_codelet*)ck_ht_entry_value(cursor);
        status = jbpf_load_codelet(codelet);

        if (status == 0) {
            codelet->loaded = true;
            // Load program
            jbpf_logger(
                JBPF_INFO, "----------------- %s: %s ----------------------\n", codelet->hook_name, codelet->name);
            jbpf_logger(
                JBPF_INFO,
                "hook_name = %s, priority = %d, runtime_threshold = %ld\n",
                codelet->hook_name,
                codelet->priority,
                codelet->e_runtime_threshold);
            jbpf_logger(JBPF_INFO, "Codelet created and loaded successfully: %s\n", codelet->name);
        } else {
            char msg[JBPF_MAX_ERR_MSG_SIZE];
            sprintf(msg, "Failed at jbpf_load_codelet: %s\n", codelet->name);
            jbpf_logger(JBPF_ERROR, "%s\n", msg);
            outcome = JBPF_CODELET_LOAD_FAIL;
            if (err) {
                strcpy(err->err_msg, msg);
            }
            break;
        }
    }

load_req_out:
    if (outcome) {
        // at least one codelet is loaded with errors or fails, need to undo loading for all
        ck_ht_iterator_init(&iterator);
        while (ck_ht_next(&new_codeletset->codelets, &iterator, &cursor) == true) {
            struct jbpf_codelet* codelet;
            codelet = (struct jbpf_codelet*)ck_ht_entry_value(cursor);
            if (codelet->loaded) {
                if (jbpf_unload_codelet(codelet) == -1) {
                    jbpf_logger(JBPF_WARN, "Failed to unload codelet: %s\n", codelet->name);
                }
                __jbpf_ctx->total_num_codelets--;
            }
            jbpf_destroy_codelet(codelet);
        }
        // Remove any remaining linked_map_entries alias (e.g., wrong alias that triggered validation error)
        ck_ht_iterator_init(&iterator);
        while (ck_ht_next(&new_codeletset->linked_map_entries.table, &iterator, &cursor) == true) {
            char* pending_alias = (char*)ck_ht_entry_key(cursor);
            struct jbpf_linked_map* linked_map = (struct jbpf_linked_map*)ck_ht_entry_value(cursor);
            jbpf_free_mem(pending_alias);
            linked_map->total_refs--;
            if (linked_map->total_refs == 0) {
                jbpf_free_mem(linked_map);
            }
        }
        ck_ht_destroy(&new_codeletset->linked_map_entries.table);
        ck_ht_destroy(&new_codeletset->codelets);
        jbpf_free_mem(new_codeletset);
        pthread_mutex_unlock(&lcm_mutex);
        return outcome;
    } else {
        if (err) {
            strcpy(err->err_msg, "Codeletset is loaded OK.");
        }
    }

    // All done. Add codeletset to the registry
    ck_ht_hash(
        &codeletset_hash,
        &__jbpf_ctx->codeletset_registry,
        new_codeletset->codeletset_id.name,
        JBPF_CODELETSET_NAME_LEN);
    ck_ht_entry_set(
        &codeletset_entry,
        codeletset_hash,
        new_codeletset->codeletset_id.name,
        JBPF_CODELETSET_NAME_LEN,
        new_codeletset);
    ck_ht_put_spmc(&__jbpf_ctx->codeletset_registry, codeletset_hash, &codeletset_entry);

    pthread_mutex_unlock(&lcm_mutex);
    jbpf_logger(JBPF_DEBUG, "Codeletset is loaded OK %d\n", outcome);
    return outcome;
}

int
jbpf_codeletset_unload(jbpf_codeletset_unload_req_s* unload_req, jbpf_codeletset_load_error_s* err)
{
    ck_ht_entry_t codeletset_entry;
    ck_ht_hash_t codeletset_hash;
    ck_ht_iterator_t iterator;
    ck_ht_entry_t* cursor;
    struct jbpf_codeletset* codeletset;
    struct jbpf_codelet* codelet;
    struct jbpf_codelet* codelet_list[JBPF_MAX_CODELETS_IN_CODELETSET];
    int num_codelets = 0;
    char msg[JBPF_MAX_ERR_MSG_SIZE];

    if (!unload_req) {
        return JBPF_CODELET_UNLOAD_FAIL;
    }

    pthread_mutex_lock(&lcm_mutex);

    if (validate_string_param("codeletset_id.name", unload_req->codeletset_id.name, JBPF_CODELETSET_NAME_LEN, err) !=
        1) {
        pthread_mutex_unlock(&lcm_mutex);
        return JBPF_CODELET_PARAM_INVALID;
    }

    struct jbpf_ctx_t* __jbpf_ctx = jbpf_get_ctx();

    // Find if the codeletset exists
    ck_ht_hash(
        &codeletset_hash, &__jbpf_ctx->codeletset_registry, unload_req->codeletset_id.name, JBPF_CODELETSET_NAME_LEN);
    ck_ht_entry_key_set(&codeletset_entry, unload_req->codeletset_id.name, JBPF_CODELETSET_NAME_LEN);

    if (ck_ht_get_spmc(&__jbpf_ctx->codeletset_registry, codeletset_hash, &codeletset_entry) == false) {
        sprintf(msg, "Codeletset does not exist\n");
        jbpf_logger(JBPF_WARN, "%s\n", msg);
        if (err) {
            strcpy(err->err_msg, msg);
        }
        pthread_mutex_unlock(&lcm_mutex);
        return JBPF_CODELET_UNLOAD_FAIL;
    }

    codeletset = (struct jbpf_codeletset*)ck_ht_entry_value(&codeletset_entry);

    // Go over each codelet of the codeletset and unload and destroy them
    ck_ht_iterator_init(&iterator);
    while (ck_ht_next(&codeletset->codelets, &iterator, &cursor)) {
        codelet = (struct jbpf_codelet*)ck_ht_entry_value(cursor);
        jbpf_unload_codelet(codelet);
        __jbpf_ctx->total_num_codelets--;
        codelet_list[num_codelets++] = codelet;
    }

    for (int codelet_idx = 0; codelet_idx < num_codelets; codelet_idx++) {
        jbpf_destroy_codelet(codelet_list[codelet_idx]);
    }

    // Remove codeletset from the registry
    ck_ht_remove_spmc(&__jbpf_ctx->codeletset_registry, codeletset_hash, &codeletset_entry);
    ck_ht_destroy(&codeletset->linked_map_entries.table);
    ck_ht_destroy(&codeletset->codelets);
    jbpf_free_mem(codeletset);

    pthread_mutex_unlock(&lcm_mutex);
    return JBPF_CODELET_UNLOAD_SUCCESS;
}

/* Thread for LCM interface */
static void*
jbpf_agent_thread_start(void* arg)
{

    struct jbpf_config* config = (struct jbpf_config*)arg;
    jbpf_lcm_ipc_server_config_t server_config = {0};
    struct jbpf_ctx_t* ctx;
    int ret;

    jbpf_register_thread();

    ctx = jbpf_get_ctx();

    server_config.load_cb = jbpf_codeletset_load;
    server_config.unload_cb = jbpf_codeletset_unload;

    snprintf(
        server_config.address.path,
        JBPF_LCM_IPC_ADDRESS_LEN,
        "%s/%s/%s",
        config->jbpf_run_path,
        config->jbpf_namespace,
        config->lcm_ipc_config.lcm_ipc_name);
    server_config.address.path[JBPF_LCM_IPC_ADDRESS_LEN - 1] = '\0';

    ctx->lcm_ipc_server_ctx = jbpf_lcm_ipc_server_init(&server_config);

    if (!ctx->lcm_ipc_server_ctx) {
        jbpf_logger(JBPF_ERROR, "Failed to create the LCM IPC server\n");
        goto out;
    }

    sem_post(&jagent_thread_sem);
    jbpf_logger(JBPF_INFO, "Started LCM IPC thread at %s\n", server_config.address.path);

    ret = jbpf_lcm_ipc_server_start(ctx->lcm_ipc_server_ctx);

    if (ret < 0) {
        jbpf_logger(JBPF_ERROR, "jbpf_lcm_ipc_server_start() failed\n");
    } else {
        jbpf_logger(JBPF_INFO, "Terminating LCM IPC thread\n");
    }

out:
    jbpf_cleanup_thread();
    sem_destroy(&jagent_thread_sem);

    return NULL;
}

/* Maintenance thread */
static void*
jbpf_maintenance_thread_start(void* arg)
{

    uint64_t start, end;

    (void)__sync_lock_test_and_set(&jbpf_ctx.jbpf_maintenance_run, true);

    jbpf_register_thread();

    start = jbpf_measure_start_time();

    sem_post(&jmaintenance_thread_sem);

    while (jbpf_ctx.jbpf_maintenance_run) {

        end = jbpf_measure_stop_time();

        if (jbpf_get_time_diff_ns(start, end) / 1000 > MAINTENANCE_CHECK_INTERVAL) {
            start = jbpf_measure_start_time();
            jbpf_report_perf_stats();
        }

        hook_periodic_call(MAINTENANCE_MEM_CHECK_INTERVAL);

        for (int i = 0; i < JBPF_MAX_NUM_REG_THREADS; i++) {
            ck_epoch_poll(&epoch_record_list[i]);
        }
        usleep(MAINTENANCE_MEM_CHECK_INTERVAL);
    }
    jbpf_cleanup_thread();
    sem_destroy(&jmaintenance_thread_sem);
    return NULL;
}

static void
init_maintenance_thread(struct jbpf_config* config)
{
    pthread_attr_t maintenance_attr;
    struct sched_param maintenance_param;
    int ret;

    sem_init(&jmaintenance_thread_sem, 0, 0);
    pthread_attr_init(&maintenance_attr);

    // Maintenance Thread
    if (config->thread_config.has_sched_policy_maintenance_thread) {
        pthread_attr_setinheritsched(&maintenance_attr, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&maintenance_attr, config->thread_config.maintenance_thread_sched_policy);

        /* If the scheduling policy is SCHED_OTHER, priority should be 0 */
        if (config->thread_config.maintenance_thread_sched_policy == SCHED_OTHER)
            maintenance_param.sched_priority = 0;
        else
            maintenance_param.sched_priority = config->thread_config.has_sched_priority_maintenance_thread
                                                   ? config->thread_config.maintenance_thread_sched_priority
                                                   : DEFAULT_SCHED_PRIORITY;

        pthread_attr_setschedparam(&maintenance_attr, &maintenance_param);
    }

    jbpf_logger(JBPF_INFO, "jbpf_maintenance_thread initialization finished\n");
    ret = pthread_create(
        &jbpf_maintenance_thread, &maintenance_attr, &jbpf_maintenance_thread_start, (void*)&config->io_config);
    if (ret) {
        jbpf_logger(JBPF_WARN, "Unable to create jbpf_maintenance_thread thread\n");
        exit(EXIT_FATAL);
    }

    if (config->thread_config.has_affinity_maintenance_thread) {
        _jbpf_set_thread_affinity(jbpf_maintenance_thread, config->thread_config.maintenance_thread_affinity_cores);
    }

    jbpf_logger(JBPF_INFO, "Setting the name of thread %u to jbpf_maintenance_thread\n", jbpf_maintenance_thread);
    ret = pthread_setname_np(jbpf_maintenance_thread, "jbpf_maint_th");
    if (ret != 0)
        jbpf_logger(JBPF_WARN, "WARNING: Could not set name of jbpf_maintenance_thread pthread\n");

    pthread_attr_destroy(&maintenance_attr);

    sem_wait(&jmaintenance_thread_sem);
    jbpf_logger(JBPF_DEBUG, "jbpf_maint_th ready\n");
}

void
_jbpf_process_output_data(
    struct jbpf_io_channel* io_channel, struct jbpf_io_stream_id* stream_id, void** bufs, int num_bufs, void* ctx)
{

    _jbpf_handle_output_data_cb(stream_id, bufs, num_bufs, ctx);
    for (int i = 0; i < num_bufs; i++) {
        jbpf_io_channel_release_buf(bufs[i]);
    }
}

static void
_jbpf_handle_out_bufs(void* ctx)
{

    if (!jbpf_ctx.io_ctx || jbpf_ctx.io_ctx->io_type == JBPF_IO_IPC_SECONDARY)
        return;

    jbpf_io_channel_handle_out_bufs(jbpf_ctx.io_ctx, _jbpf_process_output_data, ctx);
}

/* Thread for IO */
static void*
jbpf_io_thread_start(void* arg)
{
    struct jbpf_config* config = (struct jbpf_config*)arg;

    jbpf_logger(JBPF_INFO, "Starting jbpf io thread\n");

    jbpf_register_thread();

    if (config->io_config.io_thread_config.has_affinity_io_thread) {
        _jbpf_set_thread_affinity(pthread_self(), config->io_config.io_thread_config.io_thread_affinity_cores);
    }

    (void)__sync_lock_test_and_set(&jbpf_ctx.jbpf_io_run, true);

    init_maintenance_thread(config);

    sem_post(&jio_thread_sem);

    while (jbpf_ctx.jbpf_io_run) {

        // Send out messages
        _jbpf_handle_out_bufs(config->io_config.io_thread_config.output_handler_ctx);
        usleep(100);
        jbpf_maintenance();
    }
    jbpf_cleanup_thread();
    sem_destroy(&jio_thread_sem);

    (void)__sync_lock_test_and_set(&jbpf_ctx.jbpf_maintenance_run, false);
    pthread_join(jbpf_maintenance_thread, NULL);

    return NULL;
}

struct jbpf_ctx_t*
jbpf_get_ctx()
{
    return &jbpf_ctx;
}

void
jbpf_init_ebr(void)
{
    struct jbpf_ctx_t* ctx = jbpf_get_ctx();
    ck_epoch_init(&ctx->epoch);
    for (int i = 0; i < JBPF_MAX_NUM_REG_THREADS; i++) {
        ck_epoch_register(&ctx->epoch, &epoch_record_list[i], NULL);
    }
}

void
jbpf_cleanup_ebr(void)
{
    jbpf_call_barrier();
}

void
jbpf_generate_hook_list(void)
{

    int num_external_hooks, num_internal_hooks;
    struct jbpf_hook* iter;

    // Check is redundant when there is at least one hook present,
    // so we cast to uintptr_t to avoid the warning
    if ((uintptr_t)NULL == (uintptr_t)__start___hook_list)
        num_external_hooks = 0;
    else
        num_external_hooks = (__stop___hook_list - __start___hook_list);

    // Check is redundant when there is at least one hook present,
    // so we cast to uintptr_t to avoid the warning
    if ((uintptr_t)NULL == (uintptr_t)__start___jbpf_agent_hook_list)
        num_internal_hooks = 0;
    else
        num_internal_hooks = (__stop___jbpf_agent_hook_list - __start___jbpf_agent_hook_list);

    jbpf_hook_list.jbpf_hook_p = jbpf_calloc_mem(num_internal_hooks + num_external_hooks, sizeof(struct jbpf_hook*));
    jbpf_hook_list.num_hooks = num_internal_hooks + num_external_hooks;

    int index = 0;

    iter = __start___jbpf_agent_hook_list;

    if (iter) {
        for (; iter < __stop___jbpf_agent_hook_list; iter++)
            jbpf_hook_list.jbpf_hook_p[index++] = iter;
    }

    iter = __start___hook_list;

    if (iter) {
        for (; iter < __stop___hook_list; iter++)
            jbpf_hook_list.jbpf_hook_p[index++] = iter;
    }
}

void
jbpf_call_barrier()
{
    ck_epoch_barrier(e_record);
}

int
jbpf_memory_init(struct jbpf_config* config)
{

    int ret = 0;
    jbpf_logger(JBPF_INFO, "Initializing jbpf memory\n");

    jbpf_memory_setup(&config->mem_config);

    io_mempool = jbpf_init_data_mempool(JBPF_NUM_IO_DATA_ELEM, JBPF_IO_ELEM_SIZE);

    if (io_mempool)
        goto out;

    // Reset everyting if mempool was not created
    ret = -1;

out:
    return ret;
}

void
jbpf_memory_destroy(void)
{
    jbpf_destroy_data_mempool(io_mempool);
    jbpf_memory_teardown();
}

int
jbpf_init_io_interface(struct jbpf_config* config)
{

    jbpf_logger(JBPF_INFO, "Now initializing the IO inteface\n");

    struct jbpf_io_config io_config = {0};

    if (config->io_config.io_type == JBPF_IO_IPC_CONFIG) {
        jbpf_logger(JBPF_DEBUG, "Initializing with IPC\n");
        io_config.type = JBPF_IO_IPC_SECONDARY;
        io_config.ipc_config.mem_cfg.memory_size = config->io_config.io_ipc_config.ipc_mem_size;
        strncpy(
            io_config.ipc_config.addr.jbpf_io_ipc_name,
            config->io_config.io_ipc_config.ipc_name,
            JBPF_IO_IPC_MAX_NAMELEN - 1);
        io_config.ipc_config.addr.jbpf_io_ipc_name[JBPF_IO_IPC_MAX_NAMELEN - 1] = '\0';

    } else if (config->io_config.io_type == JBPF_IO_THREAD_CONFIG) {
        jbpf_logger(JBPF_DEBUG, "Initializing with thread\n");
        io_config.type = JBPF_IO_LOCAL_PRIMARY;
        io_config.local_config.mem_cfg.memory_size = config->io_config.io_thread_config.io_mem_size;
    }

    jbpf_logger(JBPF_DEBUG, "Initializing IO BIT\n");
    jbpf_ctx.io_ctx = jbpf_io_init(&io_config);

    if (jbpf_ctx.io_ctx)
        return 1;
    else
        return -1;
}

void
jbpf_destroy_io_interface(void)
{
    jbpf_io_stop();
}

void
jbpf_stop_io_interface(void)
{
    jbpf_destroy_io_interface();
}

static void
jbpf_init_threads_info(void)
{
    struct jbpf_threads_info* thinfo = &jbpf_ctx.threads_info;
    jbpf_init_bitmap(&thinfo->registered_threads, JBPF_MAX_NUM_REG_THREADS);
}

static void
jbpf_stop_threads_info(void)
{
    struct jbpf_threads_info* thinfo = &jbpf_ctx.threads_info;
    jbpf_destroy_bitmap(&thinfo->registered_threads);
}

static inline bool
register_jbpf_hook_thread(void)
{
    struct jbpf_ctx_t* ctx = jbpf_get_ctx();
    struct jbpf_threads_info* thinfo = &ctx->threads_info;
    __thread_id = jbpf_allocate_bit(&thinfo->registered_threads);
    if (__thread_id == -1)
        return false;
    e_record = &epoch_record_list[__thread_id];
    return true;
}

void
jbpf_register_thread()
{

    if (JBPF_UNLIKELY(!__jbpf_registered_thread)) {

        jbpf_io_register_thread();

        /* First register the thread with jbpf */
        if (register_jbpf_hook_thread()) {
            __jbpf_registered_thread = true;
            seed = time(NULL);
        } else {
            jbpf_logger(JBPF_ERROR, "Thread could not be registered. Consider increasing JBPF_MAX_NUM_REG_THREADS");
        }
    }
    //} else {
    //  jbpf_logger(JBPF_WARN, "Thread already registered with jbpf. Skipping...\n");
    //}
}

int
jbpf_init_default()
{
    jbpf_default_config = (struct jbpf_config*)malloc(sizeof(struct jbpf_config));
    jbpf_set_default_config_options(jbpf_default_config);
    return jbpf_init(jbpf_default_config);
}

static int
start_jbpf_interfaces(struct jbpf_config* config)
{

    int ret = 0;
    pthread_attr_t agent_attr, io_attr;
    struct sched_param agent_param, io_param;

    if (jbpf_ctx.io_ctx->io_type == JBPF_IO_LOCAL_PRIMARY) {

        pthread_attr_init(&io_attr);

        if (config->io_config.io_thread_config.has_sched_policy_io_thread) {

            sem_init(&jio_thread_sem, 0, 0);

            pthread_attr_setinheritsched(&io_attr, PTHREAD_EXPLICIT_SCHED);
            pthread_attr_setschedpolicy(&io_attr, config->io_config.io_thread_config.io_thread_sched_policy);

            /* If the scheduling policy is SCHED_OTHER, priority should be 0 */
            if (config->io_config.io_thread_config.io_thread_sched_policy == SCHED_OTHER)
                io_param.sched_priority = 0;
            else
                io_param.sched_priority = config->io_config.io_thread_config.has_sched_priority_io_thread
                                              ? config->io_config.io_thread_config.io_thread_sched_priority
                                              : DEFAULT_SCHED_PRIORITY;

            pthread_attr_setschedparam(&io_attr, &io_param);
        }

        ret = pthread_create(&jbpf_io_thread, &io_attr, &jbpf_io_thread_start, (void*)config);
        if (ret != 0) {
            jbpf_logger(JBPF_ERROR, "Unable to create jbpf_io thread\n");
            errno = ret;
            perror("Error creating jbpf_io thread");
            goto exit;
        }

        jbpf_logger(JBPF_INFO, "Setting the name of thread %u to jbpf_io_th\n", jbpf_io_thread);
        ret = pthread_setname_np(jbpf_io_thread, "jbpf_io_th");
        if (ret != 0) {
            jbpf_logger(JBPF_WARN, "Could not set name of jbpf_io pthread\n");
        }
        sem_wait(&jio_thread_sem);
        jbpf_logger(JBPF_DEBUG, "jbpf_io thread ready");

    } else {
        init_maintenance_thread(config);
    }

    if (config->lcm_ipc_config.has_lcm_ipc_thread) {

        sem_init(&jagent_thread_sem, 0, 0);

        pthread_attr_init(&agent_attr);

        if (config->thread_config.has_sched_policy_agent_thread) {
            pthread_attr_setinheritsched(&agent_attr, PTHREAD_EXPLICIT_SCHED);
            pthread_attr_setschedpolicy(&agent_attr, config->thread_config.agent_thread_sched_policy);

            /* If the scheduling policy is SCHED_OTHER, priority should be 0 */
            if (config->thread_config.agent_thread_sched_policy == SCHED_OTHER) {
                agent_param.sched_priority = 0;
            } else {
                agent_param.sched_priority = config->thread_config.has_sched_priority_agent_thread
                                                 ? config->thread_config.agent_thread_sched_priority
                                                 : DEFAULT_SCHED_PRIORITY;
            }
            pthread_attr_setschedparam(&agent_attr, &agent_param);
        }

        jbpf_logger(JBPF_INFO, "Agent thread initialization finished\n");
        ret = pthread_create(&jbpf_agent_thread, &agent_attr, &jbpf_agent_thread_start, (void*)config);
        if (ret) {
            jbpf_logger(JBPF_ERROR, "Unable to create jbpf_lcm_ipc thread\n");
            goto agent_exit;
        }

        jbpf_logger(JBPF_INFO, "Setting the name of thread %u to jbpf_lcm_ipc\n", jbpf_agent_thread);
        ret = pthread_setname_np(jbpf_agent_thread, "jbpf_lcm_ipc");
        if (ret != 0) {
            jbpf_logger(JBPF_WARN, "Could not set name of jbpf_lcm_ipc pthread\n");
        }
    agent_exit:
        pthread_attr_destroy(&agent_attr);

        sem_wait(&jagent_thread_sem);
        jbpf_logger(JBPF_DEBUG, "jbpf_lcm_ipc thread ready\n");
    }

exit:
    if (jbpf_ctx.io_ctx->io_type == JBPF_IO_LOCAL_PRIMARY) {
        pthread_attr_destroy(&io_attr);
    }
    return ret;
}

static void
jbpf_stop_interfaces(void)
{

    struct jbpf_ctx_t* ctx = jbpf_get_ctx();

    /* First stop the interface threads */
    if (ctx->lcm_ipc_server_ctx) {
        jbpf_lcm_ipc_server_stop(ctx->lcm_ipc_server_ctx);
        pthread_join(jbpf_agent_thread, NULL);
        jbpf_lcm_ipc_server_destroy(ctx->lcm_ipc_server_ctx);
        ctx->lcm_ipc_server_ctx = NULL;
    }

    if (jbpf_ctx.io_ctx->io_type == JBPF_IO_LOCAL_PRIMARY) {
        (void)__sync_lock_test_and_set(&jbpf_ctx.jbpf_io_run, false);
        pthread_join(jbpf_io_thread, NULL);
    } else {
        (void)__sync_lock_test_and_set(&jbpf_ctx.jbpf_maintenance_run, false);
        pthread_join(jbpf_maintenance_thread, NULL);
    }
}

static bool
jbpf_init_codeletset_registry(struct jbpf_ctx_t* jbpf_ctx)
{
    return ck_ht_init(
        &jbpf_ctx->codeletset_registry,
        CK_HT_MODE_BYTESTRING,
        ht_hash_wrapper,
        &ht_allocator,
        JBPF_MAX_LOADED_CODELETSETS,
        6602834);
}

int
jbpf_init(struct jbpf_config* config)
{
    int ret = 0;
    jbpf_current_config = config;

    jbpf_logger(
        JBPF_INFO,
        "Initializing jbpf v%s.%s.%s+%s (commit: %s-%s)\n",
        JBPF_VERSION_MAJOR,
        JBPF_VERSION_MINOR,
        JBPF_VERSION_PATCH,
        C_API_VERSION,
        JBPF_COMMIT_BRANCH,
        JBPF_COMMIT_HASH);
    jbpf_logger(JBPF_INFO, "USE_NATIVE = %s\n", USE_NATIVE);
    jbpf_logger(JBPF_INFO, "USE_JBPF_PERF_OPT = %s\n", USE_JBPF_PERF_OPT);
    jbpf_logger(JBPF_INFO, "USE_JBPF_PRINTF_HELPER = %s\n", USE_JBPF_PRINTF_HELPER);
    jbpf_logger(JBPF_INFO, "JBPF_STATIC = %s\n", JBPF_STATIC);
    jbpf_logger(JBPF_INFO, "JBPF_THREADS_LARGE = %s\n", JBPF_THREADS_LARGE);
#ifdef JBPF_EXPERIMENTAL_FEATURES
    jbpf_logger(JBPF_INFO, "JBPF_EXPERIMENTAL_FEATURES = ON\n");
#else
    jbpf_logger(JBPF_INFO, "JBPF_EXPERIMENTAL_FEATURES = OFF\n");
#endif

    struct jbpf_ctx_t* ctx = jbpf_get_ctx();

    char jbpf_path_namespace[JBPF_PATH_NAMESPACE_LEN] = {0};
    snprintf(jbpf_path_namespace, JBPF_PATH_NAMESPACE_LEN - 1, "%s/%s", config->jbpf_run_path, config->jbpf_namespace);
    jbpf_path_namespace[JBPF_PATH_NAMESPACE_LEN - 1] = '\0';

    if (_jbpf_create_run_dir(ctx, jbpf_path_namespace) < 0) {
        jbpf_logger(JBPF_ERROR, "Failed to create run path for jbpf. Exiting...\n");
        return -1;
    }

    jbpf_memory_init(config);

    jbpf_init_ebr();

    jbpf_generate_hook_list();

    if (jbpf_hook_list.num_hooks > MAX_NUM_HOOKS) {
        jbpf_logger(
            JBPF_ERROR,
            "Number of hooks exceeded the maximum number of hooks i.e. current %d hooks but maximum %d allowed. Please "
            "increase the MAX_NUM_HOOKS to %d or above.\n",
            jbpf_hook_list.num_hooks,
            MAX_NUM_HOOKS,
            jbpf_hook_list.num_hooks);
        ret = -1;
        goto hooks_number_exceeded_error;
    }

    if (jbpf_init_io_interface(config) == -1) {
        jbpf_logger(JBPF_ERROR, "Failed to initialize the IO interface\n");
        ret = -2;
        goto out_chan_error;
    }

    jbpf_init_codeletset_registry(ctx);

    /* Initialize perf structs of hooks */
    jbpf_init_perf();

    jbpf_init_threads_info();

    ret = start_jbpf_interfaces(config);

    if (ret) {
        ret = -3;
        goto init_interfaces_error;
    }

    return ret;

init_interfaces_error:
    jbpf_destroy_io_interface();
hooks_number_exceeded_error:
out_chan_error:
    jbpf_memory_destroy();
    return ret;
}

void
jbpf_maintenance()
{
    return;
}

void
jbpf_remove_hook_thread(void)
{
    struct jbpf_threads_info* thinfo = &jbpf_ctx.threads_info;
    jbpf_free_bit(&thinfo->registered_threads, __thread_id);
}

void
jbpf_cleanup_thread()
{
    if (!__jbpf_unregistered_thread) {
        jbpf_remove_hook_thread();
        __jbpf_unregistered_thread = true;
        jbpf_io_remove_thread();
    }
}

static void
clear_all_jbpf_hooks(void)
{
    struct jbpf_hook* hook;

    jbpf_logger(JBPF_INFO, "clear_all_jbpf_hooks...\n");

    for (int i = 0; i < jbpf_hook_list.num_hooks; i++) {
        hook = jbpf_hook_list.jbpf_hook_p[i];
        if (hook->codelets != NULL) {
            jbpf_free_mem(hook->codelets);
            hook->codelets = NULL;
        }
    }
}

static void
jbpf_remove_all_codeletsets(struct jbpf_ctx_t* jbpf_ctx)
{
    ck_ht_iterator_t iterator;
    ck_ht_entry_t* cursor;
    jbpf_codeletset_id_t codeletset_ids[JBPF_MAX_LOADED_CODELETSETS];
    int num_codeletsets = 0;

    ck_ht_iterator_init(&iterator);

    while (ck_ht_next(&jbpf_ctx->codeletset_registry, &iterator, &cursor)) {
        struct jbpf_codeletset* codeletset;
        codeletset = (struct jbpf_codeletset*)ck_ht_entry_value(cursor);
        codeletset_ids[num_codeletsets++] = codeletset->codeletset_id;
    }

    for (int codeletset_idx = 0; codeletset_idx < num_codeletsets; codeletset_idx++) {
        jbpf_codeletset_unload_req_s unload_req = {0};
        unload_req.codeletset_id = codeletset_ids[codeletset_idx];
        jbpf_codeletset_unload(&unload_req, NULL);
    }

    clear_all_jbpf_hooks();
}

void
jbpf_stop()
{

    struct jbpf_ctx_t* ctx = jbpf_get_ctx();
    if (jbpf_default_config) {
        free(jbpf_default_config);
    }

    /* Remove all codelets from hooks */
    jbpf_remove_all_codeletsets(ctx);

    jbpf_stop_interfaces();

    jbpf_cleanup_thread();

    jbpf_stop_threads_info();

    jbpf_stop_io_interface();

    /* Clear perf structs */
    jbpf_free_perf();

    jbpf_cleanup_ebr();

    /* Free output channel state memory */
    // free_io_channels();

    jbpf_memory_destroy();
}

void
jbpf_register_io_output_cb(jbpf_io_output_handler_cb io_output_handler_cb)
{
    _jbpf_handle_output_data_cb = io_output_handler_cb;
}

struct jbpf_io_ctx*
jbpf_get_io_ctx(void)
{
    struct jbpf_ctx_t* ctx = jbpf_get_ctx();
    if (ctx) {
        return ctx->io_ctx;
    }

    return NULL;
}

int
jbpf_send_input_msg(jbpf_io_stream_id_t* stream_id, void* data, size_t size)
{
    struct jbpf_ctx_t* __jbpf_ctx = jbpf_get_ctx();
    return jbpf_io_channel_send_msg(__jbpf_ctx->io_ctx, stream_id, data, size);
}

// test wrapper function
struct jbpf_map*
__jbpf_create_map(const char* name, const struct jbpf_load_map_def* map_def, const struct jbpf_map_io_def* io_def)
{
    return jbpf_create_map(name, map_def, io_def);
}

// test wrapper function
void
__jbpf_destroy_map(struct jbpf_map* map)
{
    jbpf_destroy_map(map);
}

// required for testing
void
__test_setup(void)
{
    jbpf_init_ebr();
    e_record = &epoch_record_list[0];
}