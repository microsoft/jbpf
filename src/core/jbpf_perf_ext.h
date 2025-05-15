// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef JBPF_PERF_EXT_H
#define JBPF_PERF_EXT_H

#include "jbpf_common_types.h"

/* Change these values to adjust the histogram */
/**
 * @brief Number of histogram bins
 */
#define JBPF_NUM_HIST_BINS 64

/**
 * @brief Number of nanoseconds per bin
 */
#define JBPF_NSECS_PER_BIN 100

/**
 * @brief Maximum number of hooks
 */
#define MAX_NUM_HOOKS 128

/**
 * @brief JBPF performance data structure
 * @param num Number of times the hook was called
 * @param min Minimum time taken by the hook
 * @param max Maximum time taken by the hook
 * @param hist Histogram of time taken by the hook
 * @param hook_name Name of the hook
 * @ingroup hooks
 * @ingroup core
 */
struct jbpf_perf_data
{
    uint64_t num;
    uint64_t min;
    uint64_t max;
    uint32_t hist[JBPF_NUM_HIST_BINS];
    jbpf_hook_name_t hook_name;
};

/**
 * @brief JBPF performance hook list structure
 * @note This is the struct that is passed to the perf hook. See [here](../dosc/add_new_hook.md) for more information.
 * @param num_reported_hooks Number of hooks reported
 * @param perf_data Performance data for the hooks
 * @ingroup hooks
 * @ingroup core
 */
struct jbpf_perf_hook_list
{
    uint8_t num_reported_hooks;
    struct jbpf_perf_data perf_data[MAX_NUM_HOOKS];
};

#endif
