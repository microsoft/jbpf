// Copyright (c) Microsoft Corporation. All rights reserved.

#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <printf.h>

#include "jbpf_memory.h"
#include "jbpf.h"
#include "jbpf_int.h"
#include "jbpf_defs.h"
#include "jbpf_perf.h"
#include "jbpf_utils.h"
#include "jbpf_logging.h"
#include "jbpf_utils.h"
#include "jbpf_bpf_hashmap.h"
#include "jbpf_bpf_spsc_hashmap.h"
#include "jbpf_bpf_array.h"
#include "jbpf_helper_impl.h"
#include "jbpf_common_types.h"

#include "jbpf_io_channel.h"

#define JBPF_DEFAULT_HELPER_FUNCS                                                                                    \
    {                                                                                                                \
        {"", JBPF_UNSPEC, NULL}, {"jbpf_map_lookup", JBPF_MAP_LOOKUP, (jbpf_helper_func_t)jbpf_map_lookup_elem},     \
            {"jbpf_map_lookup_reset", JBPF_MAP_LOOKUP_RESET, (jbpf_helper_func_t)jbpf_map_lookup_reset_elem},        \
            {"jbpf_map_update", JBPF_MAP_UPDATE, (jbpf_helper_func_t)jbpf_map_update_elem},                          \
            {"jbpf_map_delete", JBPF_MAP_DELETE, (jbpf_helper_func_t)jbpf_map_delete_elem},                          \
            {"jbpf_map_clear", JBPF_MAP_CLEAR, (jbpf_helper_func_t)jbpf_map_clear},                                  \
            {"jbpf_map_dump", JBPF_MAP_DUMP, (jbpf_helper_func_t)jbpf_map_dump},                                     \
            {"jbpf_time_get_ns", JBPF_TIME_GET_NS, (jbpf_helper_func_t)jbpf_time_get_ns},                            \
            {"jbpf_get_sys_time", JBPF_GET_SYS_TIME, (jbpf_helper_func_t)jbpf_get_sys_time},                         \
            {"jbpf_get_sys_time_diff_ns", JBPF_GET_SYS_TIME_DIFF_NS, (jbpf_helper_func_t)jbpf_get_sys_time_diff_ns}, \
            {"jbpf_hash", JBPF_HASH, (jbpf_helper_func_t)jbpf_hash},                                                 \
            {"jbpf_printf", JBPF_PRINTF, (jbpf_helper_func_t)jbpf_printf},                                           \
            {"jbpf_ringbuf_output", JBPF_RINGBUF_OUTPUT, (jbpf_helper_func_t)jbpf_ringbuf_output},                   \
            {"jbpf_mark_runtime_init", JBPF_MARK_RUNTIME_INIT, (jbpf_helper_func_t)jbpf_mark_runtime_init},          \
            {"jbpf_runtime_limit_exceeded",                                                                          \
             JBPF_RUNTIME_LIMIT_EXCEEDED,                                                                            \
             (jbpf_helper_func_t)jbpf_runtime_limit_exceeded},                                                       \
            {"jbpf_rand", JBPF_RAND, (jbpf_helper_func_t)jbpf_rand},                                                 \
            {"jbpf_control_input_receive",                                                                           \
             JBPF_CONTROL_INPUT_RECEIVE,                                                                             \
             (jbpf_helper_func_t)jbpf_control_input_receive},                                                        \
            {"jbpf_get_output_buf", JBPF_GET_OUTPUT_BUF, (jbpf_helper_func_t)jbpf_get_output_buf},                   \
            {"jbpf_send_output", JBPF_SEND_OUTPUT, (jbpf_helper_func_t)jbpf_send_output},                            \
    }

struct __control_input_ctx
{
    void* output_buf;
    uint32_t max_size;
    uint32_t size_read;
};

#ifdef __cplusplus
thread_local uint64_t codelet_mark_runtime;
thread_local jbpf_runtime_threshold_t e_runtime_threshold;
thread_local unsigned int seed;
#else
_Thread_local uint64_t codelet_mark_runtime;
_Thread_local jbpf_runtime_threshold_t e_runtime_threshold;
_Thread_local unsigned int seed;
#endif

static void*
jbpf_map_lookup_elem(const struct jbpf_map* map, const void* key)
{
    int index;
    struct jbpf_map* perthread_map;

    if (JBPF_UNLIKELY(!map)) {
        return NULL;
    }
    if (JBPF_UNLIKELY(!key)) {
        return NULL;
    }
    if ((strlen(map->name) == 0)) {
        return NULL;
    }

    switch (map->type) {
    case JBPF_MAP_TYPE_ARRAY:
        return jbpf_bpf_array_lookup_elem(map, key);
    case JBPF_MAP_TYPE_HASHMAP:
        return jbpf_bpf_hashmap_lookup_elem(map, key);
    case JBPF_MAP_TYPE_PER_THREAD_ARRAY:
        perthread_map = map->data;
        index = get_jbpf_hook_thread_id();
        if (index == -1)
            return NULL;
        else
            return jbpf_bpf_array_lookup_elem(&perthread_map[index], key);
    case JBPF_MAP_TYPE_PER_THREAD_HASHMAP:
        perthread_map = map->data;
        index = get_jbpf_hook_thread_id();
        if (index == -1)
            return NULL;
        else
            return jbpf_bpf_spsc_hashmap_lookup_elem(&perthread_map[index], key);
    default:
        return NULL;
    }
}

static void*
jbpf_map_lookup_reset_elem(const struct jbpf_map* map, const void* key)
{
    int index;
    struct jbpf_map* perthread_map;

    if (JBPF_UNLIKELY(!map)) {
        return NULL;
    }
    if (JBPF_UNLIKELY(!key)) {
        return NULL;
    }
    if ((strlen(map->name) == 0)) {
        return NULL;
    }
    switch (map->type) {
    case JBPF_MAP_TYPE_ARRAY:
        return jbpf_bpf_array_reset_elem(map, key);
    case JBPF_MAP_TYPE_HASHMAP:
        return jbpf_bpf_hashmap_reset_elem(map, key);
    case JBPF_MAP_TYPE_PER_THREAD_ARRAY:
        perthread_map = map->data;
        index = get_jbpf_hook_thread_id();
        if (index == -1)
            return NULL;
        else
            return jbpf_bpf_array_reset_elem(&perthread_map[index], key);
    case JBPF_MAP_TYPE_PER_THREAD_HASHMAP:
        perthread_map = map->data;
        index = get_jbpf_hook_thread_id();
        if (index == -1)
            return NULL;
        else
            return jbpf_bpf_spsc_hashmap_reset_elem(&perthread_map[index], key);
    default:
        return NULL;
    }
}

static int
jbpf_map_update_elem(struct jbpf_map* map, const void* key, void* item, uint64_t flags)
{
    int index;
    struct jbpf_map* perthread_map;

    if (JBPF_UNLIKELY(!map)) {
        return -1;
    }
    if (JBPF_UNLIKELY(!key)) {
        return -3;
    }
    if (JBPF_UNLIKELY(!item)) {
        return -4;
    }
    switch (map->type) {
    case JBPF_MAP_TYPE_ARRAY:
        return jbpf_bpf_array_update_elem(map, key, item, flags);
    case JBPF_MAP_TYPE_HASHMAP:
        return jbpf_bpf_hashmap_update_elem(map, key, item, flags);
    case JBPF_MAP_TYPE_PER_THREAD_ARRAY:
        perthread_map = map->data;
        index = get_jbpf_hook_thread_id();
        if (index == -1)
            return -5;
        else
            return jbpf_bpf_array_update_elem(&perthread_map[index], key, item, flags);
    case JBPF_MAP_TYPE_PER_THREAD_HASHMAP:
        perthread_map = map->data;
        index = get_jbpf_hook_thread_id();
        if (index == -1)
            return -5;
        else
            return jbpf_bpf_spsc_hashmap_update_elem(&perthread_map[index], key, item, flags);
    default:
        return -2;
    }
}

static int
jbpf_map_clear(struct jbpf_map* map)
{
    int index;
    struct jbpf_map* perthread_map;

    if (JBPF_UNLIKELY(!map)) {
        return -1;
    }

    switch (map->type) {
    case JBPF_MAP_TYPE_ARRAY:
        return jbpf_bpf_array_clear(map);
    case JBPF_MAP_TYPE_HASHMAP:
        return jbpf_bpf_hashmap_clear(map);
    case JBPF_MAP_TYPE_PER_THREAD_ARRAY:
        perthread_map = map->data;
        index = get_jbpf_hook_thread_id();
        if (index == -1)
            return -3;
        else
            return jbpf_bpf_array_clear(&perthread_map[index]);
    case JBPF_MAP_TYPE_PER_THREAD_HASHMAP:
        perthread_map = map->data;
        index = get_jbpf_hook_thread_id();
        if (index == -1)
            return -3;
        else
            return jbpf_bpf_spsc_hashmap_clear(&perthread_map[index]);
    default:
        return -2;
    }
}

static int
jbpf_map_dump(struct jbpf_map* map, void* data, uint32_t max_size, uint64_t flags)
{
    int index;
    struct jbpf_map* perthread_map;

    if (JBPF_UNLIKELY(!map)) {
        return -1;
    }

    switch (map->type) {
    case JBPF_MAP_TYPE_ARRAY:
        return -2;
    case JBPF_MAP_TYPE_HASHMAP:
        return jbpf_bpf_hashmap_dump(map, data, max_size, flags);
    case JBPF_MAP_TYPE_PER_THREAD_ARRAY:
        return -2;
    case JBPF_MAP_TYPE_PER_THREAD_HASHMAP:
        perthread_map = map->data;
        index = get_jbpf_hook_thread_id();
        if (index == -1)
            return -3;
        else
            return jbpf_bpf_spsc_hashmap_dump(&perthread_map[index], data, max_size, flags);
    default:
        return -2;
    }
}

static int
jbpf_map_delete_elem(struct jbpf_map* map, const void* key)
{
    int index;
    struct jbpf_map* perthread_map;

    if (JBPF_UNLIKELY(!map)) {
        return -1;
    }
    if (JBPF_UNLIKELY(!key)) {
        return -3;
    }

    switch (map->type) {
    case JBPF_MAP_TYPE_ARRAY:
        return -2;
    case JBPF_MAP_TYPE_HASHMAP:
        return jbpf_bpf_hashmap_delete_elem(map, key);
    case JBPF_MAP_TYPE_PER_THREAD_ARRAY:
        return -2;
    case JBPF_MAP_TYPE_PER_THREAD_HASHMAP:
        perthread_map = map->data;
        index = get_jbpf_hook_thread_id();
        if (index == -1)
            return -4;
        else
            return jbpf_bpf_spsc_hashmap_delete_elem(&perthread_map[index], key);
    default:
        return -2;
    }
}

static int
jbpf_printf(const char* fmt, uint32_t len, ...)
{

#ifdef JBPF_PRINTF_HELPER_ENABLED
    va_list ap, full_list;
    char* s;
    int nwanted, res;

    /* Each formating character requires at least 2 characters */
    int max_arglen = JBPF_MAX_PRINTF_STR_LEN / 2;
    int argtypes[JBPF_MAX_PRINTF_STR_LEN];

    nwanted = parse_printf_format(fmt, max_arglen, argtypes);

    if (nwanted > 3) {
        jbpf_logger(JBPF_ERROR, "Cannot have more than 3 arguments in jbpf_printf helper\n");
        return -1;
    }

    if (nwanted == 0) {
        jbpf_logger(JBPF_DEBUG, fmt, ap);
        return 0;
    } else {
        va_start(ap, len);
        va_copy(full_list, ap);

        for (int i = 0; i < nwanted; i++) {
            switch (argtypes[i] & ~PA_FLAG_MASK) {
            case PA_STRING:
                s = va_arg(ap, char*);
                res = check_unsafe_string(s);
                if (res <= 0) {
                    jbpf_logger(JBPF_ERROR, "jbpf_printf - Argument %d is not a valid string\n", i + 1);
                    return -1;
                }
            }
        }
        va_end(ap);
        jbpf_va_logger(JBPF_DEBUG, fmt, full_list);
        va_end(full_list);
    }
#endif /* JBPF_PRINTF_HELPER_ENABLED */
    return 0;
}

static uint64_t
jbpf_time_get_ns(void)
{
    struct timespec curr_time = {0, 0};
    uint64_t curr_time_ns = 0;
    clock_gettime(CLOCK_REALTIME, &curr_time);
    curr_time_ns = curr_time.tv_nsec + curr_time.tv_sec * 1.0e9;
    return curr_time_ns;
}

static uint64_t
jbpf_get_sys_time(bool is_start_time)
{
    if (is_start_time)
        return jbpf_measure_start_time();
    else
        return jbpf_measure_stop_time();
}

static uint64_t
jbpf_get_sys_time_diff_ns(uint64_t start_time, uint64_t end_time)
{
    return jbpf_get_time_diff_ns(start_time, end_time);
}

static uint32_t
jbpf_hash(void* item, uint64_t size)
{
    return hashlittle(item, (uint32_t)size, 0);
}

static void*
jbpf_get_output_buf(struct jbpf_map* map)
{

    jbpf_io_channel_t* io_channel;

    if (JBPF_UNLIKELY(!map)) {
        return NULL;
    }

    if (map->type != JBPF_MAP_TYPE_OUTPUT)
        return NULL;

    io_channel = map->data;

    return jbpf_io_channel_reserve_buf(io_channel);
}

static int
jbpf_send_output(struct jbpf_map* map)
{
    jbpf_io_channel_t* io_channel;
    int res = 0;
    if (JBPF_UNLIKELY(!map)) {
        return -1;
    }

    if (map->type != JBPF_MAP_TYPE_OUTPUT)
        return -1;

    io_channel = map->data;

    res = jbpf_io_channel_submit_buf(io_channel);

    return res;
}

static int
jbpf_ringbuf_output(struct jbpf_map* map, void* data, uint64_t size)
{

    jbpf_io_channel_t* io_channel;
    int res;

    if (JBPF_UNLIKELY(!map)) {
        return -1;
    }

    if (map->type != JBPF_MAP_TYPE_RINGBUF)
        return -1;

    io_channel = (jbpf_io_channel_t*)map->data;

    res = jbpf_io_channel_send_data(io_channel, data, size);

    if (res < 0) {
        return -1;
    } else {
        return 0;
    }
}

int
jbpf_control_input_receive(struct jbpf_map* map, void* buf, uint32_t size)
{

    jbpf_io_channel_t* io_channel;
    void* data;
    int res;

    if (JBPF_UNLIKELY(!map)) {
        return -1;
    }

    if (map->type != JBPF_MAP_TYPE_CONTROL_INPUT)
        return -1;

    if (size < map->value_size)
        return -1;

    io_channel = map->data;

    res = jbpf_io_channel_recv_data(io_channel, &data, 1);

    if (res <= 0)
        return res;

    memcpy(buf, data, map->value_size);

    jbpf_io_channel_release_buf(data);

    return 1;
}

static void
jbpf_mark_runtime_init(void)
{
    codelet_mark_runtime = jbpf_measure_start_time();
}

static int
jbpf_runtime_limit_exceeded(void)
{
    uint64_t elapsed_time = jbpf_get_time_diff_ns(codelet_mark_runtime, jbpf_measure_stop_time());
    if (elapsed_time > e_runtime_threshold) {
        // printf("Time exceeded for the codelet %ld\n", elapsed_time);
        return 1;
    }
    // printf("Time not exceeded for the codelet at this point: %ld\n", elapsed_time);
    return 0;
}

static int
jbpf_rand(void)
{
    return rand_r(&seed);
}

// Default jbpf helper functions
static jbpf_helper_func_def_t helper_funcs[MAX_HELPER_FUNC] = JBPF_DEFAULT_HELPER_FUNCS;
static const jbpf_helper_func_def_t default_helper_funcs[MAX_HELPER_FUNC] = JBPF_DEFAULT_HELPER_FUNCS;

void
jbpf_register_helper_functions(struct ubpf_vm* vm)
{
    for (int i = 0; i < MAX_HELPER_FUNC; i++) {
        if (helper_funcs[i].reloc_id != 0) {
            jbpf_logger(JBPF_DEBUG, "Registering helper function %s\n", helper_funcs[i].name);
            ubpf_register(
                vm,
                helper_funcs[i].reloc_id,
                helper_funcs[i].name,
                as_external_function_t(helper_funcs[i].function_cb));
        }
    }
}

int
jbpf_register_helper_function(jbpf_helper_func_def_t func)
{
    bool replaced = false;

    if (!func.function_cb || func.reloc_id <= 0 || func.reloc_id >= MAX_HELPER_FUNC) {
        return -1;
    }

    if (helper_funcs[func.reloc_id].reloc_id != 0) {
        replaced = true;
    }

    helper_funcs[func.reloc_id].reloc_id = func.reloc_id;
    helper_funcs[func.reloc_id].function_cb = func.function_cb;
    strncpy(helper_funcs[func.reloc_id].name, func.name, MAX_HELPER_FUN_NAME_LEN);
    helper_funcs[func.reloc_id].name[MAX_HELPER_FUN_NAME_LEN - 1] = '\0';
    return replaced ? 1 : 0;
}

int
jbpf_deregister_helper_function(int reloc_id)
{
    if (reloc_id <= 0 || reloc_id >= MAX_HELPER_FUNC)
        return -2;

    if (helper_funcs[reloc_id].reloc_id == 0) {
        return -1;
    }

    memset(&helper_funcs[reloc_id], 0, sizeof(jbpf_helper_func_def_t));
    return 0;
}

void
jbpf_reset_helper_functions()
{
    memcpy(helper_funcs, default_helper_funcs, sizeof(helper_funcs));
}

// wrapper function
void*
__jbpf_map_lookup_elem(const struct jbpf_map* map, const void* key)
{
    return jbpf_map_lookup_elem(map, key);
}

// wrapper function
int
__jbpf_map_update_elem(struct jbpf_map* map, const void* key, void* item, uint64_t flags)
{
    return jbpf_map_update_elem(map, key, item, flags);
}

// wrapper function
int
__jbpf_rand(void)
{
    return jbpf_rand();
}

// wrapper function for testing
void
__set_seed(unsigned int x)
{
    seed = x;
}

// wrapper function
uint64_t
__jbpf_get_sys_time_diff_ns(uint64_t start_time, uint64_t end_time)
{
    return jbpf_get_sys_time_diff_ns(start_time, end_time);
}

// wrapper function
uint32_t
__jbpf_hash(void* item, uint64_t size)
{
    return jbpf_hash(item, size);
}

// wrapper function
int
__jbpf_map_delete_elem(struct jbpf_map* map, const void* key)
{
    return jbpf_map_delete_elem(map, key);
}

void
__jbpf_set_e_runtime_threshold(uint64_t threshold)
{
    e_runtime_threshold = threshold;
}

// wrapper function
void
__jbpf_mark_runtime_init(void)
{
    jbpf_mark_runtime_init();
}

// wrapper function
int
__jbpf_runtime_limit_exceeded(void)
{
    return jbpf_runtime_limit_exceeded();
}

jbpf_helper_func_def_t*
__get_custom_helper_functions(void)
{
    return helper_funcs;
}

const jbpf_helper_func_def_t*
__get_default_helper_functions(void)
{
    return default_helper_funcs;
}
