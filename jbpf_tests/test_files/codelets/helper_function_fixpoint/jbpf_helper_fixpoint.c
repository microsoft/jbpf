// Copyright (c) Microsoft Corporation. All rights reserved.
/**
 * The purpose of the codelet is to make sure we can use the following functions:
 * 1. float_to_fixed
 * 2. fixed_to_float
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

    int32_t value = float_to_fixed(1) + double_to_fixed(1);
    data->test_passed = value;
    return 0;
}
