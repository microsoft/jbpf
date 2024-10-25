// Copyright (c) Microsoft Corporation. All rights reserved.

#include <string.h>
#include "jbpf_defs.h"
#include "jbpf_test_def.h"
#include "jbpf_helper.h"

// Send a single int value out
jbpf_ringbuf_map(output_map, int, 10);

struct jbpf_load_map_def SEC("maps") shared_counter_map = {
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
    int i = 0;

    struct jbpf_generic_ctx* ctx = (struct jbpf_generic_ctx*)state;

    struct packet* p = (struct packet*)(long)ctx->data;
    struct packet* p_end = (struct packet*)(long)ctx->data_end;

    if (p + 1 > p_end)
        return 1;

    output = jbpf_map_lookup_elem(&shared_counter_map, &index);
    if (!output) {
        return 1;
    }
    *output = *output + 1;

    // jbpf_printf_debug("Output: %d\n", *output);
    for (i = 0; i < 100; i++) {
        // dont check return code here. We are flooding the output
        if (jbpf_ringbuf_output(&output_map, output, sizeof(int)) < 0) {
            // failure
            p->counter_a++;
        } else {
            // success
            p->counter_b++;
        }
    }

    return 0;
}
