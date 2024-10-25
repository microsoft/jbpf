// Copyright (c) Microsoft Corporation. All rights reserved.
/**
 * Unit Tests for jbpf_rand function
 * This test the following scenarios
 * 1. jbpf_rand returns a positive value
 * 2. jbpf_rand returns different values
 * 3. jbpf_rand returns values within the range [0, RAND_MAX]
 * 4. jbpf_rand returns consistent values with the same seed
 */
#include <assert.h>
#include <stdlib.h>
#include "jbpf_test_lib.h"
#include "jbpf_helper_impl.h"

#define ITERATIONS 10000

static void
test_jbpf_rand_returns_positive_value(void** state)
{
    int result = __jbpf_rand();
    assert(result >= 0); // rand_r should return a non-negative value
}

static void
test_jbpf_rand_returns_different_values(void** state)
{
    int result1 = __jbpf_rand();
    int result2 = __jbpf_rand();
    assert(result1 != result2); // rand_r should return different values
}

static void
test_jbpf_rand_returns_values_within_range(void** state)
{
    int result = __jbpf_rand();
    assert(result >= 0 && result <= RAND_MAX); // rand_r should return values within the range [0, RAND_MAX]
}

static void
test_jbpf_rand_consistent_with_seed(void** state)
{
    __set_seed(1234);
    int result = __jbpf_rand();
    assert(result == 1584308507);
}

int
main(int argc, char** argv)
{
    const jbpf_test tests[] = {
        JBPF_CREATE_TEST(test_jbpf_rand_returns_positive_value, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_rand_returns_different_values, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_rand_returns_values_within_range, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_rand_consistent_with_seed, NULL, NULL, NULL),
    };

    int num_tests = sizeof(tests) / sizeof(jbpf_test);
    return jbpf_run_test(tests, num_tests, NULL, NULL);
}
