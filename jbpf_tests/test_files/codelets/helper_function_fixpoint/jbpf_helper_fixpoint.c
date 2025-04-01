// Copyright (c) Microsoft Corporation. All rights reserved.
/*
    This codelet tests the jbpf fix point helper functions.
    This codelet serves as an example of how to use the jbpf fix point helper functions.
*/

#include "jbpf_helper.h"
#include "jbpf_test_def.h"

SEC("jbpf_generic")
uint64_t
jbpf_main(void* state)
{
    struct jbpf_generic_ctx* ctx = (struct jbpf_generic_ctx*)state;
    struct test_packet* data = (struct test_packet*)ctx->data;
    struct test_packet* data_end = (struct test_packet*)ctx->data_end;
    if (data + 1 > data_end) {
        return 1;
    }

    data->test_passed = 1;
    int value = 1234;

    // Test fixedpt_from_double_approx
    fixedpt result = jbpf_fixedpt_from_double_approx(value);
    if (result != (value * FIXEDPT_ONE)) {
        data->test_passed = 0;
        return 1;
    }

    // Test fixedpt_to_int_approx
    int converted_back = jbpf_fixedpt_to_int_approx(result);
    if (converted_back != value) {
        data->test_passed = 0;
        return 1;
    }

    // Test fixedpt_from_uint and fixedpt_to_uint
    unsigned int uint_value = 5678;
    fixedpt uint_result = jbpf_fixedpt_from_uint(uint_value);
    unsigned int uint_converted_back = jbpf_fixedpt_to_uint(uint_result);
    if (uint_converted_back != uint_value) {
        data->test_passed = 0;
        return 1;
    }

    // Test fixedpt_from_double and fixedpt_to_double
    double double_value = 3.75;
    fixedpt double_result = jbpf_fixedpt_from_double(double_value);
    double double_converted_back = jbpf_fixedpt_to_double(double_result);
    // can't directly compare double values in ebpf
    jbpf_printf_debug("double_converted_back: %0xx\n", double_converted_back);

    return 0;
}
