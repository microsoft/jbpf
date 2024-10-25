// Copyright (c) Microsoft Corporation. All rights reserved.
/*
This codelet is used to test the per-thread hashmap and array.
This codelet creates 1 hashmap and 1 array per thread.
It will increment the counter in the hashmap and array and save the value to the packet.
*/

#include <string.h>

#include "jbpf_defs.h"
#include "jbpf_helper.h"
#include "jbpf_test_def.h"

struct jbpf_load_map_def SEC("maps") per_thread_counter = {
    .type = JBPF_MAP_TYPE_PER_THREAD_ARRAY,
    .key_size = sizeof(uint32_t),
    .value_size = sizeof(uint32_t),
    .max_entries = 1,
};

struct jbpf_load_map_def SEC("maps") per_thread_counter_hashmap = {
    .type = JBPF_MAP_TYPE_PER_THREAD_HASHMAP,
    .key_size = sizeof(uint32_t),
    .value_size = sizeof(uint32_t),
    .max_entries = 1,
};

SEC("jbpf_generic")
uint64_t
jbpf_main(void* state)
{
    struct jbpf_generic_ctx* ctx = (struct jbpf_generic_ctx*)state;
    struct packet* data = (struct packet*)ctx->data;
    struct packet* data_end = (struct packet*)ctx->data_end;
    if (data + 1 > data_end) {
        return 1;
    }

    uint32_t key = 0;
    uint32_t value;
    uint32_t temp_value1, temp_value2; // Temporary stack variable for updates

    // array
    uint32_t* c1 = jbpf_map_lookup_elem(&per_thread_counter, &key);
    if (!c1) {
        return 2;
    }

    temp_value1 = *c1 + 1;
    jbpf_map_try_update_elem(&per_thread_counter, &key, &temp_value1, 0);

    data->counter_a = temp_value1; // For the array

    // hash map
    uint32_t* c2 = jbpf_map_lookup_elem(&per_thread_counter_hashmap, &key);
    if (!c2) {
        value = 0;
        jbpf_map_try_update_elem(&per_thread_counter_hashmap, &key, &value, 0);
        c2 = jbpf_map_lookup_elem(&per_thread_counter_hashmap, &key);
        if (!c2) {
            return 3; // Prevent null pointer dereference
        }
    }

    temp_value2 = *c2 + 1;
    jbpf_map_try_update_elem(&per_thread_counter_hashmap, &key, &temp_value2, 0);

    data->counter_b = temp_value2; // For the hashmap

    return 0;
}
