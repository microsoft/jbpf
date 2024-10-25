// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef JBPF_HOOK_DEFS_H
#define JBPF_HOOK_DEFS_H

#include <stdbool.h>

#include "ubpf.h"

#include "jbpf_hook_defs_ext.h"
#include "jbpf_common_types.h"

#include "ck_epoch.h"

#define DEFAULT_RUNTIME_THRESHOLD 1000000

#ifndef HOOKS_SECTION_ALIGNMENT
#if defined(__LP64__)
#define HOOKS_SECTION_ALIGNMENT 8
#else
#define HOOKS_SECTION_ALIGNMENT 4
#endif
#endif

typedef ubpf_jit_fn jbpf_jit_fn;

enum jbpf_hook_type
{
    JBPF_HOOK_TYPE_MON = 0,
    JBPF_HOOK_TYPE_CTRL
};

struct jbpf_hook_codelet
{
    jbpf_jit_fn jbpf_codelet;
    jbpf_codelet_priority_t prio;
    jbpf_runtime_threshold_t time_thresh;
    ck_epoch_entry_t epoch_entry;
};

struct jbpf_hook
{
    const char* name;
    struct jbpf_hook_codelet* codelets;

    enum jbpf_hook_type hook_type;
    bool jbpf_perf_active;
    struct jbpf_perf_data* perf_data;
};

#endif
