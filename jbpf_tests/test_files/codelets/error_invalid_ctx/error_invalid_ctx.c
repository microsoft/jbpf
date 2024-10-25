// Copyright (c) Microsoft Corporation. All rights reserved.

#include <string.h>
#include "jbpf_defs.h"
#include "jbpf_helper.h"

struct packet
{
    int counter_a;
    int counter_b;
};

// Context that does not match the type of program ("jbpf_generic")
struct jbpf_unknown_ctx
{
    uint64_t data;
    uint64_t data_end;
    uint64_t meta_data;
    uint32_t ctx_id;
    uint32_t unknown_field1;
    uint32_t unknown_field2;
};

SEC("jbpf_generic")
uint64_t
jbpf_main(void* state)
{

    struct jbpf_unknown_ctx* ctx = (struct jbpf_unknown_ctx*)state;

    struct packet* p = (struct packet*)(long)ctx->data;
    struct packet* p_end = (struct packet*)(long)ctx->data_end;

    if (p + 1 > p_end)
        return 1;

    p->counter_a = ctx->unknown_field2;

    return 0;
}
