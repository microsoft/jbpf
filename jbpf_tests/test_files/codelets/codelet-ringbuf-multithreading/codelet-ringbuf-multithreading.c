// Copyright (c) Microsoft Corporation. All rights reserved.
/*
The codelet calls jbpf_ringbuf_output. And if the call is successful, it increments the counter_a in the packet struct.
Otherwise, it increments the counter_b.
*/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "jbpf_defs.h"
#include "jbpf_helper.h"
#include "jbpf_test_def.h"
#include <unistd.h>

jbpf_ringbuf_map(map1, req_resp, 10000)

    SEC("jbpf_generic") uint64_t jbpf_main(void* state)
{
    struct jbpf_generic_ctx* ctx = (struct jbpf_generic_ctx*)state;
    struct packet* data = (struct packet*)ctx->data;
    struct packet* data_end = (struct packet*)ctx->data_end;
    if (data + 1 > data_end) {
        return 1;
    }
    req_resp out_data = {0};
    out_data.which_req_or_resp = 2;
    // if success counter_a ++ otherwise counter_b ++;
    if (0 == jbpf_ringbuf_output(&map1, &out_data, sizeof(req_resp))) {
        data->counter_a++;
    } else {
        data->counter_b++;
    }
    return 0;
}
