// Copyright (c) Microsoft Corporation. All rights reserved.
/*
  This file includes a few unit tests for the jbpf-io-lib memory management functions.
  - jbpf_calloc
  - jbpf_malloc
  - jbpf_realloc
  - jbpf_free
*/
#include <string.h>
#include <assert.h>
#include "jbpf_mem_mgmt.h"
#include "jbpf_test_lib.h"

// Test for jbpf_calloc
static void
test_jbpf_calloc(void** state)
{
    (void)state; // Unused parameter
    size_t count = 10;
    size_t size = sizeof(int);
    int* array = (int*)jbpf_calloc(count, size);
    assert(array != NULL);
    for (size_t i = 0; i < count; i++) {
        assert(array[i] == 0);
    }
    jbpf_free(array);
}

// Test for jbpf_malloc
static void
test_jbpf_malloc(void** state)
{
    (void)state; // Unused parameter
    size_t size = 1024;
    char* buffer = (char*)jbpf_malloc(size);
    assert(buffer != NULL);
    memset(buffer, 1, size);
    for (size_t i = 0; i < size; i++) {
        assert(buffer[i] == 1);
    }
    jbpf_free(buffer);
}

// Test for jbpf_realloc
static void
test_jbpf_realloc(void** state)
{
    (void)state; // Unused parameter
    size_t initial_size = 1024;
    size_t new_size = 2048;
    char* buffer = (char*)jbpf_malloc(initial_size);
    assert(buffer != NULL);
    memset(buffer, 1, initial_size);
    char* new_buffer = (char*)jbpf_realloc(buffer, new_size);
    assert(new_buffer != NULL);
    for (size_t i = 0; i < initial_size; i++) {
        assert(new_buffer[i] == 1);
    }
    jbpf_free(new_buffer);
}

// Test for jbpf_free
static void
test_jbpf_free(void** state)
{
    (void)state; // Unused parameter
    char* buffer = (char*)jbpf_malloc(1024);
    assert(buffer != NULL);
    jbpf_free(buffer);
    // Note: Testing double free or freeing NULL might not be safe or meaningful in this context
}

int
main(int argc, char** argv)
{
    const jbpf_test tests[] = {
        JBPF_CREATE_TEST(test_jbpf_calloc, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_malloc, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_realloc, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_free, NULL, NULL, NULL),
    };

    int num_tests = sizeof(tests) / sizeof(jbpf_test);
    return jbpf_run_test(tests, num_tests, NULL, NULL);
}