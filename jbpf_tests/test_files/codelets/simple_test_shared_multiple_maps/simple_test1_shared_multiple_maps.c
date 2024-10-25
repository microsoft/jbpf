// Copyright (c) Microsoft Corporation. All rights reserved.

#include <string.h>
#include "jbpf_defs.h"
#include "jbpf_helper.h"
#include "jbpf_test_def.h"

struct jbpf_load_map_def SEC("maps") counter_a = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(uint32_t),
    .value_size = sizeof(long),
    .max_entries = 1,
};

struct jbpf_load_map_def SEC("maps") private_map = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(uint32_t),
    .value_size = sizeof(long),
    .max_entries = 20,
};

SEC("jbpf_generic")
uint64_t
jbpf_main(void* state)
{
    void* c;
    long counter, private_counter;
    uint32_t index = 0;
    uint32_t private_index = 10;

    struct jbpf_generic_ctx* ctx = (struct jbpf_generic_ctx*)state;

    struct packet* p = (struct packet*)(long)ctx->data;
    struct packet* p_end = (struct packet*)(long)ctx->data_end;

    /* Counter that checks if round is even or odd */
    c = jbpf_map_lookup_elem(&counter_a, &index);
    if (!c)
        return 1;
    counter = *(int*)c;
    counter++;

    if (p + 1 > p_end)
        return 1;

    p->counter_a = counter;

    if (jbpf_map_try_update_elem(&counter_a, &index, &counter, 0) != JBPF_MAP_SUCCESS) {
        return 1;
    }

    /* Counter that checks if round is even or odd */
    c = jbpf_map_lookup_elem(&private_map, &private_index);
    if (!c)
        return 1;
    private_counter = *(int*)c;
    private_counter++;

    p->counter_b = private_counter;

    if (jbpf_map_try_update_elem(&private_map, &private_index, &private_counter, 0) != JBPF_MAP_SUCCESS) {
        return 1;
    }

    return 0;
}
