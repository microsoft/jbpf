// Copyright (c) Microsoft Corporation. All rights reserved.
/*
    This codelet tests the jbpf fix point helper functions.
    This codelet serves as an example of how to use the jbpf fix point helper functions.
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

    data->test_passed = 1;

    // Test 1: Basic float-to-fixed and back to float conversion
    float value = 1.0f;
    int32_t fixed_value = float_to_fixed(value);
    float float_value = fixed_to_float(fixed_value);

    // Allow some small error in floating point comparison
    data->test_passed &= compare_float(float_value, 1.0f, 0.0001f);

    // Test 2: Convert negative float to fixed and back to float
    float neg_value = -1.0f;
    int32_t fixed_value_neg_float = float_to_fixed(neg_value);
    float float_value_neg_float = fixed_to_float(fixed_value_neg_float);

    // Allow some small error in floating point comparison
    data->test_passed &= compare_float(float_value_neg_float, -1.0f, 0.0001f);

    // Test 3: Basic double-to-fixed and back to float conversion
    double value_double = 1.0;
    int64_t fixed_value_double = float_to_fixed(value_double);
    double double_value = fixed_to_float(fixed_value_double);

    // Allow some small error in floating point comparison
    data->test_passed &= compare_double(double_value, 1.0, 0.0001);

    // Test 4: Convert negative double to fixed and back to float
    double neg_value_double = -1.0f;
    int64_t fixed_value_neg_double = double_to_fixed(neg_value_double);
    double double_value_neg_double = fixed_to_double(fixed_value_neg_double);

    // Allow some small error in floating point comparison
    data->test_passed &= compare_double(double_value_neg_double, -1.0, 0.0001);

    return 0;
}
