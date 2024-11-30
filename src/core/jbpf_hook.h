// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef JBPF_HOOK_H
#define JBPF_HOOK_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "jbpf_common_types.h"
#include "jbpf_hook_defs.h"
#include "jbpf_hook_defs_ext.h"
#include "jbpf_device_defs.h"
#include "jbpf_perf.h"

#include "jbpf.h"

#ifndef DISABLE_JBPF_PERF_STATS
#define JBPF_START_MEASURE_TIME(name)      \
    uint64_t start_time = 0, end_time = 0; \
    start_time = jbpf_measure_start_time();
#define JBPF_STOP_MEASURE_TIME(name)                                         \
    end_time = jbpf_measure_stop_time();                                     \
    if (JBPF_LIKELY(__jbpf_hook_##name.jbpf_perf_active)) {                  \
        struct jbpf_perf_data* perf_data = (&__jbpf_hook_##name)->perf_data; \
        _jbpf_perf_log(perf_data, start_time, end_time);                     \
    }
#else
#define JBPF_START_MEASURE_TIME(name)
#define JBPF_STOP_MEASURE_TIME(name)
#endif

#ifdef JBPF_AUTOREGISTER_THREAD
#define JBPF_REGISTER_THREAD() jbpf_register_thread();
#else
#define JBPF_REGISTER_THREAD()
#endif

extern struct jbpf_hook __start___hook_list[];
extern struct jbpf_hook __stop___hook_list[];

extern struct jbpf_hook __start___jbpf_agent_hook_list[];
extern struct jbpf_hook __stop___jbpf_agent_hook_list[];

enum hook_enabled
{
    JBPF_HOOK_DISABLED = 0,
    JBPF_HOOK_ENABLED = 1,
};

int
jbpf_register_codelet_hook(
    struct jbpf_hook* hook,
    jbpf_jit_fn codelet,
    jbpf_runtime_threshold_t runtime_threshold,
    jbpf_codelet_priority_t prio);
int
jbpf_remove_codelet_hook(struct jbpf_hook* hook, jbpf_jit_fn codelet);

#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#include <ck_epoch.h>

#ifndef PARAMS
#define PARAMS(args...) args
#endif

#define __RUN_JBPF_HOOK(name, args)                              \
    {                                                            \
        do {                                                     \
            e_runtime_threshold = hook_codelet_ptr->time_thresh; \
            hook_codelet_ptr->jbpf_codelet(args);                \
        } while ((++hook_codelet_ptr)->jbpf_codelet);            \
    }

#define HOOK_PROTO(arg...) arg
#define HOOK_ARGS(args...) args
#define HOOK_ASSIGN(args...) args

#ifndef JBPF_DISABLED

#define JBPF_CODELET_MGMT_FUNCS(name)                                                                  \
    static inline int jbpf_register_codelet_##name(                                                    \
        jbpf_jit_fn codelet, jbpf_runtime_threshold_t runtime_threshold, jbpf_codelet_priority_t prio) \
    {                                                                                                  \
        return jbpf_register_codelet_hook(&__jbpf_hook_##name, codelet, runtime_threshold, prio);      \
    }                                                                                                  \
    static inline int jbpf_remove_codelet_hook_##name(jbpf_jit_fn codelet)                             \
    {                                                                                                  \
        return jbpf_remove_codelet_hook(&__jbpf_hook_##name, codelet);                                 \
    }

#define DECLARE_JBPF_CTRL_HOOK(name, ctx_proto, ctx_arg, fields_proto, assign)                            \
    static inline uint64_t ctrl_hook_##name(fields_proto);                                                \
    extern struct jbpf_hook __jbpf_hook_##name;                                                           \
    static inline uint64_t ctrl_hook_##name(fields_proto)                                                 \
    {                                                                                                     \
        struct jbpf_hook_codelet* hook_codelet_ptr;                                                       \
        uint64_t res = JBPF_DEFAULT_CTRL_OP;                                                              \
        hook_codelet_ptr = ck_pr_load_ptr(&(&__jbpf_hook_##name)->codelets);                              \
        if (hook_codelet_ptr) {                                                                           \
            JBPF_REGISTER_THREAD()                                                                        \
            ck_epoch_begin(e_record, NULL);                                                               \
            hook_codelet_ptr = ck_pr_load_ptr(&(&__jbpf_hook_##name)->codelets);                          \
            if (hook_codelet_ptr) {                                                                       \
                ctx_proto;                                                                                \
                assign JBPF_START_MEASURE_TIME(name) e_runtime_threshold = hook_codelet_ptr->time_thresh; \
                res = hook_codelet_ptr->jbpf_codelet(HOOK_ARGS((void*)&ctx_arg, sizeof(ctx_arg)));        \
                JBPF_STOP_MEASURE_TIME(name)                                                              \
            }                                                                                             \
            ck_epoch_end(e_record, NULL);                                                                 \
        }                                                                                                 \
        return res;                                                                                       \
    }                                                                                                     \
    JBPF_CODELET_MGMT_FUNCS(name)

#define DECLARE_JBPF_HOOK(name, ctx_proto, ctx_arg, fields_proto, assign)                                           \
    static inline void hook_##name(fields_proto);                                                                   \
    extern struct jbpf_hook __jbpf_hook_##name;                                                                     \
    static inline void hook_##name(fields_proto)                                                                    \
    {                                                                                                               \
        struct jbpf_hook_codelet* hook_codelet_ptr;                                                                 \
        hook_codelet_ptr = ck_pr_load_ptr(&(&__jbpf_hook_##name)->codelets);                                        \
        if (hook_codelet_ptr) {                                                                                     \
            JBPF_REGISTER_THREAD()                                                                                  \
            ck_epoch_begin(e_record, NULL);                                                                         \
            hook_codelet_ptr = ck_pr_load_ptr(&(&__jbpf_hook_##name)->codelets);                                    \
            if (hook_codelet_ptr) {                                                                                 \
                ctx_proto;                                                                                          \
                assign JBPF_START_MEASURE_TIME(name)                                                                \
                    __RUN_JBPF_HOOK(name, HOOK_ARGS((void*)&ctx_arg, sizeof(ctx_arg))) JBPF_STOP_MEASURE_TIME(name) \
            }                                                                                                       \
            ck_epoch_end(e_record, NULL);                                                                           \
        }                                                                                                           \
    }                                                                                                               \
    JBPF_CODELET_MGMT_FUNCS(name)

/* Here we assing the various information about the hook
 * in corresponding ELF sections so that we can later parse them
 * and identify the location of the functions */
#define DEFINE_JBPF_HOOK_SECTIONS(_name, hook_list_section, hook_names_section, type)                           \
    static const char __strtab_##_name[JBPF_HOOK_NAME_LEN] __attribute__((section(hook_names_section), used)) = \
        #_name;                                                                                                 \
    struct jbpf_hook __jbpf_hook_##_name __attribute__((section(hook_list_section))) __attribute__((used))      \
    __attribute__((aligned(HOOKS_SECTION_ALIGNMENT))) = {                                                       \
        .name = __strtab_##_name,                                                                               \
        .codelets = NULL,                                                                                       \
        .hook_type = type,                                                                                      \
        .jbpf_perf_active = JBPF_HOOK_PERF_DEFAULT_STATE,                                                       \
    };                                                                                                          \
    struct jbpf_hook* jh_start_##_name = __start___hook_list;                                                   \
    struct jbpf_hook* jh_stop_##_name = __stop___hook_list;

#define DEFINE_JBPF_CTRL_HOOK(_name) \
    DEFINE_JBPF_HOOK_SECTIONS(_name, "__hook_list", "__hook_names", JBPF_HOOK_TYPE_CTRL)

#define DEFINE_JBPF_HOOK(_name) DEFINE_JBPF_HOOK_SECTIONS(_name, "__hook_list", "__hook_names", JBPF_HOOK_TYPE_MON)

#define DEFINE_JBPF_AGENT_HOOK(_name) \
    DEFINE_JBPF_HOOK_SECTIONS(_name, "__jbpf_agent_hook_list", "__jbpf_agent_hook_names", JBPF_HOOK_TYPE_MON)

#else /* jbpf hooks are not enabled */

#define JBPF_CODELET_MGMT_FUNCS(name)                                                                            \
    static inline int jbpf_register_codelet_##name(ubpf_jit_fn prog, jbpf_codelet_priority_t prio) { return 0; } \
    static inline int jbpf_remove_codelet_hook_##name(ubpf_jit_fn prog) { return 0; }

#define DECLARE_JBPF_CTRL_HOOK(name, ctx_proto, ctx_arg, fields_proto, assign) \
    static inline uint64_t ctrl_hook_##name(fields_proto) { return 0; }        \
    JBPF_CODELET_MGMT_FUNCS(name)

#define DECLARE_JBPF_HOOK(name, ctx_proto, ctx_arg, fields_proto, assign) \
    static inline void hook_##name(fields_proto) { return; }              \
    JBPF_CODELET_MGMT_FUNCS(name)

#define DEFINE_JBPF_HOOK_SECTIONS(_name, hook_list_section, hook_names_section)

/* Here we assing the various information about the hook
 * in corresponding ELF sections so that we can later parse them
 * and identify the location of the functions */
#define DEFINE_JBPF_CTRL_HOOK(_name)

#define DEFINE_JBPF_HOOK(_name)

#define DEFINE_JBPF_AGENT_HOOK(_name)

#endif /* JBPF_ENABLED */

#pragma once
#ifdef __cplusplus
}
#endif

#endif /* JBPF_HOOK_H */
