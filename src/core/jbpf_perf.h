// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef JBPF_PERF_H
#define JBPF_PERF_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "jbpf_hook_defs.h"
#include "jbpf_utils.h"
#include "jbpf_perf_ext.h"
#include "jbpf.h"
#include "jbpf_device_defs.h"

#define JBPF_HOOK_PERF_DEFAULT_STATE true

extern double g_jbpf_ticks_per_ns;
extern uint64_t g_jbpf_tick_freq;

void
_jbpf_calibrate_ticks(void);

int
jbpf_init_perf(void);
int
jbpf_init_perf_hook(struct jbpf_hook* hook);

void
jbpf_free_perf(void);
void
jbpf_free_perf_hook(struct jbpf_hook* hook);

static inline uint64_t
jbpf_measure_start_time(void)
{
#ifdef JBPF_PERF_OPT
    return jbpf_start_time();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * (uint64_t)1000000000 + (uint64_t)ts.tv_nsec;
#endif
}

static inline uint64_t
jbpf_measure_stop_time(void)
{
#ifdef JBPF_PERF_OPT
    return jbpf_end_time();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * (uint64_t)1000000000 + (uint64_t)ts.tv_nsec;
#endif
}

__attribute__((always_inline)) static uint64_t inline jbpf_get_time_diff_ns(uint64_t start_time, uint64_t end_time)
{

    if (start_time == end_time)
        return 0;

    uint64_t elapsed_time_calc =
        (end_time >= start_time) ? (end_time - start_time) : (0xFFFFFFFFFFFFFFFF - start_time + end_time + 1);

#ifdef JBPF_PERF_OPT
#if defined(__x86_64__)
    double elapsed_time = (double)elapsed_time_calc / g_jbpf_ticks_per_ns;
    return (uint64_t)elapsed_time;

#elif defined(__aarch64__)
    uint64_t freq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    return (elapsed_time_calc * 1000000000ULL) / freq;
#endif
#endif

    return elapsed_time_calc;
}

static inline void
jbpf_set_perf_hook(struct jbpf_hook* hook, bool active)
{
    __atomic_store(&hook->jbpf_perf_active, &active, __ATOMIC_RELAXED);
}

struct jbpf_perf_data*
jbpf_get_perf_data(struct jbpf_hook* hook);

void
jbpf_report_perf_stats(void);

#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

    __attribute__((always_inline)) static int inline _jbpf_perf_log(
        struct jbpf_perf_data* perf_data, uint64_t start_time, uint64_t end_time)
    {

        /* Get the CPU id */
        uint32_t perf_idx = get_jbpf_hook_thread_id();
        uint32_t bin_idx;
        uint64_t et;

        et = jbpf_get_time_diff_ns(start_time, end_time);

        if (et == 0)
            return -1;

        // bin_idx = et / JBPF_NSECS_PER_BIN;
        bin_idx = JBPF_NUM_HIST_BINS - 1 - __builtin_clzll(et);
        /* Any value that is spilling should go to the last bin */
        if (bin_idx >= JBPF_NUM_HIST_BINS)
            bin_idx = JBPF_NUM_HIST_BINS - 1;

        /* perf_idx is based on thread_id, so no need to lock */
        perf_data[perf_idx].num++;

        if (et < perf_data[perf_idx].min || perf_data[perf_idx].min == 0)
            perf_data[perf_idx].min = et;

        if (et > perf_data[perf_idx].max)
            perf_data[perf_idx].max = et;

        perf_data[perf_idx].hist[bin_idx]++;

        return 0;
    }

#pragma once
#ifdef __cplusplus
}
#endif

#endif /* JBPF_PERF_H */
