// Copyright (c) Microsoft Corporation. All rights reserved.

#define _GNU_SOURCE

#include <sched.h>

#include "jbpf_hook.h"
#include "jbpf_perf.h"
#include "jbpf_memory.h"
#include "jbpf_utils.h"
#include "jbpf_int.h"
#include "jbpf_agent_hooks.h"
#include "jbpf_logging.h"

DEFINE_JBPF_AGENT_HOOK(report_stats)

double g_jbpf_ticks_per_ns;
uint64_t g_jbpf_tick_freq;

void
_jbpf_calibrate_ticks()
{
    struct timespec sleep_period = {.tv_nsec = 5E8};
    struct timespec start_ts, end_ts;
    uint64_t ns_runtime;
    uint64_t begin = 0, end = 0;
    jbpf_logger(JBPF_INFO, "Calibrating clock ticks\n");
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_ts);
    begin = jbpf_start_time();
    nanosleep(&sleep_period, NULL);
    end = jbpf_end_time();
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_ts);
    ns_runtime = _jbpf_difftimespec_ns(&end_ts, &start_ts);

    g_jbpf_ticks_per_ns = (double)(end - begin) / (double)ns_runtime;

    g_jbpf_tick_freq = ((uint64_t)(g_jbpf_ticks_per_ns * 1000000000.0) >> 0);
    jbpf_logger(JBPF_INFO, "Ticks per ns: %f\n", (double)g_jbpf_ticks_per_ns);
    jbpf_logger(JBPF_INFO, "Tick frequency: %ld\n", g_jbpf_tick_freq);
}

int
jbpf_init_perf()
{
    struct jbpf_hook* hook;
    int i, res = 0;

    _jbpf_calibrate_ticks();

    for (i = 0; i < jbpf_hook_list.num_hooks; i++) {
        hook = jbpf_hook_list.jbpf_hook_p[i];
        res = jbpf_init_perf_hook(hook);
        if (res != 0)
            goto error;
    }

    return res;

error:
    /* Free the perf memory of all previously succesfully initialized hooks */
    for (int j = 0; j < i; j++) {
        jbpf_free_perf_hook(jbpf_hook_list.jbpf_hook_p[j]);
    }
    return -1;
}

int
jbpf_init_perf_hook(struct jbpf_hook* hook)
{
    int res = 0;

    hook->perf_data = jbpf_calloc_mem(JBPF_MAX_NUM_REG_THREADS, sizeof(struct jbpf_perf_data));

    if (!hook->perf_data)
        res = -1;

    return res;
}

void
jbpf_free_perf()
{
    struct jbpf_hook* hook;

    for (int i = 0; i < jbpf_hook_list.num_hooks; i++) {
        hook = jbpf_hook_list.jbpf_hook_p[i];
        jbpf_free_perf_hook(hook);
    }
}

void
jbpf_free_perf_hook(struct jbpf_hook* hook)
{
    jbpf_free_mem(hook->perf_data);
}

struct jbpf_perf_data*
jbpf_get_perf_data(struct jbpf_hook* hook)
{

    struct jbpf_perf_data* old_data;
    struct jbpf_perf_data* new_data;

    old_data = hook->perf_data;
    new_data = jbpf_calloc_mem(JBPF_MAX_NUM_REG_THREADS, sizeof(struct jbpf_perf_data));

    ck_pr_store_ptr(&hook->perf_data, new_data);

    return old_data;
}

void
jbpf_report_perf_stats()
{

    struct jbpf_perf_hook_list jbpf_s;
    struct jbpf_perf_data* tmp_perf_data;
    struct jbpf_hook* hook;
    struct jbpf_perf_data* perf_data[MAX_NUM_HOOKS];
    jbpf_s.num_reported_hooks = 0;

    memset(&jbpf_s, 0, sizeof(struct jbpf_perf_hook_list));

    for (int i = 0; i < jbpf_hook_list.num_hooks; i++) {
        hook = jbpf_hook_list.jbpf_hook_p[i];
        if (strncmp(hook->name, "report_stats", JBPF_HOOK_NAME_LEN) == 0)
            continue;
        perf_data[jbpf_s.num_reported_hooks] = jbpf_get_perf_data(hook);
        strncpy(jbpf_s.perf_data[jbpf_s.num_reported_hooks].hook_name, hook->name, JBPF_HOOK_NAME_LEN - 1);
        jbpf_s.perf_data[jbpf_s.num_reported_hooks].hook_name[JBPF_HOOK_NAME_LEN - 1] = '\0';
        jbpf_s.num_reported_hooks++;
    }

    // Block until all structures with perf data are no longer accessible
    ck_epoch_barrier(e_record);

    for (int i = 0; i < jbpf_s.num_reported_hooks; i++) {
        tmp_perf_data = perf_data[i];

        for (int j = 0; j < JBPF_MAX_NUM_REG_THREADS; j++) {

            if (jbpf_s.perf_data[i].min == 0 ||
                (jbpf_s.perf_data[i].min > tmp_perf_data[j].min && tmp_perf_data[j].min > 0))
                jbpf_s.perf_data[i].min = tmp_perf_data[j].min;

            if (jbpf_s.perf_data[i].max < tmp_perf_data[j].max)
                jbpf_s.perf_data[i].max = tmp_perf_data[j].max;

            jbpf_s.perf_data[i].num += tmp_perf_data[j].num;

            for (int k = 0; k < JBPF_NUM_HIST_BINS; k++) {
                jbpf_s.perf_data[i].hist[k] += tmp_perf_data[j].hist[k];
            }
        }
        jbpf_free_mem(tmp_perf_data);
    }

    // Run stats hook
    jbpf_logger(JBPF_INFO, "Running report_stats hook ...\n");
    hook_report_stats(&jbpf_s, MAINTENANCE_CHECK_INTERVAL);
}
