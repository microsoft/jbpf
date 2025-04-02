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
    uint32_t value = 0x3F800000; // 1.0 in IEEE 754 format
    int32_t fixed_value = float_to_fixed(value);
    float float_value = fixed_to_float(fixed_value);

    // Allow some small error in floating point comparison
    data->test_passed &= compare_float(float_value, 1.0f, 0.0001f);

    // Test 2: Convert negative float to fixed and back to float
    uint32_t ieee_float = 0xBF800000; // -1.0 in IEEE 754 format
    int32_t fixed_value_neg_float = float_to_fixed(ieee_float);
    float float_value_neg_float = fixed_to_float(fixed_value_neg_float);

    // Allow some small error in floating point comparison
    data->test_passed &= compare_float(float_value_neg_float, -1.0f, 0.0001f);

    // Test 3: Test conversion for a small positive float value (e.g., 0.5)
    uint32_t small_pos_float = 0x3F000000; // 0.5 in IEEE 754 format
    int32_t fixed_value_small_pos = float_to_fixed(small_pos_float);
    float float_value_small_pos = fixed_to_float(fixed_value_small_pos);

    // Allow some small error in floating point comparison
    data->test_passed &= compare_float(float_value_small_pos, 0.5f, 0.0001f);

    // Test 4: Test conversion for a small negative float value (e.g., -0.5)
    uint32_t small_neg_float = 0xBF000000; // -0.5 in IEEE 754 format
    int32_t fixed_value_small_neg = float_to_fixed(small_neg_float);
    float float_value_small_neg = fixed_to_float(fixed_value_small_neg);

    // Allow some small error in floating point comparison
    data->test_passed &= compare_float(float_value_small_neg, -0.5f, 0.0001f);

    // Test 5: Convert maximum float value (positive) and check
    uint32_t max_pos_float = 0x7F7FFFFF; // max positive float before infinity in IEEE 754 format
    int32_t fixed_value_max_pos = float_to_fixed(max_pos_float);
    float float_value_max_pos = fixed_to_float(fixed_value_max_pos);

    // Allow some small error in floating point comparison
    data->test_passed &= compare_float(float_value_max_pos, 3.4028235e38f, 0.0001f);

    // Test 6: Convert minimum float value (negative) and check
    uint32_t max_neg_float = 0xFF7FFFFF; // max negative float before negative infinity in IEEE 754 format
    int32_t fixed_value_max_neg = float_to_fixed(max_neg_float);
    float float_value_max_neg = fixed_to_float(fixed_value_max_neg);

    // Allow some small error in floating point comparison
    data->test_passed &= compare_float(float_value_max_neg, -3.4028235e38f, 0.0001f);

    return 0;
}
