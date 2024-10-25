// Copyright (c) Microsoft Corporation. All rights reserved.
/*
This codelet tests the concurrency of the control input API.
Only one thread will be able to write to the control input at a time.
*/

#include <string.h>

#include "jbpf_defs.h"
#include "jbpf_helper.h"
#include "jbpf_test_def.h"

jbpf_control_input_map(input_map1, custom_api, 100);

jbpf_control_input_map(input_map2, custom_api2, 100);

jbpf_output_map(output_map1, custom_api, 1000);

jbpf_output_map(output_map2, custom_api2, 1000);

SEC("jbpf_generic")
uint64_t
jbpf_main(void* state)
{
    struct jbpf_generic_ctx* ctx = (struct jbpf_generic_ctx*)state;
    struct packet4* data = (struct packet4*)ctx->data;
    struct packet4* data_end = (struct packet4*)ctx->data_end;
    if (data + 1 > data_end) {
        return 1;
    }

    custom_api api;
    custom_api2 api2;

    if (jbpf_control_input_receive(&input_map1, &api, sizeof(custom_api)) > 0) {
        data->counter_a++;
        custom_api* out1;
        out1 = jbpf_get_output_buf(&output_map1);
        if (!out1) {
            return 1;
        }
        memcpy(out1, &api, sizeof(custom_api));
        if (jbpf_send_output(&output_map1) < 0) {
            return 1;
        }
    } else {
        data->counter_b++;
    }
    if (jbpf_control_input_receive(&input_map2, &api2, sizeof(custom_api2)) > 0) {
        data->counter_c++;
        custom_api2* out2;
        out2 = jbpf_get_output_buf(&output_map2);
        if (!out2) {
            return 1;
        }
        memcpy(out2, &api2, sizeof(custom_api2));
        if (jbpf_send_output(&output_map2) < 0) {
            return 1;
        }
    } else {
        data->counter_d++;
    }
    return 0;
}
