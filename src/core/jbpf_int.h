// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef JBPF_INT_H_
#define JBPF_INT_H_

#include "ck_ht.h"
#include "ubpf.h"

#include "jbpf_device_defs.h"
#include "jbpf_hook_defs.h"
#include "jbpf_defs.h"
#include "jbpf_lcm_api.h"
#include "jbpf_bitmap.h"
#include "jbpf_io.h"
#include "jbpf_common_types.h"

#define JBPF_MAIN_FUNCTION_NAME "jbpf_main"
#define JBPF_LINKED_MAP_ALIAS_NAME_LEN (1024U)
#define JBPF_MESSAGE_LEN (256U)
#define JBPF_MAX_CODELET_MAPS (64U)

extern ck_epoch_record_t epoch_record_list[JBPF_MAX_NUM_REG_THREADS];

struct jbpf_map
{
    enum ubpf_map_type type;
    unsigned int key_size;
    unsigned int value_size;
    unsigned int max_entries;
    jbpf_map_name_t name;
    void* data;
};

struct jbpf_codelet_io_serde_obj_files
{
    bool has_serde;
    char* serde_obj;
    size_t serde_obj_size;
};

struct jbpf_codelet_obj_files
{
    char* codelet_obj;
    size_t codelet_obj_size;
    int num_in_io_channels;
    struct jbpf_codelet_io_serde_obj_files in_io_obj_files[JBPF_MAX_IO_CHANNEL];
    int num_out_io_channels;
    struct jbpf_codelet_io_serde_obj_files out_io_obj_files[JBPF_MAX_IO_CHANNEL];
};

struct jbpf_map_io_def
{
    jbpf_io_channel_desc_s* io_desc;
    struct jbpf_codelet_io_serde_obj_files* obj_files;
};

struct jbpf_linked_map
{
    struct jbpf_map* map;
    int ref_count;
    int total_refs;
};

struct jbpf_linked_map_entries
{
    // Hash table that holds the linked maps with all their aliases
    ck_ht_t table;
};

struct jbpf_codeletset
{
    jbpf_codeletset_id_t codeletset_id;
    // Hash table that holds all the codelets of the codeletset
    ck_ht_t codelets;
    struct jbpf_linked_map_entries linked_map_entries;
};

struct jbpf_codelet_ctx
{
    struct jbpf_codelet* codelet;
    jbpf_codelet_descriptor_s* codelet_desc;
    struct jbpf_codelet_obj_files* obj_files;
    struct jbpf_ctx_t* jbpf_ctx;
};

struct jbpf_codelet
{
    struct ubpf_vm* vm;
    jbpf_jit_fn codelet_fn;
    ck_ht_t maps;
    unsigned long long int loaded_at;
    jbpf_codelet_name_t name; // codelet name
    jbpf_hook_name_t hook_name;
    jbpf_codelet_priority_t priority;
    jbpf_runtime_threshold_t e_runtime_threshold;
    struct jbpf_codeletset* codeletset;
    bool loaded;
    bool relocation_error;
};

struct jbpf_hook_list
{
    int num_hooks;
    struct jbpf_hook** jbpf_hook_p;
};

extern struct jbpf_hook_list jbpf_hook_list;

struct jbpf_threads_info
{
    struct jbpf_bitmap registered_threads;
};

struct jbpf_ctx_t
{
    /* Run path*/
    char jbpf_run_path[JBPF_RUN_PATH_LEN];

    jbpf_lcm_ipc_server_ctx_t lcm_ipc_server_ctx;

    /* Registered jbpf codeletsets */
    ck_ht_t codeletset_registry;
    /* Total number of registered maps */
    int nmap_reg;
    int total_num_codelets;
    /* Metadata about registered output channels */
    struct jbpf_io_ctx* io_ctx;

    bool jbpf_io_run;
    bool jbpf_maintenance_run;

    struct jbpf_threads_info threads_info;

    ck_epoch_t epoch;
};

struct jbpf_ctx_t*
jbpf_get_ctx(void);

void
jbpf_call_barrier(void);

int
validate_string_param(char* param_name, char* param, uint32_t param_maxlen, jbpf_codeletset_load_error_s* err);

// test wrapper functions
// we don't want to expose these functions to the public API
// so we define them as static, but we still want to test them
// so we make them visible to the test code by defining them as weak
// and then defining them in the test code
struct jbpf_map*
__jbpf_create_map(const char* name, const struct jbpf_load_map_def* map_def, const struct jbpf_map_io_def* io_def);
void
__jbpf_destroy_map(struct jbpf_map* map);
void*
__jbpf_map_lookup_elem(const struct jbpf_map* map, const void* key);
int
__jbpf_map_update_elem(struct jbpf_map* map, const void* key, void* item, uint64_t flags);
int
__jbpf_map_delete_elem(struct jbpf_map* map, const void* key);
void
__test_setup(void);

#endif // JBPF_INT_H_