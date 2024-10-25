// Copyright (c) Microsoft Corporation. All rights reserved.
/*
    This codelet receives a command from the control input and increments a counter using atomic operations.
    If the control input is received, the counter_a is incremented, otherwise counter_b is incremented.
*/

#include <string.h>

#include "jbpf_defs.h"
#include "jbpf_helper.h"
#include "jbpf_test_def.h"

jbpf_control_input_map(input_map, custom_api, 10000);

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
    custom_api api = {0};

    if (jbpf_control_input_receive(&input_map, &api, sizeof(custom_api)) > 0) {
        __sync_fetch_and_add(&data->counter_a, 1);
    } else {
        __sync_fetch_and_add(&data->counter_b, 1);
    }
    return 0;
}
