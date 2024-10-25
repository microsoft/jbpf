// Copyright (c) Microsoft Corporation. All rights reserved.

#include <unistd.h>
#include <assert.h>
#include "jbpf.h"
#include "jbpf_int.h"
#include "jbpf_hook.h"
#include "jbpf_defs.h"
#include "jbpf_test_def.h"
#include "jbpf_memory.h"
#include "jbpf_bpf_array.h"
#include "jbpf_bpf_hashmap.h"
#include "jbpf_utils.h"
#include "jbpf_bpf_hashmap_int.h"
#include "jbpf_test_lib.h"

#define MAX_ITER 2000

struct hashmap_complex_val
{
    int valA;
    void* valB;
};

struct jbpf_config* jconfig;

DECLARE_JBPF_HOOK(
    test1,
    struct jbpf_generic_ctx ctx,
    ctx,
    HOOK_PROTO(struct packet* p, int ctx_id),
    HOOK_ASSIGN(ctx.ctx_id = ctx_id; ctx.data = (uint64_t)(void*)p; ctx.data_end = (uint64_t)(void*)(p + 1);))

DECLARE_JBPF_HOOK(
    test2,
    struct jbpf_generic_ctx ctx,
    ctx,
    HOOK_PROTO(struct packet* p, int ctx_id),
    HOOK_ASSIGN(ctx.ctx_id = ctx_id; ctx.data = (uint64_t)(void*)p; ctx.data_end = (uint64_t)(void*)(p + 1);))

DEFINE_JBPF_HOOK(test1)
DEFINE_JBPF_HOOK(test2)

struct hashmap_ctx
{
    int a;
    int b;
};

/* Returns true if element is to be kept */
bool
keep_item(void* key, void* value, void* args)
{

    struct hashmap_ctx* ctx = (struct hashmap_ctx*)args;

    int val = *(int*)value;

    if (val == ctx->a || val == ctx->b)
        return true;

    return false;
}

bool
keep_item_no_ctx(void* key, void* value, void* args)
{

    int val = *(int*)value;

    if (val == 200 || val == 300)
        return true;

    return false;
}

/*
 * This is run once before all unit group tests
 */
static int
unit_group_setup(void** state)
{
    int ret;
    struct jbpf_config* jbpf_config = (struct jbpf_config*)malloc(sizeof(struct jbpf_config));
    *state = jbpf_config;
    jbpf_set_default_config_options(jbpf_config);
    jbpf_config->lcm_ipc_config.has_lcm_ipc_thread = false;
    ret = jbpf_init(jbpf_config);
    jbpf_register_thread();
    sleep(1);
    return ret;
}

/*
 * This is run once after all unit group tests
 */
static int
unit_group_teardown(void** state)
{
    jbpf_stop();
    free(*state);
    return 0;
}

// static int
// deallocate_maps_teardown(void** state)
// {
//     struct jbpf_ctx_t* jbpf_ctx = jbpf_get_ctx();
//     reset_map_registry(jbpf_ctx, false);
//     return 0;
// }

// static void
// allocate_and_free_all_maps(void** state)
// {
//     struct jbpf_map* map[MAX_NUM_MAPS];
//     struct jbpf_map* failed_map;
//     struct jbpf_load_map_def map_def;
//     int i;
//     char map_name[5] = "test";
//     map_def.type = JBPF_MAP_TYPE_ARRAY;
//     map_def.key_size = 4;
//     map_def.value_size = 8;
//     map_def.max_entries = 20;
//     for (i = 0; i < MAX_NUM_MAPS; i++) {
//         map[i] = jbpf_create_map(map_name, &map_def, NULL);
//         assert(map[i] != NULL);
//     }

//     /* It should be impossible to create more maps */
//     failed_map = jbpf_create_map(map_name, &map_def, NULL);
//     assert(failed_map == NULL);
// }

// static void
// allocate_maps_and_free_reuse_should_be_ok(void** state)
// {
//     struct jbpf_map* map[MAX_NUM_MAPS];
//     struct jbpf_load_map_def map_def;
//     struct jbpf_ctx_t* jbpf_ctx;
//     int i;
//     char map_name[5] = "test";
//     map_def.type = JBPF_MAP_TYPE_ARRAY;
//     map_def.key_size = 4;
//     map_def.value_size = 8;
//     map_def.max_entries = 20;

//     jbpf_ctx = jbpf_get_ctx();

//     // allocate all maps
//     for (i = 0; i < MAX_NUM_MAPS; i++) {
//         map[i] = create_jbpf_map(&map_def, map_name, sizeof(map_name));
//         assert(map[i] != NULL);
//     }

//     // cleanup 50%
//     for (i = 0; i < MAX_NUM_MAPS; i += 2) {
//         jbpf_release_map(jbpf_ctx, map[i]->fd);
//     }

//     // should be ok to allocate half
//     for (i = 0; i < MAX_NUM_MAPS; i += 2) {
//         map[i] = create_jbpf_map(&map_def, map_name, sizeof(map_name));
//         assert(map[i] != NULL);
//     }

//     // cleanup
//     for (i = 0; i < MAX_NUM_MAPS; i++) {
//         jbpf_release_map(jbpf_ctx, map[i]->fd);
//     }
// }

static int
test_array_map_setup(void** state)
{
    struct jbpf_load_map_def map_def;
    struct jbpf_map* map;
    char map_name[5] = "test";

    map_def.type = JBPF_MAP_TYPE_ARRAY;
    map_def.key_size = sizeof(int);
    map_def.value_size = sizeof(long);
    map_def.max_entries = 20;

    map = jbpf_create_map(map_name, &map_def, NULL);

    if (!map)
        return -1;

    *state = map;

    return 0;
}

static int
test_array_map_teardown(void** state)
{
    struct jbpf_map* map;
    struct jbpf_ctx_t* jbpf_ctx;

    jbpf_ctx = jbpf_get_ctx();

    map = *state;
    jbpf_release_map(jbpf_ctx, map->fd);

    return 0;
}

static int
test_hashmap_map_setup(void** state)
{
    struct jbpf_load_map_def map_def;
    struct jbpf_map* map;
    char map_name[13] = "hashmap_test";

    map_def.type = JBPF_MAP_TYPE_HASHMAP;
    map_def.key_size = sizeof(int);
    map_def.value_size = sizeof(long);
    map_def.max_entries = 4;

    map = create_jbpf_map(&map_def, map_name, sizeof(map_name));
    if (!map)
        return -1;

    *state = map;

    return 0;
}

static int
test_hashmap_map_teardown(void** state)
{
    struct jbpf_map* map;
    struct jbpf_ctx_t* jbpf_ctx;

    jbpf_ctx = jbpf_get_ctx();

    map = *state;
    jbpf_release_map(jbpf_ctx, map->fd);

    return 0;
}

static void
test_array_map(void** state)
{
    struct jbpf_map* map;
    int index1 = 19, index2 = 23, index3 = -1;
    long val1 = 100, val2 = 200;
    int res;
    void* val;

    map = *state;

    res = jbpf_bpf_array_update_elem(map, &index1, &val1, 0);
    assert(res == 0);

    val = jbpf_bpf_array_lookup_elem(map, &index1);
    assert(val != NULL);
    assert(*(long*)val == val1);

    res = jbpf_bpf_array_update_elem(map, &index2, &val2, 0);
    assert(res == -1);

    val = jbpf_bpf_array_lookup_elem(map, &index2);
    assert(val == NULL);

    res = jbpf_bpf_array_update_elem(map, &index3, &val2, 0);
    assert(res == -1);
}

static void
test_array_map_clear(void** state)
{
    struct jbpf_map* map;
    int index1 = 19, index2 = 5;
    long val1 = 100, val2 = 200;
    int res;
    void* val;

    map = *state;

    res = jbpf_bpf_array_update_elem(map, &index1, &val1, 0);
    assert(res == 0);

    res = jbpf_bpf_array_update_elem(map, &index2, &val2, 0);
    assert(res == 0);

    val = jbpf_bpf_array_lookup_elem(map, &index1);
    assert(*(long*)val == val1);

    val = jbpf_bpf_array_lookup_elem(map, &index2);
    assert(*(long*)val == val2);

    res = jbpf_bpf_array_clear(map);
    assert(res == 0);

    val = jbpf_bpf_array_lookup_elem(map, &index1);
    assert(*(long*)val == 0);

    val = jbpf_bpf_array_lookup_elem(map, &index2);
    assert(*(long*)val == 0);
}

static void
test_mempool(void** state)
{
    jbpf_mempool_ctx_t* ctx;
    uintptr_t* data;
    uintptr_t no_data;
    ctx = jbpf_init_data_mempool(5, 20);

    int mempool_size = jbpf_get_data_mempool_size(ctx);

    data = jbpf_alloc_mem(mempool_size * sizeof(void*));

    // All allocations should succeed here
    for (int i = 0; i < mempool_size; i++) {
        data[i] = (uintptr_t)jbpf_alloc_data_mem(ctx);
        assert(data != NULL);
    }

    // This allocation should fail, as we have ran out of memory
    no_data = (uintptr_t)jbpf_alloc_data_mem(ctx);
    assert(no_data == 0);

    // We free 2 entries
    jbpf_free_data_mem((void*)data[0]);
    jbpf_free_data_mem((void*)data[1]);
    data[0] = (uintptr_t)NULL;
    data[1] = (uintptr_t)NULL;

    // Now we should be able to allocate to more again
    data[0] = (uintptr_t)jbpf_alloc_data_mem(ctx);
    data[1] = (uintptr_t)jbpf_alloc_data_mem(ctx);
    no_data = (uintptr_t)jbpf_alloc_data_mem(ctx);
    assert(data[0] != 0);
    assert(data[1] != 0);
    assert(no_data == 0);

    assert(ctx != NULL);
    for (int i = 0; i < mempool_size; i++) {
        jbpf_free_data_mem((void*)data[i]);
    }

    jbpf_free_mem(data);

    jbpf_destroy_data_mempool(ctx);
}

static void
test_hashmap_map(void** state)
{
    struct jbpf_map* map;
    int key1 = 19, key2 = 23, key3 = -1, key5 = 5;
    char* key4 = "test_key";
    long val1 = 100, val2 = 200, val3 = 300, val4 = 400, val5 = 500;
    int res;
    void* val;

    map = *state;

    res = jbpf_bpf_hashmap_update_elem(map, &key1, &val1, 0);
    assert(res == 0);

    /* Add the same element again */
    res = jbpf_bpf_hashmap_update_elem(map, &key1, &val1, 0);
    assert(res == 0);
    assert(jbpf_bpf_hashmap_size(map) == 1);

    res = jbpf_bpf_hashmap_update_elem(map, &key2, &val2, 0);
    assert(res == 0);
    assert(jbpf_bpf_hashmap_size(map) == 2);

    res = jbpf_bpf_hashmap_update_elem(map, &key3, &val3, 0);
    assert(res == 0);
    assert(jbpf_bpf_hashmap_size(map) == 3);

    res = jbpf_bpf_hashmap_update_elem(map, key4, &val4, 0);
    assert(res == 0);
    assert(jbpf_bpf_hashmap_size(map) == 4);

    /* The map should be full by now */
    res = jbpf_bpf_hashmap_update_elem(map, &key5, &val5, 0);
    assert(res == -4);

    /* Check that all the pairs are in the map */
    val = jbpf_bpf_hashmap_lookup_elem(map, &key1);
    assert(val != 0);
    assert(*(long*)val == val1);

    val = jbpf_bpf_hashmap_lookup_elem(map, &key2);
    assert(val != 0);
    assert(*(long*)val == val2);

    val = jbpf_bpf_hashmap_lookup_elem(map, &key3);
    assert(val != 0);
    assert(*(long*)val == val3);

    val = jbpf_bpf_hashmap_lookup_elem(map, key4);
    assert(val != 0);
    assert(*(long*)val == val4);

    /* key4 should be the same as "test", since key size is 4 */
    val = jbpf_bpf_hashmap_lookup_elem(map, "test");
    assert(val != NULL);
    assert(*(long*)val == val4);

    /* Delete an existing key */
    res = jbpf_bpf_hashmap_delete_elem(map, &key1);
    assert(res == 0);
    assert(jbpf_bpf_hashmap_size(map) == 3);

    /* Lookup and delete a non-existent pair */
    val = jbpf_bpf_hashmap_lookup_elem(map, &key5);
    assert(val == NULL);

    res = jbpf_bpf_hashmap_delete_elem(map, &key5);
    assert(res == -1);
}

static void
test_hashmap_map_dump(void** state)
{
    struct jbpf_map* map;
    int key1 = 19, key2 = 23, key3 = -1, key4 = -10;
    long val1 = 100, val2 = 200, val3 = 300, val4 = 400;
    int res;
    // void *val;

    char reg1[100];
    char reg2[2];

    map = *state;

    res = jbpf_bpf_hashmap_update_elem(map, &key1, &val1, 0);
    assert(res == 0);

    /* Add the same element again */
    res = jbpf_bpf_hashmap_update_elem(map, &key1, &val1, 0);
    assert(res == 0);
    assert(jbpf_bpf_hashmap_size(map) == 1);

    res = jbpf_bpf_hashmap_update_elem(map, &key2, &val2, 0);
    assert(res == 0);
    assert(jbpf_bpf_hashmap_size(map) == 2);

    res = jbpf_bpf_hashmap_update_elem(map, &key3, &val3, 0);
    assert(res == 0);
    assert(jbpf_bpf_hashmap_size(map) == 3);

    res = jbpf_bpf_hashmap_update_elem(map, &key4, &val4, 0);
    assert(res == 0);
    assert(jbpf_bpf_hashmap_size(map) == 4);

    /* There is enough space, so all elements should be dumped */
    res = jbpf_bpf_hashmap_dump(map, reg1, sizeof(reg1), 0);
    assert(res == 4);

    /* Check the dump of data */
    int* key;
    long* value;
    int offset = sizeof(int) + sizeof(long);

    int num_val1 = 1, num_val2 = 1, num_val3 = 1, num_val4 = 1;

    for (int i = 0; i < 4; i++) {
        key = (int*)((uint8_t*)reg1 + (i * offset));
        value = (long*)((uint8_t*)reg1 + (sizeof(int) + (i * offset)));
        switch (*key) {
        case 19:
            assert(*value == val1);
            num_val1--;
            break;
        case 23:
            assert(*value == val2);
            num_val2--;
            break;
        case -1:
            assert(*value == val3);
            num_val3--;
            break;
        case -10:
            assert(*value == val4);
            num_val4--;
            break;
        default:
            assert(false);
        }
    }

    /* Make sure that we only got each value exactly once*/
    assert(num_val1 == 0);
    assert(num_val2 == 0);
    assert(num_val3 == 0);
    assert(num_val4 == 0);

    /* There is not enough space, so nothing should be dumped */
    res = jbpf_bpf_hashmap_dump(map, reg2, sizeof(reg2), 0);
    assert(res == 0);

    /* Dump again */
    /* There is enough space, so all elements should be dumped */
    res = jbpf_bpf_hashmap_dump(map, reg1, sizeof(reg1), 0);
    assert(res == 4);
}

static void
test_hashmap_map_clear(void** state)
{
    struct jbpf_map* map;
    int key1 = 19, key2 = 23, key3 = -1, key4 = -10;
    long val1 = 100, val2 = 200, val3 = 300, val4 = 400;
    int res;

    map = *state;

    res = jbpf_bpf_hashmap_update_elem(map, &key1, &val1, 0);
    assert(res == 0);

    res = jbpf_bpf_hashmap_update_elem(map, &key2, &val2, 0);
    assert(res == 0);
    assert(jbpf_bpf_hashmap_size(map) == 2);

    res = jbpf_bpf_hashmap_update_elem(map, &key3, &val3, 0);
    assert(res == 0);
    assert(jbpf_bpf_hashmap_size(map) == 3);

    res = jbpf_bpf_hashmap_update_elem(map, &key4, &val4, 0);
    assert(res == 0);
    assert(jbpf_bpf_hashmap_size(map) == 4);

    res = jbpf_bpf_hashmap_clear(map);
    assert(res == 0);
    assert(jbpf_bpf_hashmap_size(map) == 0);
}

int
main(int argc, char** argv)
{
    struct jbpf_map* array_map;
    struct jbpf_map* hashmap_map;

    const jbpf_test jbpf_unit_tests[] = {
        // JBPF_CREATE_TEST(allocate_maps_and_free_reuse_should_be_ok, NULL, deallocate_maps_teardown, NULL),
        // JBPF_CREATE_TEST(allocate_and_free_all_jbpf_vms, NULL, deallocate_jbpf_vms_teardown, NULL),
        // JBPF_CREATE_TEST(exceed_max_vm_allocation, NULL, deallocate_jbpf_vms_teardown, NULL),
        // JBPF_CREATE_TEST(allocate_and_free_all_maps, NULL, deallocate_maps_teardown, NULL),
        JBPF_CREATE_TEST(test_mempool, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_array_map, test_array_map_setup, test_array_map_teardown, &array_map),
        JBPF_CREATE_TEST(test_array_map_clear, test_array_map_setup, test_array_map_teardown, &array_map),
        JBPF_CREATE_TEST(test_hashmap_map, test_hashmap_map_setup, test_hashmap_map_teardown, &hashmap_map),
        JBPF_CREATE_TEST(test_hashmap_map_dump, test_hashmap_map_setup, test_hashmap_map_teardown, &hashmap_map),
        JBPF_CREATE_TEST(test_hashmap_map_clear, test_hashmap_map_setup, test_hashmap_map_teardown, &hashmap_map),
    };

    int num_tests = sizeof(jbpf_unit_tests) / sizeof(jbpf_test);
    return jbpf_run_test(jbpf_unit_tests, num_tests, unit_group_setup, unit_group_teardown);
}
