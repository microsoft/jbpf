// Copyright (c) Microsoft Corporation. All rights reserved.
/*
    This contains unit tests for JBPF_MAP_TYPE_ARRAY. It tests the following functions:
    - jbpf_create_map
    - jbpf_map_update_elem
    - jbpf_map_lookup_elem
    - jbpf_destroy_map

    It tests the following scenarios:
    - Accessing all elements in the array
    - Accessing a key that does not exist in the array
    - Accessing a single entry array
    - Accessing a key that does not exist in a single entry array
*/

#include <assert.h>
#include "jbpf_memory.h"
#include "jbpf_bitmap.h"
#include "jbpf_test_lib.h"
#include "jbpf_defs.h"
#include "jbpf_helper_impl.h"
#include "jbpf_int.h"

#define TEST_ARRAY_SIZE 10

/*
 * This is run once before all system group tests
 */
static int
system_group_setup(void** state)
{
    struct jbpf_agent_mem_config mem_config;
    mem_config.mem_size = 1024 * 1024;
    jbpf_memory_setup(&mem_config);
    return 0;
}

/*
 * This is run once after all system group tests
 */
static int
system_group_teardown(void** state)
{
    jbpf_memory_teardown();
    return 0;
}

static int
test_setup(void** state)
{
    struct jbpf_load_map_def map_def = {
        .type = JBPF_MAP_TYPE_ARRAY,
        .key_size = sizeof(int),
        .value_size = sizeof(int),
        .max_entries = TEST_ARRAY_SIZE,
    };
    struct jbpf_map* array = __jbpf_create_map("map1", &map_def, NULL);
    for (int i = 0; i < TEST_ARRAY_SIZE; ++i) {
        int key = i;
        int val = i;
        int ret = __jbpf_map_update_elem(array, &key, &val, 0);
        assert(ret == 0);
    }
    *state = array;
    return 0;
}

static void
test_array_access(void** state)
{
    struct jbpf_map* array = (struct jbpf_map*)*state;
    for (int i = 0; i < TEST_ARRAY_SIZE; ++i) {
        int key = i;
        int* val = (int*)__jbpf_map_lookup_elem(array, &key);
        assert(val);
        assert(*val == i);
    }
}

static int
test_teardown(void** state)
{
    struct jbpf_map* array = (struct jbpf_map*)*state;
    __jbpf_destroy_map(array);
    return 0;
}

static int
test_setup_single_entry(void** state)
{
    struct jbpf_load_map_def map_def = {
        .type = JBPF_MAP_TYPE_ARRAY,
        .key_size = sizeof(int),
        .value_size = sizeof(int),
        .max_entries = 1,
    };
    struct jbpf_map* array = __jbpf_create_map("map1", &map_def, NULL);
    int key = 0;
    int val = 999;
    int ret = __jbpf_map_update_elem(array, &key, &val, 0);
    assert(ret == 0);
    *state = array;
    return 0;
}

static void
test_array_access_single_entry(void** state)
{
    struct jbpf_map* array = (struct jbpf_map*)*state;
    int key = 0;
    int* val = (int*)__jbpf_map_lookup_elem(array, &key);
    assert(val);
    assert(*val == 999);
}

static void
test_array_access_out_of_bounds(void** state)
{
    struct jbpf_map* array = (struct jbpf_map*)*state;
    for (int i = 0; i < TEST_ARRAY_SIZE; ++i) {
        int key = i + 9999;
        int* val = (int*)__jbpf_map_lookup_elem(array, &key);
        assert(!val);
    }
}

static void
test_array_access_out_of_bounds_single_entry(void** state)
{
    struct jbpf_map* array = (struct jbpf_map*)*state;
    for (int i = 0; i < 999; ++i) {
        int key = i + 9999;
        int* val = (int*)__jbpf_map_lookup_elem(array, &key);
        assert(!val);
    }
}

int
main(int argc, char** argv)
{
    struct jbpf_map* state;
    const jbpf_test tests[] = {
        JBPF_CREATE_TEST(test_array_access, test_setup, test_teardown, &state),
        JBPF_CREATE_TEST(test_array_access_out_of_bounds, test_setup, test_teardown, &state),
        JBPF_CREATE_TEST(test_array_access_single_entry, test_setup_single_entry, test_teardown, &state),
        JBPF_CREATE_TEST(test_array_access_out_of_bounds_single_entry, test_setup_single_entry, test_teardown, &state),
    };

    int num_tests = sizeof(tests) / sizeof(jbpf_test);
    return jbpf_run_test(tests, num_tests, system_group_setup, system_group_teardown);
}
