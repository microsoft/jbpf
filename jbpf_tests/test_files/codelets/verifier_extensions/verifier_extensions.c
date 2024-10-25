// Copyright (c) Microsoft Corporation. All rights reserved.

#include <string.h>
#include "jbpf_defs.h"
#include "jbpf_helper.h"
#include "jbpf_verifier_extension_defs.h"

// Helper function extension
static int (*unknown_helper_function)(void*, int, int, int) = (void*)CUSTOM_HELPER_START_ID;

// New type of map extension defined in "jbpf_verifier_extension_defs.h"
struct jbpf_load_map_def SEC("maps") my_custom_map = {
    .type = CUSTOM_MAP_START_ID,
    .key_size = sizeof(uint32_t),
    .value_size = sizeof(long),
    .max_entries = 1,
};

struct packet
{
    int counter_a;
};

// New program type "jbpf_my_prog_type"
SEC("jbpf_my_prog_type")
uint64_t
jbpf_main(void* state)
{

    // New type of context defined in "jbpf_verifier_extension_defs.h"
    struct my_new_jbpf_ctx* ctx = (struct my_new_jbpf_ctx*)state;

    int res = 0;
    int val_a = ctx->static_field1;
    int val_b = ctx->static_field2;
    int val_c = ctx->static_field3;

    struct packet* p = (struct packet*)(long)ctx->data;
    struct packet* p_end = (struct packet*)(long)ctx->data_end;

    if (p + 1 > p_end)
        return 1;

    res = unknown_helper_function(&my_custom_map, 100, 200, 300);

    // Set the computed value to counter_a, so that it can be checked by the testing program
    p->counter_a = val_a + val_b + val_c + res;

    return 0;
}
