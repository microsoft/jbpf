// Copyright (c) Microsoft Corporation. All rights reserved.

#include <string.h>
#include "jbpf_defs.h"
#include "jbpf_helper.h"
#include "jbpf_test_def.h"
#include "jbpf_verifier_extension_defs.h"

// define helper functions

/* return:
        EBPF_RETURN_TYPE_INTEGER
   args:
        EBPF_ARGUMENT_TYPE_DONTCARE,
        EBPF_ARGUMENT_TYPE_ANYTHING,
        EBPF_ARGUMENT_TYPE_CONST_SIZE,
        EBPF_ARGUMENT_TYPE_CONST_SIZE_OR_ZERO,
        EBPF_ARGUMENT_TYPE_PTR_TO_CTX
*/
static int (*helper_func_1)(uint64_t, uint64_t, uint64_t, uint64_t, void*) = (void*)CUSTOM_HELPER_START_ID;

/* return:
        EBPF_RETURN_TYPE_PTR_TO_MAP_VALUE_OR_NULL
   args:
        EBPF_ARGUMENT_TYPE_PTR_TO_MAP
        EBPF_ARGUMENT_TYPE_PTR_TO_MAP_OF_PROGRAMS
        EBPF_ARGUMENT_TYPE_PTR_TO_MAP_KEY
        EBPF_ARGUMENT_TYPE_PTR_TO_MAP_VALUE
*/
static void* (*helper_func_2)(void*, void*, void*, void*) = (void*)CUSTOM_HELPER_START_ID + 1;

/* return:
        EBPF_RETURN_TYPE_INTEGER_OR_NO_RETURN_IF_SUCCEED
   args:
        EBPF_ARGUMENT_TYPE_ANYTHING
        EBPF_ARGUMENT_TYPE_PTR_TO_READABLE_MEM
        EBPF_ARGUMENT_TYPE_CONST_SIZE_OR_ZERO
        EBPF_ARGUMENT_TYPE_PTR_TO_READABLE_MEM_OR_NULL
        EBPF_ARGUMENT_TYPE_CONST_SIZE_OR_ZERO
*/
static int (*helper_func_3)(uint64_t, void*, uint64_t, void*, uint64_t) = (void*)CUSTOM_HELPER_START_ID + 2;
static void (*helper_func_4)(uint64_t, void*, uint64_t, void*, uint64_t) = (void*)CUSTOM_HELPER_START_ID + 3;

/* return:
        EBPF_RETURN_TYPE_INTEGER_OR_NO_RETURN_IF_SUCCEED
   args:
        EBPF_ARGUMENT_TYPE_ANYTHING,
        EBPF_ARGUMENT_TYPE_PTR_TO_WRITABLE_MEM,
        EBPF_ARGUMENT_TYPE_CONST_SIZE_OR_ZERO
*/
static int (*helper_func_5)(uint64_t, void*, uint64_t) = (void*)CUSTOM_HELPER_START_ID + 4;

jbpf_ringbuf_map(output_map, int, 128);

struct jbpf_load_map_def SEC("maps") shared_map = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(int),
    .max_entries = 10,
};

// New program type "jbpf_my_prog_type"
SEC("jbpf_my_prog_type")
uint64_t
jbpf_main(void* state)
{
    // New type of context defined in "jbpf_verifier_extension_defs.h"
    struct my_new_jbpf_ctx* ctx = (struct my_new_jbpf_ctx*)state;

    struct packet* p = (struct packet*)(long)ctx->data;
    struct packet* p_end = (struct packet*)(long)ctx->data_end;
    int index = 0;

    uint32_t static_field1 = ctx->static_field1;
    uint16_t static_field2 = ctx->static_field2;
    uint8_t static_field3 = ctx->static_field3;

    char buffer[32] = {0};        // A fixed-size buffer
    char readable_data[64] = {0}; // Example readable buffer
    char optional_data[64] = {0}; // Example optional buffer
    char writable_data[64] = {0}; // Example writable buffer

    if (p + 1 > p_end)
        return 1;

    int* shared_map_value = jbpf_map_lookup_elem(&shared_map, &index);
    if (!shared_map_value) {
        return 1;
    }

    // call helper function 1
    int result = helper_func_1(1, 2, (uint64_t)buffer, sizeof(buffer), ctx);
    if (jbpf_ringbuf_output(&output_map, &result, sizeof(int)) < 0) {
        return 1;
    }

    // call helper function 2
    void* ptr_to_map_of_programs = &shared_map;
    int key = p->counter_a;
    void* result2 = helper_func_2(&shared_map, ptr_to_map_of_programs, &index, shared_map_value);
    if (result2 != NULL) {
        key += static_field1;
        if (jbpf_ringbuf_output(&output_map, &key, sizeof(int)) < 0) {
            return 1;
        }
    }

    // call helper function 3
    int result3 = helper_func_3(1, readable_data, sizeof(readable_data), optional_data, sizeof(optional_data));
    if (result3 == 3333) {
        result3 += static_field2;
        if (jbpf_ringbuf_output(&output_map, &result3, sizeof(int)) < 0) {
            return 1;
        }
    }
    // call helper function 4 with optional data as NULL
    helper_func_4(0, readable_data, sizeof(readable_data), NULL, 0);

    // call helper function 5
    int result5 = helper_func_5(1, writable_data, sizeof(writable_data));
    if (result5 == 5555) {
        result5 += static_field3;
        if (jbpf_ringbuf_output(&output_map, &result5, sizeof(int)) < 0) {
            return 1;
        }
    }

    return 0;
}
