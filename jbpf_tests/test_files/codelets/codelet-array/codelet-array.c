// Copyright (c) Microsoft Corporation. All rights reserved.
/*
    This codelet defines a simple array map with 10 entries.
    It then updates the map with 10 elements and checks if the elements are updated correctly.
    It then performs some tests when the given index is out of bounds.
*/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "jbpf_defs.h"
#include "jbpf_helper.h"
#include "jbpf_test_def.h"
#include <unistd.h>

struct jbpf_load_map_def SEC("maps") map1 = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(uint64_t),
    .value_size = sizeof(uint32_t),
    .max_entries = 10,
};

SEC("jbpf_generic")
uint64_t
jbpf_main(void* state)
{
    struct jbpf_generic_ctx* ctx = (struct jbpf_generic_ctx*)state;
    struct test_packet* data = (struct test_packet*)ctx->data;
    struct test_packet* data_end = (struct test_packet*)ctx->data_end;
    if (data + 1 > data_end) {
        return 1;
    }
    // clear the test result
    data->test_passed = 0;
    // test jbpf_map_update_elem when the key is valid
    for (int i = 0; i < 10; ++i) {
        uint64_t key = i;
        int ret = jbpf_map_update_elem(&map1, &key, &i, 0);
        if (ret != JBPF_MAP_SUCCESS) {
            return 1;
        }
    }
    // test jbpf_map_lookup_elem when the key is valid
    for (int i = 0; i < 10; ++i) {
        uint64_t key = i;
        int* val = (int*)jbpf_map_lookup_elem(&map1, &key);
        if (val == NULL) {
            return 1;
        }
        if (*val != i) {
            return 1;
        }
    }
    // test jbpf_map_update_elem when the key is out of bounds: positive index
    for (int i = 999; i < 1009; ++i) {
        uint64_t key = i;
        int ret = jbpf_map_try_update_elem(&map1, &key, &i, 0);
        if (ret != JBPF_MAP_ERROR) {
            return 1;
        }
    }
    // test jbpf_map_update_elem when the key is out of bounds: negative index
    for (int i = -999; i < -989; ++i) {
        uint64_t key = i;
        int ret = jbpf_map_try_update_elem(&map1, &key, &i, 0);
        if (ret != JBPF_MAP_ERROR) {
            return 1;
        }
    }
    // test jbpf_map_lookup_elem when the key is out of bounds: positive index
    for (int i = 1234; i < 1244; ++i) {
        uint64_t key = i;
        int* val = (int*)jbpf_map_lookup_elem(&map1, &key);
        if (val != NULL) {
            return 1;
        }
    }
    // test jbpf_map_lookup_elem when the key is out of bounds: negative index
    for (int i = -1234; i < -1224; ++i) {
        uint64_t key = i;
        int* val = (int*)jbpf_map_lookup_elem(&map1, &key);
        if (val != NULL) {
            return 1;
        }
    }
    // indicate success
    data->test_passed = 1;
    return 0;
}
