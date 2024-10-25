// Copyright (c) Microsoft Corporation. All rights reserved.

#include <string.h>
#include "jbpf_defs.h"
#include "jbpf_test_def.h"
#include "jbpf_helper.h"

// Send a single int value out
jbpf_output_map(output_map0, int, 10);
jbpf_output_map(output_map1, int, 10);
jbpf_output_map(output_map2, int, 10);
jbpf_output_map(output_map3, int, 10);
jbpf_output_map(output_map4, int, 10);

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
struct jbpf_load_map_def SEC("maps") shared_map_output2 = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(int),
    .max_entries = 1,
};
struct jbpf_load_map_def SEC("maps") shared_map_output3 = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(int),
    .max_entries = 1,
};
struct jbpf_load_map_def SEC("maps") shared_map_output4 = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(int),
    .max_entries = 1,
};

SEC("jbpf_ran_fapi")
uint64_t
jbpf_main(void* state)
{
    uint32_t index = 0;
    int *output0, *output1, *output2, *output3, *output4;
    int *outvar0, *outvar1, *outvar2, *outvar3, *outvar4;

    output0 = jbpf_map_lookup_elem(&shared_map_output0, &index);
    if (!output0) {
        return 1;
    }
    outvar0 = jbpf_get_output_buf(&output_map0);
    if (!outvar0) {
        return 1;
    }
    *outvar0 = *output0;
    if (jbpf_send_output(&output_map0) < 0) {
        return 1;
    }

    output1 = jbpf_map_lookup_elem(&shared_map_output1, &index);
    if (!output1) {
        return 2;
    }
    outvar1 = jbpf_get_output_buf(&output_map1);
    if (!outvar1) {
        return 2;
    }
    *outvar1 = *output1;
    if (jbpf_send_output(&output_map1) < 0) {
        return 2;
    }

    output2 = jbpf_map_lookup_elem(&shared_map_output2, &index);
    if (!output2) {
        return 3;
    }
    outvar2 = jbpf_get_output_buf(&output_map2);
    if (!outvar2) {
        return 3;
    }
    *outvar2 = *output2;
    if (jbpf_send_output(&output_map2) < 0) {
        return 3;
    }

    output3 = jbpf_map_lookup_elem(&shared_map_output3, &index);
    if (!output3) {
        return 4;
    }
    outvar3 = jbpf_get_output_buf(&output_map3);
    if (!outvar3) {
        return 4;
    }
    *outvar3 = *output3;
    if (jbpf_send_output(&output_map3) < 0) {
        return 4;
    }

    output4 = jbpf_map_lookup_elem(&shared_map_output4, &index);
    if (!output4) {
        return 5;
    }
    outvar4 = jbpf_get_output_buf(&output_map4);
    if (!outvar4) {
        return 5;
    }
    *outvar4 = *output4;
    if (jbpf_send_output(&output_map4) < 0) {
        return 5;
    }

    return 0;
}
