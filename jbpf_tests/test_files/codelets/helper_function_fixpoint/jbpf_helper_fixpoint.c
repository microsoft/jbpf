// Copyright (c) Microsoft Corporation. All rights reserved.
/**
 * This codelet tests the fixpoint helper function.
 * It performs various tests on the floating-point to fixed-point conversion functions.
 * The tests include:
 * - Basic float-to-fixed and back to float conversion
 * - Convert negative float to fixed and back to float
 * - Basic double-to-fixed and back to float conversion
 * - Convert negative double to fixed and back to float
 * - Complex math: Add two fixed-point values
 * - Complex math: Subtract two fixed-point values
 * When the test is run, it will set the test_passed field to 1 if all tests pass.
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
    float value = 2.0f;
    int32_t fixed_value = float_to_fixed(value);
    float float_value = fixed_to_float(fixed_value);

    // Allow some small error in floating point comparison
    data->test_passed &= compare_float(float_value, 2.0f, 0.0001f);

    // Test 2: Convert negative float to fixed and back to float
    float neg_value = -1.0f;
    int32_t fixed_value_neg_float = float_to_fixed(neg_value);
    float float_value_neg_float = fixed_to_float(fixed_value_neg_float);

    // Allow some small error in floating point comparison
    data->test_passed &= compare_float(float_value_neg_float, -1.0f, 0.0001f);

    // Test 3: Basic double-to-fixed and back to float conversion
    double value_double = 1.0;
    int64_t fixed_value_double = double_to_fixed(value_double);
    double double_value = fixed_to_double(fixed_value_double);

    // Allow some small error in floating point comparison
    data->test_passed &= compare_double(double_value, 1.0, 0.0001);

    // Test 4: Convert negative double to fixed and back to float
    double neg_value_double = -1.0f;
    int64_t fixed_value_neg_double = double_to_fixed(neg_value_double);
    double double_value_neg_double = fixed_to_double(fixed_value_neg_double);

    // Allow some small error in floating point comparison
    data->test_passed &= compare_double(double_value_neg_double, -1.0, 0.0001);

    // Test 5: Complex math: Add two fixed-point values
    float x = 1.0f;
    float y = 2.0f;
    int32_t fixed1 = float_to_fixed(x);
    int32_t fixed2 = float_to_fixed(y);
    int32_t result_fixed = fixed1 + fixed2;
    float result_float = fixed_to_float(result_fixed);
    // Expected result: 1.0 + 2.0 = 3.0
    data->test_passed &= compare_float(result_float, 3.0f, 0.0001f);

    // Test 6: Complex math: Subtract two fixed-point values
    x = 2.0f;
    y = 1.0f;
    fixed1 = float_to_fixed(x);
    fixed2 = float_to_fixed(y);
    result_fixed = fixed1 - fixed2;
    result_float = fixed_to_float(result_fixed);
    // Expected result: 2.0 - 1.0 = 1.0
    data->test_passed &= compare_float(result_float, 1.0f, 0.0001f);

    return 0;
}
