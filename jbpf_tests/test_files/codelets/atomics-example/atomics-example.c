// Copyright (c) Microsoft Corporation. All rights reserved.
/**
 * This is an example codelet to demonstrate the usage of atomic operations in jbpf.
 * This test codelet demonstrates the usage of atomic operations in jbpf.
 * The codelet tests the following atomic operations:
 * 1. __sync_fetch_and_add
 * 2. __sync_fetch_and_or
 * 3. __sync_fetch_and_and
 * 4. __sync_fetch_and_xor
 * 5. __sync_lock_test_and_set
 * 6. __sync_val_compare_and_swap
 */

#include "jbpf_helper.h"
#include "jbpf_defs.h"
#include "jbpf_test_def.h"

SEC("jbpf_generic")
uint64_t
jbpf_main(void* state)
{
    struct jbpf_generic_ctx* ctx = (struct jbpf_generic_ctx*)state;
    struct counter_vals* cvals = (struct counter_vals*)ctx->data;
    struct counter_vals* cvals_end = (struct counter_vals*)ctx->data_end;
    if (cvals + 1 > cvals_end) {
        return 3;
    }
    uint64_t u64_old_swap, u64_unchanged_swap, u64_changed_swap;
    uint64_t u64_xadd_res;
    uint64_t u64_fetch_or_res, u64_fetch_and_res, u64_fetch_xor_res;

    for (int i = 0; i < 1000; i++) {
        __sync_fetch_and_add(&cvals->u64_counter, 1);
        u64_xadd_res = __sync_fetch_and_add(&cvals->u64_counter, 1);
    }

    __sync_fetch_and_or(&cvals->u64_or_flag, 0xffffffffffffffff);

    u64_fetch_or_res = __sync_fetch_and_or(&cvals->u64_or_flag, 0xffffffffffffffff);

    __sync_fetch_and_and(&cvals->u64_and_flag, 0xffffffffffffffff);
    u64_fetch_and_res = __sync_fetch_and_and(&cvals->u64_and_flag, 0xffffffffffffffff);

    __sync_fetch_and_xor(&cvals->u64_xor_flag, 0xffffffffffffffff);
    u64_fetch_xor_res = __sync_fetch_and_xor(&cvals->u64_xor_flag, 0xffffffffffffffff);

    u64_old_swap = __sync_lock_test_and_set(&cvals->u64_swap_val, cvals->u64_counter);

    u64_unchanged_swap = __sync_val_compare_and_swap(&cvals->u64_cmp_swap_val, cvals->u64_counter, cvals->u64_counter);
    u64_changed_swap = __sync_val_compare_and_swap(
        &cvals->u64_changed_cmp_swap_val, cvals->u64_changed_cmp_swap_val, cvals->u64_counter);

    // // Print the values
    // jbpf_printf_debug("u64_counter is %ld and old value was %ld\n", cvals->u64_counter, u64_xadd_res);

    // jbpf_printf_debug("u64_or_flag is 0x%llx and u64_fetch_or_res is 0x%llx\n", cvals->u64_or_flag,
    // u64_fetch_or_res); jbpf_printf_debug(
    //     "u64_and_flag is 0x%llx and u64_fetch_and_res is 0x%llx\n", cvals->u64_and_flag, u64_fetch_and_res);
    // jbpf_printf_debug(
    //     "u64_xor_flag is 0x%llx and u64_fetch_xor_res is 0x%llx\n", cvals->u64_xor_flag, u64_fetch_xor_res);

    // jbpf_printf_debug("u64_swap_val is %lld and old val is %lld\n", cvals->u64_swap_val, u64_old_swap);
    // jbpf_printf_debug(
    //     "u64_unchanged_swap is %lld and u64_cmp_swap_val is %lld\n", u64_unchanged_swap, cvals->u64_cmp_swap_val);
    // jbpf_printf_debug(
    //     "u64_changed_swap is %lld and u64_changed_cmp_swap_val is %lld\n",
    //     u64_changed_swap,
    //     cvals->u64_changed_cmp_swap_val);

    // jbpf_printf_debug("The non-atomic counter is %ld", cvals->u64_non_atomic_counter);

    // jbpf_printf_debug("u64_swap_val is 0xllx\n", cvals->u64_swap_val);
    return 0;
}
