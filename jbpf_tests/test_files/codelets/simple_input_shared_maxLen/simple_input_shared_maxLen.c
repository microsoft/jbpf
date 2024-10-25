// Copyright (c) Microsoft Corporation. All rights reserved.
/*
This codelet tests the map with maximum name length.
*/

#include <string.h>

#include "jbpf_defs.h"
#include "jbpf_helper.h"
#include "jbpf_test_def.h"

jbpf_control_input_map(
    input_map_0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef01234,
    custom_api,
    100);

struct jbpf_load_map_def SEC("maps")
    shared_map_0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123 =
        {
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
    custom_api api = {0};

    if (jbpf_control_input_receive(
            &input_map_0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef01234,
            &api,
            sizeof(custom_api)) > 0) {
        if (jbpf_map_update_elem(
                &shared_map_0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123,
                &index,
                &api.command,
                0) < 0) {
            return 1;
        }
    }
    return 0;
}
