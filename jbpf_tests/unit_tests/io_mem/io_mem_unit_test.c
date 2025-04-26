// Copyright (c) Microsoft Corporation. All rights reserved.
/*
    This file contains unit tests for the following functions:
    - round_up_pow_of_two
    - jbpf_io_queue_create
    - jbpf_io_queue_reserve
    - jbpf_io_queue_release
    - jbpf_io_queue_dequeue
    - jbpf_io_queue_get_elem_size
    - _jbpf_io_tohex_str
    - _jbpf_round_up_mem
    - jbpf_get_mempool_size
    - _jbpf_io_ipc_parse_addr
*/
#include <assert.h>

#include "jbpf_io_queue_int.h"
#include "jbpf_io_queue.h"
#include "jbpf_io_defs.h"
#include "jbpf_mempool.h"
#include "jbpf_mempool_int.h"
#include "jbpf_mem_mgmt.h"
#include "jbpf_mem_mgmt_int.h"
#include "jbpf_io_thread_mgmt.h"
#include "jbpf_io_utils.h"
#include "jbpf_mem_mgmt_utils.h"
#include "jbpf_io_ipc_int.h"
#include "jbpf_test_lib.h"
#include "jbpf_io_utils.h"

static jbpf_mem_ctx_t* mem_ctx;
static jbpf_mempool_t* mempool;

#define MEM_META_PATH "/tmp/jbpf_unit_test"

void
test_round_up_pow_of_two(void** state)
{
    assert(round_up_pow_of_two(0) == 0);
    assert(round_up_pow_of_two(1) == 1);
    assert(round_up_pow_of_two(2) == 2);
    assert(round_up_pow_of_two(3) == 4);
    assert(round_up_pow_of_two(5) == 8);
    assert(round_up_pow_of_two(16) == 16);
    assert(round_up_pow_of_two(17) == 32);
    assert(round_up_pow_of_two(1023) == 1024);
    assert(round_up_pow_of_two(1025) == 2048);
}

static int
group_setup(void** state)
{
    (void)state; /* unused */
    mem_ctx = jbpf_create_mem_ctx(4 * 1024 * 1024, MEM_META_PATH, "jbpf-io-lib-unit-tests", 0);
    assert(mem_ctx != NULL);
    mempool = jbpf_init_mempool_ctx(mem_ctx, 100, 512, JBPF_MEMPOOL_MP);
    return 0;
}

static int
group_teardown(void** state)
{
    (void)state; /* unused */
    jbpf_destroy_mempool(mempool);
    jbpf_destroy_mem_ctx(mem_ctx, MEM_META_PATH);
    return 0;
}

static void
test_jbpf_io_queue_create_valid_parameters(void** state)
{
    (void)state; /* unused */
    struct jbpf_io_queue_ctx* result;
    result = jbpf_io_queue_create(10, sizeof(int), JBPF_IO_CHANNEL_OUTPUT, mem_ctx);
    assert(result != NULL);
    jbpf_io_queue_free(result);
}

static void
test_jbpf_io_queue_reserve_null_ctx(void** state)
{
    JBPF_IO_UNUSED(state); /* unused */
    void* result = jbpf_io_queue_reserve(NULL);
    JBPF_IO_UNUSED(result);
    assert(result == NULL);
}

static void
test_jbpf_io_queue_release_null(void** state)
{
    JBPF_IO_UNUSED(state); /* unused */
    // Expect no crashes or errors
    jbpf_io_queue_release(NULL, false);
}

static void
test_jbpf_io_queue_dequeue_null_ctx(void** state)
{
    JBPF_IO_UNUSED(state); /* unused */
    void* result = jbpf_io_queue_dequeue(NULL);
    JBPF_IO_UNUSED(result);
    assert(result == NULL);
}

static void
test_jbpf_io_queue_get_elem_size_valid(void** state)
{
    JBPF_IO_UNUSED(state); /* unused */
    struct jbpf_io_queue_ctx* ioq_ctx = jbpf_io_queue_create(10, sizeof(int), JBPF_IO_CHANNEL_OUTPUT, mem_ctx);
    int elem_size = jbpf_io_queue_get_elem_size(ioq_ctx);
    JBPF_IO_UNUSED(elem_size);
    assert(elem_size == sizeof(int));
    jbpf_io_queue_free(ioq_ctx);
}

static void
test_jbpf_io_queue_get_elem_size_null_ctx(void** state)
{
    JBPF_IO_UNUSED(state); /* unused */
    int elem_size = jbpf_io_queue_get_elem_size(NULL);
    JBPF_IO_UNUSED(elem_size);
    assert(elem_size == -1);
}

static void
test_jbpf_io_tohex_str_valid_input(void** state)
{
    JBPF_IO_UNUSED(state); /* unused */
    char input[] = "hello";
    size_t insz = sizeof(input) - 1;
    char output[11]; // 2 * length of "hello" + 1 for null terminator
    size_t outsz = sizeof(output);
    int result = _jbpf_io_tohex_str(input, insz, output, outsz);
    JBPF_IO_UNUSED(result);
    assert(result == 0);
    assert(strcmp(output, "68656c6c6f") == 0); // Expected hex representation of "hello"
}

static void
test_jbpf_io_tohex_str_null_input(void** state)
{
    JBPF_IO_UNUSED(state); /* unused */
    char output[10];
    int result = _jbpf_io_tohex_str(NULL, 5, output, sizeof(output));
    JBPF_IO_UNUSED(result);
    assert(result == -1);
}

static void
test_jbpf_io_tohex_str_null_output(void** state)
{
    JBPF_IO_UNUSED(state); /* unused */
    char input[] = "test";
    int result = _jbpf_io_tohex_str(input, sizeof(input) - 1, NULL, 10);
    JBPF_IO_UNUSED(result);
    assert(result == -1);
}

static void
test_jbpf_io_tohex_str_insufficient_output_size(void** state)
{
    char input[] = "test";
    char output[5]; // Intentionally too small
    int result = _jbpf_io_tohex_str(input, sizeof(input) - 1, output, sizeof(output));
    JBPF_IO_UNUSED(result);
    assert(result == -1);
}

static void
test_jbpf_io_tohex_str_empty_input(void** state)
{
    JBPF_IO_UNUSED(state); /* unused */
    char input[] = "";
    char output[1];
    int result = _jbpf_io_tohex_str(input, sizeof(input) - 1, output, sizeof(output));
    JBPF_IO_UNUSED(result);
    assert(result == 0);
    assert(strlen(output) == 0); // Expect empty string
}

static void
test_normal_case(void** state)
{
    JBPF_IO_UNUSED(state); /* unused */
    size_t result = _jbpf_round_up_mem(10, 4);
    JBPF_IO_UNUSED(result);
    assert(result == 12);
}

static void
test_already_a_multiple(void** state)
{
    JBPF_IO_UNUSED(state); /* unused */
    size_t result = _jbpf_round_up_mem(12, 4);
    JBPF_IO_UNUSED(result);
    assert(result == 12);
}

static void
test_zero_multiple_size(void** state)
{
    JBPF_IO_UNUSED(state); /* unused */
    size_t result = _jbpf_round_up_mem(10, 0);
    JBPF_IO_UNUSED(result);
    assert(result == 10);
}

static void
test_zero_original_size(void** state)
{
    JBPF_IO_UNUSED(state); /* unused */
    size_t result = _jbpf_round_up_mem(0, 4);
    JBPF_IO_UNUSED(result);
    assert(result == 0);
}

static void
test_both_zero(void** state)
{
    JBPF_IO_UNUSED(state); /* unused */
    size_t result = _jbpf_round_up_mem(0, 0);
    JBPF_IO_UNUSED(result);
    assert(result == 0);
}

// Allocate and check ref_cnt and free mempool size
// Then free and check mempool size
static void
test_jbpf_get_mempool_size_valid(void** state)
{
    JBPF_IO_UNUSED(state); /* unused */
    jbpf_mbuf_t** mbuf;
    int size = jbpf_get_mempool_capacity(mempool);
    assert(size > 0);
    mbuf = malloc(size * sizeof(jbpf_mbuf_t));
    for (int i = 0; i < size; i++) {
        mbuf[i] = jbpf_mbuf_alloc(mempool);
        assert(mbuf != NULL);
        assert(mbuf[i]->ref_cnt == 1);
        __assert__(jbpf_get_mempool_size(mempool) == size - i - 1);
    }
    for (int i = 0; i < size; i++) {
        jbpf_mbuf_free(mbuf[i], false);
        assert(mbuf[i]->ref_cnt == 0);
        __assert__(jbpf_get_mempool_size(mempool) == i + 1);
    }
    free(mbuf);
}

// Allocate, shared, check ref_cnt and free mempool size
// Then free and check mempool size
static void
test_jbpf_get_mempool_size_valid_shared(void** state)
{
    JBPF_IO_UNUSED(state); /* unused */
    jbpf_mbuf_t** mbuf;
    jbpf_mbuf_t** shared_mbuf;
    int size = jbpf_get_mempool_capacity(mempool);
    JBPF_IO_UNUSED(size);
    assert(size > 0);
    mbuf = malloc(size * sizeof(jbpf_mbuf_t));
    shared_mbuf = malloc(size * sizeof(jbpf_mbuf_t));
    for (int i = 0; i < size; i++) {
        mbuf[i] = jbpf_mbuf_alloc(mempool);
        shared_mbuf[i] = jbpf_mbuf_share(mbuf[i]);
        assert(mbuf != NULL);
        assert(shared_mbuf[i] != NULL);
        assert(mbuf[i]->ref_cnt == 2);
        __assert__(jbpf_get_mempool_size(mempool) == size - i - 1);
    }
    // free one by one - ref cnt should be 1
    for (int i = 0; i < size; i++) {
        jbpf_mbuf_free(mbuf[i], false);
        assert(mbuf[i]->ref_cnt == 1);
        __assert__(jbpf_get_mempool_size(mempool) == 0);
    }
    // free one by one - ref cnt should be 0, and size should update
    for (int i = 0; i < size; i++) {
        jbpf_mbuf_free(shared_mbuf[i], false);
        assert(shared_mbuf[i]->ref_cnt == 0);
        __assert__(jbpf_get_mempool_size(mempool) == i + 1);
    }
    free(mbuf);
    free(shared_mbuf);
}

// Test function for NULL mempool input
static void
test_jbpf_get_mempool_size_null(void** state)
{
    JBPF_IO_UNUSED(state); /* unused */
    int size = jbpf_get_mempool_size(NULL);
    JBPF_IO_UNUSED(size);
    assert(size == -1);
}

static void
test_valid_unix_address(void** state)
{
    struct jbpf_io_ipc_proto_addr dipc_primary_addr;
    const char* valid_unix_addr = "unix://test_socket";
    int result = _jbpf_io_ipc_parse_addr("/path/to/run", valid_unix_addr, &dipc_primary_addr);
    JBPF_IO_UNUSED(result);
    assert(result == 0);
    assert(dipc_primary_addr.type == JBPF_IO_IPC_TYPE_UNIX);
    assert(strcmp(dipc_primary_addr.un_addr.sun_path, "/path/to/run/test_socket") == 0);
}

static void
test_valid_vsock_address(void** state)
{
    struct jbpf_io_ipc_proto_addr dipc_primary_addr;
    const char* valid_vsock_addr = "vsock://3:1024";
    int result = _jbpf_io_ipc_parse_addr("/path/to/run", valid_vsock_addr, &dipc_primary_addr);
    JBPF_IO_UNUSED(result);
    assert(result == 0);
    assert(dipc_primary_addr.type == JBPF_IO_IPC_TYPE_VSOCK);
    assert(dipc_primary_addr.vsock_addr.svm_cid == 3);
    assert(dipc_primary_addr.vsock_addr.svm_port == 1024);
}

static void
test_invalid_address_format(void** state)
{
    struct jbpf_io_ipc_proto_addr dipc_primary_addr;
    const char* invalid_addr = "invalid_address";
    int result = _jbpf_io_ipc_parse_addr("/path/to/run", invalid_addr, &dipc_primary_addr);
    JBPF_IO_UNUSED(result);
    assert(result == -1);
}

static void
test_null_parameters(void** state)
{
    const char* valid_unix_addr = "unix://test_socket";
    int result = _jbpf_io_ipc_parse_addr("/path/to/run", valid_unix_addr, NULL);
    JBPF_IO_UNUSED(result);
    assert(result == -1);
}

static void
test_edge_case_empty_string(void** state)
{
    struct jbpf_io_ipc_proto_addr dipc_primary_addr;
    const char* empty_addr = "";
    int result = _jbpf_io_ipc_parse_addr("/path/to/run", empty_addr, &dipc_primary_addr);
    JBPF_IO_UNUSED(result);
    assert(result == -1);
    assert(dipc_primary_addr.type == JBPF_IO_IPC_TYPE_UNIX);
}

static void
test_invalid_protocol(void** state)
{
    struct jbpf_io_ipc_proto_addr dipc_primary_addr;
    const char* invalid_protocol_addr = "invalid://test_socket";
    int result = _jbpf_io_ipc_parse_addr("/path/to/run", invalid_protocol_addr, &dipc_primary_addr);
    JBPF_IO_UNUSED(result);
    assert(result == -1);
    assert(dipc_primary_addr.type == JBPF_IO_IPC_TYPE_INVALID);
}

int
main(int argc, char** argv)
{
    const jbpf_test tests[] = {
        JBPF_CREATE_TEST(test_round_up_pow_of_two, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_io_queue_create_valid_parameters, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_io_queue_reserve_null_ctx, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_io_queue_release_null, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_io_queue_dequeue_null_ctx, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_io_queue_get_elem_size_valid, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_io_queue_get_elem_size_null_ctx, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_io_tohex_str_valid_input, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_io_tohex_str_null_input, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_io_tohex_str_null_output, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_io_tohex_str_insufficient_output_size, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_io_tohex_str_empty_input, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_normal_case, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_already_a_multiple, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_zero_multiple_size, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_zero_original_size, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_both_zero, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_get_mempool_size_valid, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_get_mempool_size_valid_shared, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_jbpf_get_mempool_size_null, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_valid_unix_address, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_valid_vsock_address, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_invalid_address_format, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_null_parameters, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_edge_case_empty_string, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_invalid_protocol, NULL, NULL, NULL),
    };

    int num_tests = sizeof(tests) / sizeof(jbpf_test);
    return jbpf_run_test(tests, num_tests, group_setup, group_teardown);
}
