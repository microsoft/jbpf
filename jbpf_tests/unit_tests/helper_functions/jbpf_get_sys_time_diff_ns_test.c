// Copyright (c) Microsoft Corporation. All rights reserved.
/**
 * Unit Tests for jbpf_get_sys_time_diff_ns function
 * This test the following scenarios
 * 1. The difference between the same time should be 0
 * 2. The difference between two different times should be positive
 * 3. The difference between two different times should be positive even if the times are inverted
 * 4. The difference between two times 0 should be 0
 */

#include <assert.h>
#include <stdlib.h>
#include "jbpf_test_lib.h"
#include "jbpf_helper_impl.h"
#include "jbpf_perf.h"
#include "jbpf_utils.h"

int
group_test_setup(void** state)
{
    _jbpf_calibrate_ticks();
    return 0;
}

void
test_jbpf_get_sys_time_diff_ns_same_time(void** state)
{
    uint64_t start_time = 1000000000; // 1 second in nanoseconds
    uint64_t end_time = 1000000000;   // Same time
    uint64_t result = __jbpf_get_sys_time_diff_ns(start_time, end_time);
    JBPF_UNUSED(result);
    assert(result == 0); // The difference should be 0
}

void
test_jbpf_get_sys_time_diff_ns_positive_diff(void** state)
{
    uint64_t start_time = 1000000000; // 1 second in nanoseconds
    uint64_t end_time = 2000000000;   // 2 seconds in nanoseconds
    uint64_t result = __jbpf_get_sys_time_diff_ns(start_time, end_time);
    JBPF_UNUSED(result);
    assert(result > 0); // The difference should be positive.
}

void
test_jbpf_get_sys_time_diff_ns_inverted_times(void** state)
{
    uint64_t start_time = 2000000000; // 2 seconds in nanoseconds
    uint64_t end_time = 1000000000;   // 1 second in nanoseconds
    uint64_t result = __jbpf_get_sys_time_diff_ns(start_time, end_time);
    JBPF_UNUSED(result);
    assert(result > 0); // The difference should be positive.
}

void
test_jbpf_get_sys_time_diff_ns_zero_times(void** state)
{
    uint64_t start_time = 0;
    uint64_t end_time = 0;
    uint64_t result = __jbpf_get_sys_time_diff_ns(start_time, end_time);
    JBPF_UNUSED(result);
    assert(result == 0); // The difference should be 0
}

int
main(int argc, char** argv)
{
    const jbpf_test tests[] = {
        JBPF_CREATE_TEST(test_jbpf_get_sys_time_diff_ns_zero_times, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_get_sys_time_diff_ns_same_time, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_get_sys_time_diff_ns_positive_diff, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_get_sys_time_diff_ns_inverted_times, NULL, NULL, NULL)};

    int num_tests = sizeof(tests) / sizeof(jbpf_test);
    return jbpf_run_test(tests, num_tests, group_test_setup, NULL);
}