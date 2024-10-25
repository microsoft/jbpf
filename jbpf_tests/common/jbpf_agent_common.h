// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef JBPF_AGENT_COMMON
#define JBPF_AGENT_COMMON

#include "jbpf_hook.h"
#include "jbpf_defs.h"
#include "jbpf_logging.h"
#include "jbpf_test_def.h"
#include "jbpf_verifier_extension_defs.h"

#include <stdio.h>

DECLARE_JBPF_HOOK(
    test0,
    struct jbpf_generic_ctx ctx,
    ctx,
    HOOK_PROTO(struct counter_vals* p, int ctx_id),
    HOOK_ASSIGN(ctx.ctx_id = ctx_id; ctx.data = (uint64_t)(void*)p; ctx.data_end = (uint64_t)(void*)(p + 1);))

// have a hook which have a name which starts with the same characters as another hook
DECLARE_JBPF_HOOK(
    test1111,
    struct jbpf_generic_ctx ctx,
    ctx,
    HOOK_PROTO(struct packet* p, int ctx_id),
    HOOK_ASSIGN(ctx.ctx_id = ctx_id; ctx.data = (uint64_t)(void*)p; ctx.data_end = (uint64_t)(void*)(p + 1);))

DECLARE_JBPF_HOOK(
    test_single_result,
    struct jbpf_generic_ctx ctx,
    ctx,
    HOOK_PROTO(struct test_packet* p, int ctx_id),
    HOOK_ASSIGN(ctx.ctx_id = ctx_id; ctx.data = (uint64_t)(void*)p; ctx.data_end = (uint64_t)(void*)(p + 1);))

DECLARE_JBPF_HOOK(
    test1,
    struct jbpf_generic_ctx ctx,
    ctx,
    HOOK_PROTO(struct packet* p, int ctx_id),
    HOOK_ASSIGN(ctx.ctx_id = ctx_id; ctx.data = (uint64_t)(void*)p; ctx.data_end = (uint64_t)(void*)(p + 1);))

DECLARE_JBPF_HOOK(
    test2,
    struct jbpf_generic_ctx ctx,
    ctx,
    HOOK_PROTO(struct packet* p, int ctx_id),
    HOOK_ASSIGN(ctx.ctx_id = ctx_id; ctx.data = (uint64_t)(void*)p; ctx.data_end = (uint64_t)(void*)(p + 1);))

DECLARE_JBPF_HOOK(
    test3,
    struct jbpf_generic_ctx ctx,
    ctx,
    HOOK_PROTO(struct packet4* p, int ctx_id),
    HOOK_ASSIGN(ctx.ctx_id = ctx_id; ctx.data = (uint64_t)(void*)p; ctx.data_end = (uint64_t)(void*)(p + 1);))

DECLARE_JBPF_HOOK(
    test4,
    struct jbpf_generic_ctx ctx,
    ctx,
    HOOK_PROTO(struct packet* p, int ctx_id),
    HOOK_ASSIGN(ctx.ctx_id = ctx_id; ctx.data = (uint64_t)(void*)p; ctx.data_end = (uint64_t)(void*)(p + 1);))

DECLARE_JBPF_HOOK(
    test5,
    struct jbpf_generic_ctx ctx,
    ctx,
    HOOK_PROTO(struct packet* p, int ctx_id),
    HOOK_ASSIGN(ctx.ctx_id = ctx_id; ctx.data = (uint64_t)(void*)p; ctx.data_end = (uint64_t)(void*)(p + 1);))

DECLARE_JBPF_HOOK(
    test6,
    struct jbpf_generic_ctx ctx,
    ctx,
    HOOK_PROTO(struct packet* p, int ctx_id),
    HOOK_ASSIGN(ctx.ctx_id = ctx_id; ctx.data = (uint64_t)(void*)p; ctx.data_end = (uint64_t)(void*)(p + 1);))

DECLARE_JBPF_HOOK(
    test7,
    struct my_new_jbpf_ctx ctx,
    ctx,
    HOOK_PROTO(
        struct packet* p, uint64_t meta_data, uint32_t static_field1, uint16_t static_field2, uint8_t static_field3),
    HOOK_ASSIGN(ctx.data = (uint64_t)(void*)p; ctx.data_end = (uint64_t)(void*)(p + 1); ctx.meta_data = meta_data;
                ctx.static_field1 = static_field1;
                ctx.static_field2 = static_field2;
                ctx.static_field3 = static_field3;))

// This hook has a name that is the maximum hook name length
DECLARE_JBPF_HOOK(
    test0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789a,
    struct jbpf_generic_ctx ctx,
    ctx,
    HOOK_PROTO(struct packet* p, int ctx_id),
    HOOK_ASSIGN(ctx.ctx_id = ctx_id; ctx.data = (uint64_t)(void*)p; ctx.data_end = (uint64_t)(void*)(p + 1);))

DEFINE_JBPF_HOOK(test0)
DEFINE_JBPF_HOOK(test1111)
DEFINE_JBPF_HOOK(test_single_result)
DEFINE_JBPF_HOOK(test1)
DEFINE_JBPF_HOOK(test2)
DEFINE_JBPF_HOOK(test3)
DEFINE_JBPF_HOOK(test4)
DEFINE_JBPF_HOOK(test5)
DEFINE_JBPF_HOOK(test6)
DEFINE_JBPF_HOOK(test7)
DEFINE_JBPF_HOOK(
    test0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789a)

DECLARE_JBPF_CTRL_HOOK(
    ctrl_test0,
    struct jbpf_generic_ctx ctx,
    ctx,
    HOOK_PROTO(struct counter_vals* p, int ctx_id),
    HOOK_ASSIGN(ctx.ctx_id = ctx_id; ctx.data = (uint64_t)(void*)p; ctx.data_end = (uint64_t)(void*)(p + 1);))

DEFINE_JBPF_CTRL_HOOK(ctrl_test0)

#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

    void
    jbpf_agent_logger(jbpf_logging_level level, const char* s, ...);

    void
    sig_handler(int signo);

    int
    handle_signal(void);

    // Fisher-Yates algorithm, which guarantees an unbiased shuffle
    void
    shuffle(int* array, size_t n);

#pragma once
#ifdef __cplusplus
}
#endif

#endif /* JBPF_AGENT_COMMON */
