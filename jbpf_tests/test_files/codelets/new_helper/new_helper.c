// Copyright (c) Microsoft Corporation. All rights reserved.

#include <string.h>
#include "jbpf_defs.h"
#include "jbpf_helper.h"
#include "jbpf_verifier_extension_defs.h"

// Helper function extension
static int (*unknown_helper_function)(void*, int, int, int) = (void*)CUSTOM_HELPER_START_ID;

struct packet
{
    int counter_a;
    int counter_b;
};

// New program type "jbpf_my_prog_type"
SEC("jbpf_generic")
uint64_t
jbpf_main(void* state)
{

    // New type of context defined in "jbpf_verifier_extension_defs.h"
    struct jbpf_generic_ctx* ctx = (struct jbpf_generic_ctx*)state;

    int res = 0;

    struct packet* p = (struct packet*)(long)ctx->data;
    struct packet* p_end = (struct packet*)(long)ctx->data_end;

    if (p + 1 > p_end)
        return 0;

    res = unknown_helper_function(ctx, 100, 200, 300);
    jbpf_time_get_ns();

    p->counter_a = res;

    return 0;
}
