// Copyright (c) Microsoft Corporation. All rights reserved.

#include <assert.h>
#include "jbpf_memory.h"
#include "jbpf_bitmap.h"
#include "jbpf_test_lib.h"
#include "jbpf_utils.h"

#define TEST_BITMAP_SIZE 100

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
test_bitmap_setup(void** state)
{
    struct jbpf_bitmap* bitmap;
    bitmap = jbpf_alloc_mem(sizeof(struct jbpf_bitmap));
    jbpf_init_bitmap(bitmap, TEST_BITMAP_SIZE);
    *state = bitmap;
    return 0;
}

static int
test_bitmap_teardown(void** state)
{
    struct jbpf_bitmap* bitmap;
    bitmap = (struct jbpf_bitmap*)*state;
    jbpf_destroy_bitmap(bitmap);
    jbpf_free_mem(bitmap);
    return 0;
}

static void
test_bitmap_free_without_allocate(void** state)
{
    struct jbpf_bitmap* bitmap;
    bitmap = (struct jbpf_bitmap*)*state;
    jbpf_free_bit(bitmap, 1234);
}

static void
test_bitmap_happy_path(void** state)
{
    struct jbpf_bitmap* bitmap;
    bitmap = (struct jbpf_bitmap*)*state;

    int allocated_bit = -1;

    JBPF_UNUSED(allocated_bit);

    int id;
    // First test that we can allocate up to the capacity
    for (id = 0; id < TEST_BITMAP_SIZE; id++) {
        allocated_bit = jbpf_allocate_bit(bitmap);
        assert(id == allocated_bit);
    }

    // Bitmap should be full, so we cannot allocate more
    allocated_bit = jbpf_allocate_bit(bitmap);
    assert(allocated_bit == -1);

    // Remove 2 elements
    int relem1 = 10;
    int relem2 = 20;
    jbpf_free_bit(bitmap, relem1);
    jbpf_free_bit(bitmap, relem2);

    jbpf_free_bit(bitmap, 1010101);
    jbpf_free_bit(bitmap, -100);

    // Add two more elements and they should be in the positions we just removed
    allocated_bit = jbpf_allocate_bit(bitmap);
    assert(relem1 == allocated_bit);
    allocated_bit = jbpf_allocate_bit(bitmap);
    assert(relem2 == allocated_bit);

    // Next one should be -1, because the bitmap is full again
    allocated_bit = jbpf_allocate_bit(bitmap);
    assert(allocated_bit == -1);
}

int
main(int argc, char** argv)
{
    struct jbpf_bitmap* bitmap;
    const jbpf_test tests[] = {
        JBPF_CREATE_TEST(test_bitmap_happy_path, test_bitmap_setup, test_bitmap_teardown, &bitmap),
        JBPF_CREATE_TEST(test_bitmap_free_without_allocate, test_bitmap_setup, test_bitmap_teardown, &bitmap),
    };

    int num_tests = sizeof(tests) / sizeof(jbpf_test);
    return jbpf_run_test(tests, num_tests, system_group_setup, system_group_teardown);
}
