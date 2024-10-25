// Copyright (c) Microsoft Corporation. All rights reserved.
/*
This codelet tests the concurrency of the control input API.
Only one thread will be able to write to the control input at a time.
*/

#include <string.h>

#include "jbpf_defs.h"
#include "jbpf_helper.h"
#include "jbpf_test_def.h"

jbpf_output_map(output_map, int, 100);

struct jbpf_load_map_def SEC("maps") shared_map_output0 = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(int),
    .max_entries = 1,
};

struct jbpf_load_map_def SEC("maps") shared_map_output1 = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(int),
    .max_entries = 1,
};

SEC("jbpf_generic")
uint64_t
jbpf_main(void* state)
{
    uint32_t index = 0;
    int* output;
    int* outvar;
    struct jbpf_generic_ctx* ctx = (struct jbpf_generic_ctx*)state;
    struct packet* p = (struct packet*)(long)ctx->data;

    output = jbpf_map_lookup_elem(&shared_map_output0, &index);
    if (!output) {
        return 1;
    }

    *output = *output + p->counter_a;

    outvar = jbpf_get_output_buf(&output_map);
    if (!outvar) {
        return 1;
    }
    *outvar = *output;
    if (jbpf_send_output(&output_map) < 0) {
        return 1;
    }

    output = jbpf_map_lookup_elem(&shared_map_output1, &index);
    if (!output) {
        return 1;
    }

    *output = *output + p->counter_a;

    outvar = jbpf_get_output_buf(&output_map);
    if (!outvar) {
        return 1;
    }
    *outvar = *output;
    if (jbpf_send_output(&output_map) < 0) {
        return 1;
    }

    return 0;
}
