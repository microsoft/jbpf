// Copyright (c) Microsoft Corporation. All rights reserved.

#include <string.h>
#include "jbpf_defs.h"
#include "jbpf_helper.h"
#include "jbpf_test_def.h"
#include "jbpf_verifier_extension_defs.h"

// define helper functions

/* return:
        EBPF_RETURN_TYPE_INTEGER
   args:
        EBPF_ARGUMENT_TYPE_PTR_TO_MAP,
        EBPF_ARGUMENT_TYPE_ANYTHING
*/
static int (*helper1)(void*, uint64_t) = (void*)CUSTOM_HELPER_START_ID;

struct jbpf_load_map_def SEC("maps") my_custom_map1 = {
    .type = CUSTOM_MAP_START_ID,
    .key_size = sizeof(int),
    .value_size = sizeof(int),
    .max_entries = 1,
};

struct jbpf_load_map_def SEC("maps") my_custom_map2 = {
    .type = CUSTOM_MAP_START_ID + 1,
    .key_size = sizeof(int),
    .value_size = sizeof(int),
    .max_entries = 16,
};

struct jbpf_load_map_def SEC("maps") my_custom_map3 = {
    .type = CUSTOM_MAP_START_ID + 2,
    .key_size = sizeof(int),
    .value_size = sizeof(struct jbpf_load_map_def),
    .max_entries = 1,
};

struct jbpf_load_map_def SEC("maps") my_custom_map4 = {
    .type = CUSTOM_MAP_START_ID + 3,
    .key_size = sizeof(int),
    .value_size = sizeof(struct jbpf_load_map_def),
    .max_entries = 16,
};

SEC("jbpf_generic")
uint64_t
jbpf_main(void* state)
{
    int res = 0;
    int index0 = 0;
    int index5 = 5;
    int* value;
    struct jbpf_generic_ctx* ctx = (struct jbpf_generic_ctx*)state;
    struct packet* p = (struct packet*)(long)ctx->data;
    struct packet* p_end = (struct packet*)(long)ctx->data_end;

    if (p + 1 > p_end)
        return 1;

    res = helper1(&my_custom_map1, 1);
    p->counter_a += res;

    value = jbpf_map_lookup_elem(&my_custom_map2, &index5);
    if (!value) {
        return 1;
    }
    *value = *value + p->counter_a;

    res = helper1(&my_custom_map2, (uint64_t)*value);

    res = helper1(&my_custom_map3, 1);

    struct jbpf_load_map_def* map = jbpf_map_lookup_elem(&my_custom_map4, &index5);
    if (!map) {
        return 1;
    }
    value = jbpf_map_lookup_elem(map, &index0);
    if (!value) {
        return 1;
    }
    *value = *value + p->counter_a;
    p->counter_a = *value;
    res = helper1(map, (uint64_t)*value);
    p->counter_a = res;

    return 0;
}
