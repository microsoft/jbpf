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

    float a = 1.0;
    float b = 2.0;

    // 3.0 in fixedpt
    data->test_passed = float_to_fixed(a) + float_to_fixed(b);

    double aa = 3.0;
    double bb = 4.0;

    // 7.0 in fixedpt, so  3.0 + 7.0 = 10.0 which should be passed to the test_passed value
    data->test_passed += double_to_fixed(aa) + double_to_fixed(bb);

    return 0;
}
