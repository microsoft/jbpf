// Copyright (c) Microsoft Corporation. All rights reserved.
/*
This codelet tests the concurrency of jbpf_send_output - only 1 is allowed, and others will fail
*/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "jbpf_defs.h"
#include "jbpf_helper.h"
#include "jbpf_test_def.h"
#include <unistd.h>

jbpf_output_map(map1, req_resp, 1000);

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
    // if success counter_a ++ otherwise counter_b ++;
    req_resp* out1;
    out1 = jbpf_get_output_buf(&map1);
    if (!out1) {
        data->counter_b++;
        return 0;
    }
    out1->req_or_resp.resp.id = data->counter_a;
    out1->which_req_or_resp = 2;
    if (jbpf_send_output(&map1) < 0) {
        data->counter_b++;
    } else {
        data->counter_a++;
    }
    return 0;
}
