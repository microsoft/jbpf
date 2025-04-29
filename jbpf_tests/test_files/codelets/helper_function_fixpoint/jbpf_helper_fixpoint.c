// Copyright (c) Microsoft Corporation. All rights reserved.
/**
 * The purpose of the codelet is to make sure we can use the following functions:
 * 1. float_to_fixed
 * 2. fixed_to_float
 * 3. double_to_fixed
 * 4. fixed_to_double
 */

#include "jbpf_helper.h"
#include "jbpf_test_def.h"
#include "jbpf_helper_utils.h"

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

    // Test case 1: convert two floats to fixedpt and add them, then convert back to float
    float a = 1.23;
    float b = 2.34;
    // 3.57
    data->test_float_1 = fixed_to_float(fixedpt_add(float_to_fixed(a), float_to_fixed(b)));

    // Test case 2: convert two doubles to fixedpt and add them, then convert back to double
    double c = 3.45;
    double d = 4.56;
    // 8.01
    data->test_double_1 = fixed_to_double(fixedpt_add(double_to_fixed(c), double_to_fixed(d)));

    // Test case 3: test fixed_to_float
    data->test_float_2 = fixed_to_float(fixedpt_rconst(3.14));

    // Test case 4: test fixed_to_double
    data->test_double_2 = fixed_to_double(fixedpt_rconst(2.71));

    // Test case 5: test float_to_fixed
    data->test_int_1 = float_to_fixed(1.2);

    // Test case 6: test double_to_fixed
    data->test_int64_1 = double_to_fixed(2.345);

    // Test case 7: test fixedpt_sub
    data->test_float_3 = fixed_to_float(fixedpt_sub(fixedpt_rconst(5.1), fixedpt_rconst(2.9)));

    // Test case 8: test fixedpt_mul
    data->test_float_4 = fixed_to_float(fixedpt_mul(fixedpt_rconst(2.5), fixedpt_rconst(1.3)));

    // Test case 9: test fixedpt_div
    data->test_float_5 = fixed_to_float(fixedpt_div(fixedpt_rconst(7.5), fixedpt_rconst(2.5)));

    // Test case 10: test fixedpt_sub for double
    data->test_double_3 = fixed_to_double(fixedpt_sub(double_to_fixed(5.1), double_to_fixed(2.9)));

    // Test case 11: test fixedpt_mul for double
    data->test_double_4 = fixed_to_double(fixedpt_mul(double_to_fixed(2.5), double_to_fixed(1.3)));

    // Test case 12: test fixedpt_div for double
    data->test_double_5 = fixed_to_double(fixedpt_div(double_to_fixed(7.5), double_to_fixed(2.5)));

    // Test case 13: mixed operations for float
    float a1 = 1.23;
    float b1 = 2.34;
    double c1 = 3.45;
    double d1 = 4.56;
    // (a1 + b2 * c1) / d1 = (1.23 + 2.34 * 3.45) / 4.56 = 2.04
    data->test_float_6 = fixed_to_float(fixedpt_div(
        fixedpt_add(float_to_fixed(a1), fixedpt_mul(float_to_fixed(b1), double_to_fixed(c1))), double_to_fixed(d1)));

    // Test case 14: mixed operations for double
    double a2 = 1.23;
    double b2 = 2.34;
    double c2 = 3.45;
    double d2 = 4.56;
    // (a2 + b2 * c2) / d2 = (1.23 + 2.34 * 3.45) / 4.56 = 2.04
    data->test_double_6 = fixed_to_double(fixedpt_div(
        fixedpt_add(double_to_fixed(a2), fixedpt_mul(double_to_fixed(b2), double_to_fixed(c2))), double_to_fixed(d2)));

    return 0;
}
