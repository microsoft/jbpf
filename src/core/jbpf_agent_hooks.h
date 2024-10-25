// Copyright (c) Microsoft Corporation. All rights reserved.
#ifndef JBPF_AGENT_HOOKS_
#define JBPF_AGENT_HOOKS_

#include "jbpf_utils.h"
#include "jbpf_hook.h"
#include "jbpf_defs.h"

/**
 * @brief Declare a jbpf hook: report_stats
 * @note This is called internally by the jbpf library to collect stats about the loaded codelets and their runtime.
 * @ingroup hooks
 * @ingroup core
 */
DECLARE_JBPF_HOOK(
    report_stats,
    struct jbpf_stats_ctx ctx,
    ctx,
    HOOK_PROTO(struct jbpf_perf_hook_list* hook_perfs, uint32_t meas_period),
    HOOK_ASSIGN(ctx.meas_period = meas_period; ctx.data = (uint64_t)(void*)hook_perfs;
                ctx.data_end = (uint64_t)((uint8_t*)hook_perfs + sizeof(struct jbpf_perf_hook_list));))

/**
 * @brief Declare a jbpf hook: periodic_call
 * @note This hook will be called periodically with the specified period.
 * @note We could use this hook to load codelets that need to perform periodic tasks, while being outside of any fast
 * path (e.g., aggregate data stored in shared maps and clear the maps).
 * @ingroup hooks
 * @ingroup core
 */
DECLARE_JBPF_HOOK(
    periodic_call,
    struct jbpf_stats_ctx ctx,
    ctx,
    HOOK_PROTO(uint32_t meas_period),
    HOOK_ASSIGN(ctx.meas_period = meas_period; ctx.data = (uint64_t)(void*)NULL; ctx.data_end = (uint64_t)(void*)NULL;))

#endif
