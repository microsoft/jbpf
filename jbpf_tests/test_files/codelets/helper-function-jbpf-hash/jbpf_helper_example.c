// Copyright (c) Microsoft Corporation. All rights reserved.
/*
    This codelet tests the jbpf_hash helper function.
    This codelet serves and example of how to use the jbpf_hash helper function.
*/

#include <string.h>
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
    char msg[14] = {0};
    strcpy(msg, "Hello, World!");
    uint32_t hash = jbpf_hash(msg, 14);
    // avoid hash variable being optimized out
    jbpf_printf_debug("Hash: %x\n", hash);
    data->test_passed = hash == 0xb3a4ed74;
    return 0;
}
