// Copyright (c) Microsoft Corporation. All rights reserved.

#include <string.h>
#include "jbpf_defs.h"
#include "jbpf_helper.h"

struct packet
{
    int counter_a;
    int counter_b;
};

// Use relocation value 100, which does not exist
static void (*unknown_helper_function)(int, int, int) = (void*)MAX_HELPER_FUNC;

SEC("jbpf_generic")
uint64_t
jbpf_main(void* state)
{

    struct jbpf_generic_ctx* ctx = (struct jbpf_generic_ctx*)state;

    struct packet* p = (struct packet*)(long)ctx->data;
    struct packet* p_end = (struct packet*)(long)ctx->data_end;

    if (p + 1 > p_end)
        return 1;

    p->counter_a++;

    unknown_helper_function(p->counter_a, p->counter_b, 100);

    return 0;
}
