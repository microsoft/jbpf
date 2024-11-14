// Copyright (c) Microsoft Corporation. All rights reserved.
/**
 * Unit Tests for jbpf_hash function
 * This test the following scenarios
 * 1. Hashing an empty item
 * 2. Hashing a single byte
 * 3. Hashing a string
 * 4. Hashing binary data
 * 5. Hashing large data
 * 6. Hashing a non-null pointer with zero size
 * 7. Hashing consistency
 * 8. Hashing different inputs
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "jbpf_test_lib.h"
#include "jbpf_helper_impl.h"
#include "jbpf_utils.h"

static void
test_jbpf_hash_empty_item(void** state)
{
    void* item = NULL;
    uint64_t size = 0;
    uint32_t result = __jbpf_hash(item, size);
    JBPF_UNUSED(result);
    assert(result == 3735928559);
}

static void
test_jbpf_hash_single_byte(void** state)
{
    char item = 'A';
    uint64_t size = 1;
    uint32_t result = __jbpf_hash(&item, size);
    uint32_t expected_hash = 16862113;
    JBPF_UNUSED(expected_hash);
    JBPF_UNUSED(result);
    assert(result == expected_hash);
}

static void
test_jbpf_hash_string(void** state)
{
    char* item = "jbpf";
    uint64_t size = strlen(item); // Hash only the characters of the string
    uint32_t result = __jbpf_hash(item, size);
    uint32_t expected_hash = 2542796865;
    JBPF_UNUSED(expected_hash);
    JBPF_UNUSED(result);
    assert(result == expected_hash);
}

static void
test_jbpf_hash_binary_data(void** state)
{
    unsigned char item[] = {0x01, 0xFF, 0xA5, 0x4B};
    uint64_t size = sizeof(item);
    uint32_t result = __jbpf_hash(item, size);
    uint32_t expected_hash = 370203305;
    JBPF_UNUSED(expected_hash);
    JBPF_UNUSED(result);
    assert(result == expected_hash);
}

static void
test_jbpf_hash_large_data(void** state)
{
    char item[1024];                 // 1 KB of data
    memset(item, 'A', sizeof(item)); // Fill with 'A'
    uint64_t size = sizeof(item);
    uint32_t result = __jbpf_hash(item, size);
    uint32_t expected_hash = 1918934519;
    JBPF_UNUSED(expected_hash);
    JBPF_UNUSED(result);
    assert(result == expected_hash);
}

static void
test_jbpf_hash_non_null_pointer_zero_size(void** state)
{
    char item = 'A';
    uint64_t size = 0;
    uint32_t result = __jbpf_hash(&item, size);
    uint32_t expected_hash = 3735928559;
    JBPF_UNUSED(expected_hash);
    JBPF_UNUSED(result);
    assert(result == expected_hash);
}

static void
test_jbpf_hash_consistency(void** state)
{
    char* item = "jbpf";
    uint64_t size = strlen(item);
    uint32_t result1 = __jbpf_hash(item, size);
    uint32_t result2 = __jbpf_hash(item, size);
    JBPF_UNUSED(result1);
    JBPF_UNUSED(result2);
    assert(result1 == result2); // Hash should be consistent for the same input
}

static void
test_jbpf_hash_different_inputs(void** state)
{
    char* item1 = "jbpf";
    char* item2 = "hash";
    uint64_t size1 = strlen(item1);
    uint64_t size2 = strlen(item2);

    uint32_t result1 = __jbpf_hash(item1, size1);
    uint32_t result2 = __jbpf_hash(item2, size2);
    JBPF_UNUSED(result1);
    JBPF_UNUSED(result2);
    assert(result1 != result2); // Hashes should differ for different inputs
}

int
main(int argc, char** argv)
{
    const jbpf_test tests[] = {
        JBPF_CREATE_TEST(test_jbpf_hash_empty_item, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_hash_single_byte, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_hash_string, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_hash_binary_data, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_hash_large_data, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_hash_non_null_pointer_zero_size, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_hash_consistency, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_hash_different_inputs, NULL, NULL, NULL)};

    int num_tests = sizeof(tests) / sizeof(jbpf_test);
    return jbpf_run_test(tests, num_tests, NULL, NULL);
}
