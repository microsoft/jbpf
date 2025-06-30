// Copyright (c) Microsoft Corporation. All rights reserved.
/*
This codelet receives a command from the control input and sends it to the output.
*/

#include <string.h>

#include "jbpf_defs.h"
#include "jbpf_helper.h"
#include "jbpf_test_def.h"

jbpf_control_input_map(input_map, custom_api, 10000);

jbpf_output_map(output_map, int, 10000);

SEC("jbpf_generic")
uint64_t
jbpf_main(void* state)
{
    custom_api api = {0};
    int* output;

    jbpf_printf_debug("jbpf_main called with state %p\n", state);
    if (jbpf_control_input_receive(&input_map, &api, sizeof(custom_api)) > 0) {
        jbpf_printf_debug("Received command: %u\n", api.command);
        output = jbpf_get_output_buf(&output_map);

        if (!output) {
            return 1;
        }
        *output = api.command;

        jbpf_printf_debug("Sending output: %d\n", *output);
        if (jbpf_send_output(&output_map) < 0) {
            return 1;
        }
    }
    return 0;
}
