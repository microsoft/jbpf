// Copyright (c) Microsoft Corporation. All rights reserved.
/*
This codelet tests the concurrency of the control input API.
Only one thread will be able to write to the control input at a time.
*/

#include <string.h>

#include "jbpf_defs.h"
#include "jbpf_helper.h"
#include "jbpf_test_def.h"

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

    output = jbpf_map_lookup_elem(&shared_counter_map, &index);
    if (!output) {
        return 1;
    }

    *output = *output + 1;
    // jbpf_printf_debug("Output: %d\n", *output);

    if (jbpf_ringbuf_output(&output_map, output, sizeof(int)) < 0) {
        return 1;
    }

    return 0;
}
