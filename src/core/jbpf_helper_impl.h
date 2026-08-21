#ifndef JBPF_HELPER_IMPL_H
#define JBPF_HELPER_IMPL_H

#include "ubpf.h"
#include "jbpf.h"

void
jbpf_register_helper_functions(struct ubpf_vm* vm);

// wrapper functions for testing
int
__jbpf_rand(void);
void
__set_seed(unsigned int x);
uint64_t
__jbpf_get_sys_time_diff_ns(uint64_t start_time, uint64_t end_time);
uint32_t
__jbpf_hash(void* item, uint64_t size);
void
__jbpf_set_e_runtime_threshold(uint64_t threshold);
void
__jbpf_mark_runtime_init(void);
int
__jbpf_runtime_limit_exceeded(void);

#ifdef __cplusplus
extern "C"
{
#endif

    // emulator
    jbpf_helper_func_def_t*
    __get_custom_helper_functions(void);

    const jbpf_helper_func_def_t*
    __get_default_helper_functions(void);

#ifdef __cplusplus
}
#endif

#endif