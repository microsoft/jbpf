// Copyright (c) Microsoft Corporation. All rights reserved.

#include <string.h>
#include "jbpf_defs.h"
#include "jbpf_helper.h"
#include "jbpf_test_def.h"

// clang-format off
#define APPLY_TO_IDS(X) \
    X(32) X(33) X(34) X(35) X(36) X(37) X(38) X(39) X(40) X(41) X(42) X(43) X(44) X(45) X(46) X(47) \
    X(48) X(49) X(50) X(51) X(52) X(53) X(54) X(55) X(56) X(57) X(58) X(59) X(60) X(61) X(62) X(63)
// clang-format on

// define max number of helper functions
#define helper_func(id) static int (*unknown_helper_function_##id)(void*, int) = (void*)id;

APPLY_TO_IDS(helper_func)

// Step 1: Define the X macro for generating case statements
#define CALL_HELPER_FUNC_CASE(id)                    \
    case id:                                         \
        res = unknown_helper_function_##id(ctx, id); \
        break;
// Step 2: Define the CALL_HELPER_FUNC macro using a switch-case block
#define CALL_HELPER_FUNC(ctx, id)               \
    ({                                          \
        int res = -1;                           \
        switch (id) {                           \
            APPLY_TO_IDS(CALL_HELPER_FUNC_CASE) \
        default:                                \
            break;                              \
        }                                       \
        res;                                    \
    })

// Example function usage:
jbpf_ringbuf_map(output_map, int, 128);

SEC("jbpf_generic")
uint64_t
jbpf_main(void* state)
{
    struct jbpf_generic_ctx* ctx = (struct jbpf_generic_ctx*)state;
    struct packet* p = (struct packet*)(long)ctx->data;
    struct packet* p_end = (struct packet*)(long)ctx->data_end;

    if (p + 1 > p_end)
        return 1;

    if ((p->counter_a >= 32) && (p->counter_a <= 63)) {
        int res = CALL_HELPER_FUNC(ctx, p->counter_a);
        if (jbpf_ringbuf_output(&output_map, &res, sizeof(int)) < 0) {
            return 1;
        }
    } else {
        return 1;
    }

    return 0;
}
