
// Copyright (c) Microsoft Corporation. All rights reserved.
/**
 * Unit tests for the following functions:
 * - __jbpf_runtime_limit_exceeded
 * - __jbpf_mark_runtime_init
 *
 * This tests the following scenarios:
 * 1. Set the runtime threshold to 1 second and check if the runtime limit is exceeded before and after the threshold.
 * 2. Set the runtime threshold to 500 ns and check if the runtime limit is exceeded after 1 second consistently.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "jbpf_test_lib.h"
#include "jbpf_helper_impl.h"
#include "jbpf_perf.h"

static int
test_setup(void** state)
{
    __jbpf_mark_runtime_init();
    _jbpf_calibrate_ticks();
    return 0;
}

static void
test_runtime_threshold_before_and_after(void** state)
{
    // set threshold to 1 second
    __jbpf_set_e_runtime_threshold(1000 * 1000 * 1000);
    assert(__jbpf_runtime_limit_exceeded() == 0);
    assert(__jbpf_runtime_limit_exceeded() == 0);
    assert(__jbpf_runtime_limit_exceeded() == 0);
    sleep(1);
    assert(__jbpf_runtime_limit_exceeded() == 1);
}

static void
test_runtime_threshold_consistent_passed(void** state)
{
    __jbpf_set_e_runtime_threshold(500);
    sleep(1);
    assert(__jbpf_runtime_limit_exceeded() == 1);
    assert(__jbpf_runtime_limit_exceeded() == 1);
}

int
main(int argc, char** argv)
{
    const jbpf_test tests[] = {
        JBPF_CREATE_TEST(test_runtime_threshold_before_and_after, test_setup, NULL, NULL),
        JBPF_CREATE_TEST(test_runtime_threshold_consistent_passed, test_setup, NULL, NULL),
    };

    int num_tests = sizeof(tests) / sizeof(jbpf_test);
    return jbpf_run_test(tests, num_tests, NULL, NULL);
}
