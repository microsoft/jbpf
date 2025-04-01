// Copyright (c) Microsoft Corporation. All rights reserved.
/*
    This codelet tests the jbpf_hash helper function.
    This codelet serves and example of how to use the jbpf_hash helper function.
*/

#include "jbpf_helper.h"
#include "jbpf_test_def.h"
#include "jbpf_fixpoint_utils.h"

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

    // Test conversion from double to fixedpt and back
    double value = 3.75;
    fixedpt result = fixedpt_from_double(value);
    double converted_back = fixedpt_to_double(result);
    data->test_passed = (converted_back == value);

    // Test conversion from uint to fixedpt and back
    unsigned int uint_value = 10;
    fixedpt uint_result = fixedpt_from_uint(uint_value);
    unsigned int uint_converted_back = fixedpt_to_uint(uint_result);
    data->test_passed &= (uint_converted_back == uint_value);

    // Test conversion from fixedpt to double
    fixedpt fixed_value = fixedpt_from_double(5.5);
    double fixed_result = fixedpt_to_double(fixed_value);
    data->test_passed &= (fixed_result == 5.5);

    // Test conversion from fixedpt to uint
    fixedpt fixed_uint_value = fixedpt_from_uint(20);
    unsigned int fixed_uint_result = fixedpt_to_uint(fixed_uint_value);
    data->test_passed &= (fixed_uint_result == 20);
    return 0;
}
