// Copyright (c) Microsoft Corporation. All rights reserved.
/*
This codelet tests different return codes from a codelet.
Only one thread will be able to write to the control input at a time.
*/

#include <string.h>

#include "jbpf_defs.h"
#include "jbpf_helper.h"
#include "jbpf_test_def.h"

jbpf_control_input_map(input_map, custom_api, 100);

jbpf_output_map(output_map, custom_api, 1000);

SEC("jbpf_generic")
uint64_t
jbpf_main(void* state)
{
    struct jbpf_generic_ctx* ctx = (struct jbpf_generic_ctx*)state;
    struct counter_vals* data = (struct counter_vals*)ctx->data;
    struct counter_vals* data_end = (struct counter_vals*)ctx->data_end;
    if (data + 1 > data_end) {
        return 1;
    }

    custom_api api;

    if (jbpf_control_input_receive(&input_map, &api, sizeof(custom_api)) > 0) {
        if ((data->u32_counter % 2) == 0) {
            return data->u32_counter;
        } else {
            custom_api* out;
            out = jbpf_get_output_buf(&output_map);
            if (!out) {
                return data->u32_and_flag;
            }
            out->command = data->u32_counter;
            if (jbpf_send_output(&output_map) < 0) {
                return data->u32_and_flag;
            }
        }
    } else {
        return data->u32_or_flag;
    }
    return data->u32_xor_flag;
}
