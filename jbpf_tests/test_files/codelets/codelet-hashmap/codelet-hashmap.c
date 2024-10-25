// Copyright (c) Microsoft Corporation. All rights reserved.
/*
  This codelet is used to test the hash map read and write
  if the counter_a is negative, we increment map[key]
  otherwise, we increment counter_b by map[key]
*/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "jbpf_defs.h"
#include "jbpf_helper.h"
#include "jbpf_test_def.h"
#include <unistd.h>

struct jbpf_load_map_def SEC("maps") map1 = {
    .type = JBPF_MAP_TYPE_HASHMAP,
    .key_size = sizeof(uint64_t),
    .value_size = sizeof(int),
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
    uint64_t key = 0;
    // if counter_a is negative we increment the map[key] by counter_b
    // else we set the counter_b from reading map[key]
    if (data->counter_a < 0) {
        // get the value from the map and increment it
        int* val = (int*)jbpf_map_lookup_elem(&map1, &key);
        int new_val = 0;
        if (val) {
            new_val = *val;
        }
        new_val++;
        // then we update the map with the new value
        if (jbpf_map_try_update_elem(&map1, &key, &new_val, 0) != JBPF_MAP_SUCCESS) {
            return 1;
        }
    } else {
        int* val = (int*)jbpf_map_lookup_elem(&map1, &key);
        if (val) {
            data->counter_b += *val;
        }
    }
    return 0;
}
