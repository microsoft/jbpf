// Copyright (c) Microsoft Corporation. All rights reserved.
/*
    This contains unit tests for JBPF_MAP_TYPE_HASHMAP. It tests the following functions:
    - jbpf_create_map
    - jbpf_map_update_elem
    - jbpf_map_lookup_elem
    - jbpf_destroy_map
    - jbpf_map_delete_elem

    It tests the following scenarios:
    - Accessing all elements in the hashmap
    - Accessing a key that does not exist in the hashmap
    - Accessing a single entry hashmap
    - Accessing a key that does not exist in a single entry hashmap
    - Removing a key that does not exist in the hashmap
    - Adding a key to a full hashmap (single entry and multiple entries)
    - Removing a key that exists in the hashmap and then checking if it exists
*/

#include <assert.h>
#include "jbpf_memory.h"
#include "jbpf_test_lib.h"
#include "jbpf_defs.h"
#include "jbpf_helper_impl.h"
#include "jbpf_int.h"
#include "jbpf_bpf_hashmap.h"
#include "jbpf_utils.h"

#define TEST_HASHMAP_SIZE 1000

/*
 * This is run once before all system group tests
 */
static int
system_group_setup(void** state)
{
    struct jbpf_agent_mem_config mem_config;
    mem_config.mem_size = 1024 * 1024 * 1024;
    __test_setup();
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
        .type = JBPF_MAP_TYPE_HASHMAP,
        .key_size = sizeof(int),
        .value_size = sizeof(int),
        .max_entries = TEST_HASHMAP_SIZE,
    };
    struct jbpf_map* hashmap = __jbpf_create_map("map1", &map_def, NULL);
    for (int i = 0; i < TEST_HASHMAP_SIZE; ++i) {
        int key = i;
        int val = i;
        int ret = __jbpf_map_update_elem(hashmap, &key, &val, 0);
        JBPF_UNUSED(ret);
        assert(ret == 0);
    }
    *state = hashmap;
    return 0;
}

static int
test_setup_full_map_single(void** state)
{
    struct jbpf_load_map_def map_def = {
        .type = JBPF_MAP_TYPE_HASHMAP,
        .key_size = sizeof(int),
        .value_size = sizeof(int),
        .max_entries = 1,
    };
    struct jbpf_map* hashmap = __jbpf_create_map("map1", &map_def, NULL);
    int key = 0;
    int val = 0;
    int ret = __jbpf_map_update_elem(hashmap, &key, &val, 0);
    JBPF_UNUSED(ret);
    assert(ret == 0);
    *state = hashmap;
    return 0;
}

static int
test_setup_full_map(void** state)
{
    struct jbpf_load_map_def map_def = {
        .type = JBPF_MAP_TYPE_HASHMAP,
        .key_size = sizeof(int),
        .value_size = sizeof(int),
        .max_entries = TEST_HASHMAP_SIZE,
    };
    struct jbpf_map* hashmap = __jbpf_create_map("map1", &map_def, NULL);
    for (int i = 0; i < TEST_HASHMAP_SIZE; ++i) {
        int key = i;
        int val = i;
        int ret = __jbpf_map_update_elem(hashmap, &key, &val, 0);
        JBPF_UNUSED(ret);
        assert(ret == 0);
    }
    *state = hashmap;
    return 0;
}

static void
test_hashmap_access(void** state)
{
    struct jbpf_map* hashmap = (struct jbpf_map*)*state;
    for (int i = 0; i < TEST_HASHMAP_SIZE; ++i) {
        int key = i;
        int* val = (int*)__jbpf_map_lookup_elem((struct jbpf_map*)hashmap, &key);
        JBPF_UNUSED(val);
        assert(val);
        assert(*val == i);
    }
}

static int
test_teardown(void** state)
{
    struct jbpf_map* hashmap = (struct jbpf_map*)*state;
    __jbpf_destroy_map(hashmap);
    return 0;
}

static int
test_setup_single_entry(void** state)
{
    struct jbpf_load_map_def map_def = {
        .type = JBPF_MAP_TYPE_HASHMAP,
        .key_size = sizeof(int),
        .value_size = sizeof(int),
        .max_entries = 1,
    };
    struct jbpf_map* hashmap = __jbpf_create_map("map1", &map_def, NULL);
    int key = 0;
    int val = 999;
    int ret = __jbpf_map_update_elem((struct jbpf_map*)hashmap, &key, &val, 0);
    JBPF_UNUSED(ret);
    assert(ret == 0);
    *state = hashmap;
    return 0;
}

static void
test_hashmap_access_single_entry(void** state)
{
    struct jbpf_map* hashmap = (struct jbpf_map*)*state;
    int key = 0;
    int* val = (int*)__jbpf_map_lookup_elem((struct jbpf_map*)hashmap, &key);
    JBPF_UNUSED(val);
    assert(val);
    assert(*val == 999);
}

static void
test_hashmap_access_key_not_exist(void** state)
{
    struct jbpf_map* hashmap = (struct jbpf_map*)*state;
    for (int i = 0; i < TEST_HASHMAP_SIZE; ++i) {
        int key = i + 9999;
        int* val = (int*)__jbpf_map_lookup_elem((struct jbpf_map*)hashmap, &key);
        JBPF_UNUSED(val);
        assert(!val);
    }
}

static void
test_hashmap_access_key_not_exist_single_entry(void** state)
{
    struct jbpf_map* hashmap = (struct jbpf_map*)*state;
    for (int i = 0; i < 999; ++i) {
        int key = i + 9999;
        int* val = (int*)__jbpf_map_lookup_elem((struct jbpf_map*)hashmap, &key);
        JBPF_UNUSED(val);
        assert(!val);
    }
}

static void
test_hashmap_remove_key_not_exist(void** state)
{
    struct jbpf_map* hashmap = (struct jbpf_map*)*state;
    for (int i = 0; i < TEST_HASHMAP_SIZE; ++i) {
        int key = i + 999999;
        int ret = __jbpf_map_delete_elem((struct jbpf_map*)hashmap, &key);
        JBPF_UNUSED(ret);
        assert(ret == -1);
    }
}

static void
test_hashmap_add_key_to_a_full_map(void** state)
{
    struct jbpf_map* hashmap = (struct jbpf_map*)*state;
    int key = 9999;
    int val = 9999;
    int ret = __jbpf_map_update_elem((struct jbpf_map*)hashmap, &key, &val, 0);
    JBPF_UNUSED(ret);
    assert(ret == JBPF_MAP_FULL);
}

static void
test_hashmap_remove_key_exist_and_then_check(void** state)
{
    struct jbpf_map* hashmap = (struct jbpf_map*)*state;
    int key = 0;
    int ret = __jbpf_map_delete_elem((struct jbpf_map*)hashmap, &key);
    JBPF_UNUSED(ret);
    assert(ret == 0);
    int* val = (int*)__jbpf_map_lookup_elem((struct jbpf_map*)hashmap, &key);
    JBPF_UNUSED(val);
    assert(!val);
    // then we can add
    int val2 = 9999;
    int ret2 = __jbpf_map_update_elem((struct jbpf_map*)hashmap, &key, &val2, 0);
    JBPF_UNUSED(ret2);
    assert(ret2 == 0);
}

int
main(int argc, char** argv)
{
    struct jbpf_map* state;
    const jbpf_test tests[] = {
        JBPF_CREATE_TEST(test_hashmap_access, test_setup, test_teardown, &state),
        JBPF_CREATE_TEST(test_hashmap_access_key_not_exist, test_setup, test_teardown, &state),
        JBPF_CREATE_TEST(test_hashmap_access_single_entry, test_setup_single_entry, test_teardown, &state),
        JBPF_CREATE_TEST(
            test_hashmap_access_key_not_exist_single_entry, test_setup_single_entry, test_teardown, &state),
        JBPF_CREATE_TEST(test_hashmap_remove_key_not_exist, test_setup, test_teardown, &state),
        JBPF_CREATE_TEST(test_hashmap_add_key_to_a_full_map, test_setup_full_map, test_teardown, &state),
        JBPF_CREATE_TEST(test_hashmap_add_key_to_a_full_map, test_setup_full_map_single, test_teardown, &state),
        JBPF_CREATE_TEST(test_hashmap_remove_key_exist_and_then_check, test_setup_full_map, test_teardown, &state),
    };

    int num_tests = sizeof(tests) / sizeof(jbpf_test);
    return jbpf_run_test(tests, num_tests, system_group_setup, system_group_teardown);
}
