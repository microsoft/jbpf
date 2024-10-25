// Copyright (c) Microsoft Corporation. All rights reserved.

#include <string.h>
#include "jbpf_defs.h"
#include "jbpf_test_def.h"
#include "jbpf_helper.h"

// Send a single int value out
jbpf_ringbuf_map(output_map, int, 10);

SEC("jbpf_generic")
uint64_t
jbpf_main(void* state)
{

    struct jbpf_generic_ctx* ctx = (struct jbpf_generic_ctx*)state;

    struct packet* p = (struct packet*)(long)ctx->data;
    struct packet* p_end = (struct packet*)(long)ctx->data_end;

    if (p + 1 > p_end)
        return 1;

    if (jbpf_ringbuf_output(&output_map, &p->counter_a, sizeof(int)) < 0) {
        return 1;
    }

    return 0;
}
