// Copyright (c) Microsoft Corporation. All rights reserved.

#include "jbpf_test_lib.h"
#include <stdio.h>
#include <string.h>

jbpf_test
__jbpf_create_test(
    jbpf_test_function_t test_func,
    jbpf_setup_function_t setup_func,
    jbpf_teardown_function_t teardown_func,
    void* initial_state,
    char* name)
{
    jbpf_test test;
    strcpy(test.name, name);
    test.test_func = test_func;
    test.setup_func = setup_func;
    test.teardown_func = teardown_func;
    test.initial_state = initial_state;
    return test;
}

int
jbpf_run_test(
    const jbpf_test* tests,
    int num_tests,
    jbpf_setup_function_t group_setup_func,
    jbpf_teardown_function_t group_teardown_func)
{
    int i;
    int ret = 0;
    void* state;
    if (group_setup_func) {
        ret = group_setup_func(&state);
        if (ret != 0) {
            return ret;
        }
    }
    for (i = 0; i < num_tests; i++) {
        state = tests[i].initial_state;
        if (tests[i].setup_func) {
            printf("Running setup for test %s ... ", tests[i].name);
            ret = tests[i].setup_func(&state);
            if (ret != 0) {
                printf("Failed with ret=%d\n", ret);
                return ret;
            }
            printf("Passed!\n");
        }
        printf("Running test %s ... ", tests[i].name);
        tests[i].test_func(&state);
        printf("Passed!\n");
        if (tests[i].teardown_func) {
            printf("Running teardown for test %s ... ", tests[i].name);
            ret = tests[i].teardown_func(&state);
            if (ret != 0) {
                printf("Failed with ret=%d\n", ret);
                return ret;
            }
            printf("Passed!\n");
        }
    }
    if (group_teardown_func) {
        ret = group_teardown_func(&state);
        if (ret != 0) {
            return ret;
        }
    }
    return 0;
}