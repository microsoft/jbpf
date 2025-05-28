// Copyright (c) Microsoft Corporation. All rights reserved.
/*
This codelet tests the concurrency of the control input API.
Only one thread will be able to write to the control input at a time.
*/

#include <string.h>

#include "jbpf_defs.h"
#include "jbpf_helper.h"
#include "jbpf_test_def.h"

jbpf_control_input_map(input_map0, custom_api, 100);
jbpf_control_input_map(input_map1, custom_api, 100);
jbpf_control_input_map(input_map2, custom_api, 100);
jbpf_control_input_map(input_map3, custom_api, 100);
jbpf_control_input_map(input_map4, custom_api, 100);

struct jbpf_load_map_def SEC("maps") shared_map0 = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(int),
    .max_entries = 1,
};
struct jbpf_load_map_def SEC("maps") shared_map1 = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(int),
    .max_entries = 1,
};
struct jbpf_load_map_def SEC("maps") shared_map2 = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(int),
    .max_entries = 1,
};
struct jbpf_load_map_def SEC("maps") shared_map3 = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(int),
    .max_entries = 1,
};
struct jbpf_load_map_def SEC("maps") shared_map4 = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(int),
    .max_entries = 1,
};
struct jbpf_load_map_def SEC("maps") shared_map5 = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(int),
    .max_entries = 1,
};
struct jbpf_load_map_def SEC("maps") shared_map6 = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(int),
    .max_entries = 1,
};
struct jbpf_load_map_def SEC("maps") shared_map7 = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(int),
    .max_entries = 1,
};
struct jbpf_load_map_def SEC("maps") shared_map8 = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(int),
    .max_entries = 1,
};
struct jbpf_load_map_def SEC("maps") shared_map9 = {
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
    custom_api api = {0};

    if (jbpf_control_input_receive(&input_map0, &api, sizeof(custom_api)) > 0) {
        if (jbpf_map_update_elem(&shared_map0, &index, &api.command, 0) < 0) {
            return 1;
        }
    } else if (jbpf_control_input_receive(&input_map1, &api, sizeof(custom_api)) > 0) {
        if (jbpf_map_update_elem(&shared_map1, &index, &api.command, 0) < 0) {
            return 1;
        }
    } else if (jbpf_control_input_receive(&input_map2, &api, sizeof(custom_api)) > 0) {
        if (jbpf_map_update_elem(&shared_map2, &index, &api.command, 0) < 0) {
            return 1;
        }
    } else if (jbpf_control_input_receive(&input_map3, &api, sizeof(custom_api)) > 0) {
        if (jbpf_map_update_elem(&shared_map3, &index, &api.command, 0) < 0) {
            return 1;
        }
    } else if (jbpf_control_input_receive(&input_map4, &api, sizeof(custom_api)) > 0) {
        if (jbpf_map_update_elem(&shared_map4, &index, &api.command, 0) < 0) {
            return 1;
        }
    }

    if (jbpf_map_update_elem(&shared_map5, &index, &api.command, 0) < 0) {
        return 1;
    }
    if (jbpf_map_update_elem(&shared_map6, &index, &api.command, 0) < 0) {
        return 1;
    }
    if (jbpf_map_update_elem(&shared_map7, &index, &api.command, 0) < 0) {
        return 1;
    }
    if (jbpf_map_update_elem(&shared_map8, &index, &api.command, 0) < 0) {
        return 1;
    }
    if (jbpf_map_update_elem(&shared_map9, &index, &api.command, 0) < 0) {
        return 1;
    }



    return 0;
}
