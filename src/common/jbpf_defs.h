// Copyright (c) Microsoft Corporation. All rights reserved.
#ifndef JBPF_DEFS_H
#define JBPF_DEFS_H

#include <stdint.h>

#define SEC(NAME) __attribute__((section(NAME), used))

#define MAX_PROTO_NAME 32
#define MAX_PROTO_MSG_NAME 32
#define MAX_PROTO_HASH 128

/* jbpf MAP flags */
#define JBPF_MAP_CLEAR_FLAG (1 << 0)

/**
 * @brief declare a jbpf output map
 * @param name name of the map
 * @param struct_name name of the struct
 * @note The struct will be sent out of the map and jbpf needs it to figure out the size of the elements of the
 * ringbuffer.
 * @param num_elems max number of elements in the map
 * @ingroup jbpf_agent
 */
#ifndef jbpf_output_map
#define jbpf_output_map(name, struct_name, num_elems) \
    struct jbpf_load_map_def SEC("maps") name = {     \
        .type = JBPF_MAP_TYPE_OUTPUT,                 \
        .max_entries = num_elems,                     \
        .value_size = sizeof(struct_name),            \
    };
#endif

/**
 * @brief declare a jbpf ringbuf map
 * @param name name of the map
 * @param struct_name name of the struct
 * @note The struct will be sent out of the map and jbpf needs it to figure out the size of the elements of the
 * ringbuffer.
 * @param num_elems max number of elements in the map
 * @ingroup jbpf_agent
 */
#ifndef jbpf_ringbuf_map
#define jbpf_ringbuf_map(name, struct_name, num_elems) \
    struct jbpf_load_map_def SEC("maps") name = {      \
        .type = JBPF_MAP_TYPE_RINGBUF,                 \
        .max_entries = num_elems,                      \
        .value_size = sizeof(struct_name),             \
    };
#endif

/**
 * @brief declare a jbpf control input map
 * @param name name of the map
 * @param struct_name name of the struct
 * @note The struct will be sent out of the map and jbpf needs it to figure out the size of the elements of the buffer.
 * @param num_elems max number of elements in the map
 * @note .type: type of the map, e.g. JBPF_MAP_TYPE_CONTROL_INPUT *
 * @ingroup jbpf_agent
 */
#ifndef jbpf_control_input_map
#define jbpf_control_input_map(name, struct_name, num_elems) \
    struct jbpf_load_map_def SEC("maps") name = {            \
        .type = JBPF_MAP_TYPE_CONTROL_INPUT,                 \
        .max_entries = num_elems,                            \
        .value_size = sizeof(struct_name),                   \
    };
#endif

/**
 * @brief jbpf program type
 * @ingroup core
 * @note JBPF_PROG_TYPE_UNSPEC: Unspecified program type
 * @note JBPF_PROG_TYPE_GENERIC: Generic program type
 * @note JBPF_PROG_TYPE_STATS: Performance stats program type
 * @note JBPF_NUM_PROG_TYPES_MAX: Placeholder for the maximum number of program types
 */
enum jbpf_prog_type
{
    JBPF_PROG_TYPE_UNSPEC = 0,
    JBPF_PROG_TYPE_GENERIC,
    JBPF_PROG_TYPE_STATS,
    JBPF_NUM_PROG_TYPES_MAX,
};

/**
 * @brief jbpf helper function types
 * @note JBPF_UNSPEC: Unspecified
 * @note JBPF_MAP_LOOKUP: Lookup a value in a map
 * @note JBPF_MAP_LOOKUP_RESET: Lookup a value in a map and reset the value
 * @note JBPF_MAP_UPDATE: Update a value in a map
 * @note JBPF_MAP_DELETE: Delete a value in a map
 * @note JBPF_MAP_CLEAR: Clear a map
 * @note JBPF_MAP_DUMP: Dumpy the map
 * @note JBPF_TIME_GET_NS: Get time in ns
 * @note JBPF_GET_SYS_TIME: Get system time
 * @note JBPF_GET_SYS_TIME_DIFF_NS: Get system time difference in ns
 * @note JBPF_HASH: hash function
 * @note JBPF_PRINTF: log printing
 * @note JBPF_RINGBUF_OUTPUT: Ringbuf output
 * @note JBPF_MARK_RUNTIME_INIT: Mark runtime init
 * @note JBPF_RUNTIME_LIMIT_EXCEEDED: Runtime limit exceeded
 * @note JBPF_RAND: get a random number
 * @note JBPF_CONTROL_INPUT_RECEIVE: Receive data from control input
 * @note JBPF_GET_OUTPUT_BUF: Get output buffer pointer
 * @note JBPF_SEND_OUTPUT: Send output
 * @note JBPF_NUM_HELPERS_MAX: Placeholder for the maximum number of helper functions
 * @ingroup core
 */
enum jbpf_helper_type
{
    JBPF_UNSPEC = 0,
    JBPF_MAP_LOOKUP,
    JBPF_MAP_LOOKUP_RESET,
    JBPF_MAP_UPDATE,
    JBPF_MAP_DELETE,
    JBPF_MAP_CLEAR,
    JBPF_MAP_DUMP,
    JBPF_TIME_GET_NS,
    JBPF_GET_SYS_TIME,
    JBPF_GET_SYS_TIME_DIFF_NS,
    JBPF_HASH,
    JBPF_PRINTF,
    JBPF_RINGBUF_OUTPUT,
    JBPF_MARK_RUNTIME_INIT,
    JBPF_RUNTIME_LIMIT_EXCEEDED,
    JBPF_RAND,
    JBPF_CONTROL_INPUT_RECEIVE,
    JBPF_GET_OUTPUT_BUF,
    JBPF_SEND_OUTPUT,
    JBPF_NUM_HELPERS_MAX, // Use this as the starting value for any additional helper functions
};

/**
 * @brief jbpf map type
 * @ingroup core
 */
enum ubpf_map_type
{
    JBPF_MAP_TYPE_UNSPEC = 0,
    JBPF_MAP_TYPE_ARRAY = 1,
    JBPF_MAP_TYPE_HASHMAP = 2,
    JBPF_MAP_TYPE_RINGBUF = 3,
    JBPF_MAP_TYPE_CONTROL_INPUT = 4,
    JBPF_MAP_TYPE_PER_THREAD_ARRAY = 5,
    JBPF_MAP_TYPE_PER_THREAD_HASHMAP = 6,
    JBPF_MAP_TYPE_OUTPUT = 7,
    JBPF_MAP_TYPE_MAX,
};

/**
 * @brief jbpf map definition as they appear in an ELF file, so field width matters.
 * @ingroup core
 */
struct jbpf_load_map_def
{
    uint32_t type;
    uint32_t key_size;
    uint32_t value_size;
    uint32_t max_entries;
    uint32_t map_flags;
    uint32_t inner_map_idx;
    uint32_t numa_node;
    uint32_t nb_hash_functions;
};

/* Declarations of program contexts go here */

/**
 * @brief Generic context that can be used in any hook
 * @ingroup core
 * @note This is a generic context that can be used in any hook
 * @param data Pointer to the beginning of any struct
 * @param data_end Pointer to the end of any struct
 * @param meta_data Used for the program to store metadata
 * @param ctx_id Can be used to store a unique context id to identiry the caller
 */
struct jbpf_generic_ctx
{
    uint64_t data;      /* Pointer to the beginning of any struct */
    uint64_t data_end;  /* Pointer to the end of any struct */
    uint64_t meta_data; /* Used for the program to store metadata */
    uint32_t ctx_id;    /* Can be used to store a unique context id to identiry the caller */
};

/**
 * @brief Context declaration for jbpf perf data
 * @ingroup core
 * @note This is a context for jbpf perf data
 * @param data Pointer to the beginning of struct containing jbpf perf stats
 * @param data_end Pointer to the end of struct containing jbpf perf stats
 * @param meta_data Used for the program to store metadata
 * @param meas_period Period of measurements in ms
 */
struct jbpf_stats_ctx
{
    uint64_t data;        /* Pointer to the beginning of struct containing jbpf perf stats */
    uint64_t data_end;    /* Pointer to the end of struct containing jbpf perf stats */
    uint64_t meta_data;   /* Used for the program to store metadata */
    uint32_t meas_period; /* Period of measurements in ms */
};

#endif
