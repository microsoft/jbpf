// Copyright (c) Microsoft Corporation. All rights reserved.
/*
This codelet is used to test if we can create a hashmap of size 8388607 (using a binary search to find the maximum
number that won't throw OOM) and if we can update it. For a hash map, the actual memory allocated by the ck_ht
implementation is significantly larger than just the storage required for max_entries. Specifically, it allocates
roughly 8× the space needed to store the values. For example, if each value is a 4-byte integer and you're storing 65536
× 1024 entries, the raw value storage would require about 256 MB. However, the ck_ht hash map
internally allocates around 8× that amount, resulting in a total allocation of roughly 2 GB. This excessive allocation
can lead to memory allocation failures (OOM) during ck_ht_init.
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
    .key_size = sizeof(int),
    .value_size = 8,
    .max_entries = 8388607, // raw storage size is 8388607*8 bytes = 64MB
    // but the ck_ht implementation allocates approx 8x that amount, so total allocation is about 512MB.
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
    uint64_t val = 0;
    if (jbpf_map_try_update_elem(&map1, &key, &val, 0) == JBPF_MAP_SUCCESS) {
        val = 1;
    }
    data->counter_a = val;
    return val & 0xFF;
}
