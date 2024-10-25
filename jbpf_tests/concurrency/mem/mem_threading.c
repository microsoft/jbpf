// Copyright (c) Microsoft Corporation. All rights reserved.
/*
    This test creates 3 threads, each thread allocates 100 buffers and shares 10,20,70 buffers with other threads
   respectively. The threads synchronize using barriers to ensure that all threads have allocated the buffers before
   proceeding to free them. The test checks that all buffers are freed and returned to the pool correctly.
*/

#include <jbpf_test_lib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "jbpf_mem_mgmt.h"
#include "jbpf_mem_mgmt_utils.h"
#include "jbpf_mempool.h"
#include "jbpf_mempool_int.h"
#include "jbpf_io_utils.h"

pthread_barrier_t barrier;
pthread_barrier_t barrier2;
pthread_barrier_t barrier3;
jbpf_mempool_t* mempool;

#define SHARE_BUFFER_SIZE_1 10
#define SHARE_BUFFER_SIZE_2 20
#define SHARE_BUFFER_SIZE_3 70
#define NON_SHARE_SIZE_BUFFER 1000
// 2GB memory
#define MEM_SIZE ((unsigned long long)2 * 1024 * 1024 * 1024)
#define NUM_OF_ELEMENTS 10000
#define ELEMENT_SIZE 8

void*
worker1(void* args)
{
    jbpf_mbuf_t** shared_mbuf = args;
    pthread_barrier_wait(&barrier);
    jbpf_mbuf_t* mb[NON_SHARE_SIZE_BUFFER];
    for (int i = 0; i < NON_SHARE_SIZE_BUFFER; i++) {
        mb[i] = jbpf_mbuf_alloc(mempool);
        assert(mb[i] != NULL);
    }
    pthread_barrier_wait(&barrier2);
    printf("Time to release all non-shared buffers for worker 1\n");
    // Free all non-shared buffers
    for (int i = 0; i < NON_SHARE_SIZE_BUFFER; i++) {
        jbpf_mbuf_free(mb[i], false);
    }
    pthread_barrier_wait(&barrier3);
    printf("Time to release all shared buffers for worker 1\n");
    // Free all shared buffers
    for (int i = 0; i < SHARE_BUFFER_SIZE_1; i++) {
        jbpf_mbuf_free(shared_mbuf[i], false);
    }
    return NULL;
}

void*
worker2(void* args)
{
    jbpf_mbuf_t** shared_mbuf = args;
    pthread_barrier_wait(&barrier);
    jbpf_mbuf_t* mb[NON_SHARE_SIZE_BUFFER];
    for (int i = 0; i < NON_SHARE_SIZE_BUFFER; i++) {
        mb[i] = jbpf_mbuf_alloc(mempool);
        assert(mb[i] != NULL);
    }
    pthread_barrier_wait(&barrier2);
    printf("Time to release all non-shared buffers for worker 2\n");
    // Free all non-shared buffers
    for (int i = 0; i < NON_SHARE_SIZE_BUFFER; i++) {
        jbpf_mbuf_free(mb[i], false);
    }
    pthread_barrier_wait(&barrier3);
    printf("Time to release all shared buffers for worker 2\n");
    // Free all shared buffers
    for (int i = 0; i < SHARE_BUFFER_SIZE_2; i++) {
        jbpf_mbuf_free(shared_mbuf[i], false);
    }
    return NULL;
}

void*
worker3(void* args)
{
    jbpf_mbuf_t** shared_mbuf = args;
    pthread_barrier_wait(&barrier);
    jbpf_mbuf_t* mb[NON_SHARE_SIZE_BUFFER];
    for (int i = 0; i < NON_SHARE_SIZE_BUFFER; i++) {
        mb[i] = jbpf_mbuf_alloc(mempool);
        assert(mb[i] != NULL);
    }
    pthread_barrier_wait(&barrier2);
    printf("Time to release all non-shared buffers for worker 3\n");
    // Free all non-shared buffers
    for (int i = 0; i < NON_SHARE_SIZE_BUFFER; i++) {
        jbpf_mbuf_free(mb[i], false);
    }
    pthread_barrier_wait(&barrier3);
    printf("Time to release all shared buffers for worker 3\n");
    // Free all shared buffers
    for (int i = 0; i < SHARE_BUFFER_SIZE_3; i++) {
        jbpf_mbuf_free(shared_mbuf[i], false);
    }
    return NULL;
}

int
main(int argc, char** argv)
{
    int flags = 0;
    int res;
    void* ptr;
    pthread_t tid, tid2, tid3;
    printf("Running mem_threading concurrency test...\n");

    jbpf_mbuf_t* shared_mbuf1[SHARE_BUFFER_SIZE_1];
    jbpf_mbuf_t* shared_mbuf2[SHARE_BUFFER_SIZE_2];
    jbpf_mbuf_t* shared_mbuf3[SHARE_BUFFER_SIZE_3];
    jbpf_mbuf_t* mbuf1[SHARE_BUFFER_SIZE_1];
    jbpf_mbuf_t* mbuf2[SHARE_BUFFER_SIZE_2];
    jbpf_mbuf_t* mbuf3[SHARE_BUFFER_SIZE_3];

    int ret;

    JBPF_IO_UNUSED(ret);
    JBPF_IO_UNUSED(res);

    ret = pthread_barrier_init(&barrier, NULL, 3);
    assert(ret == 0);

    ret = pthread_barrier_init(&barrier2, NULL, 4);
    assert(ret == 0);

    ret = pthread_barrier_init(&barrier3, NULL, 3);
    assert(ret == 0);

    res = jbpf_init_memory(MEM_SIZE, flags);
    assert(res == 0);

    printf("Memory initialized successfully\n");
    ptr = jbpf_malloc(1024 * 1024);
    assert(ptr != NULL);
    mempool = jbpf_init_mempool(NUM_OF_ELEMENTS, ELEMENT_SIZE, JBPF_MEMPOOL_MP);
    assert(mempool != NULL);

    // Allocate and share pointers that will be used by all threads
    for (int i = 0; i < SHARE_BUFFER_SIZE_1; i++) {
        mbuf1[i] = jbpf_mbuf_alloc(mempool);
        assert(mbuf1[i] != NULL);
        shared_mbuf1[i] = jbpf_mbuf_share(mbuf1[i]);
    }

    for (int i = 0; i < SHARE_BUFFER_SIZE_2; i++) {
        mbuf2[i] = jbpf_mbuf_alloc(mempool);
        assert(mbuf2[i] != NULL);
        shared_mbuf2[i] = jbpf_mbuf_share(mbuf2[i]);
    }

    for (int i = 0; i < SHARE_BUFFER_SIZE_3; i++) {
        mbuf3[i] = jbpf_mbuf_alloc(mempool);
        assert(mbuf3[i] != NULL);
        shared_mbuf3[i] = jbpf_mbuf_share(mbuf3[i]);
    }

    // Create threads and pass the pointers
    pthread_create(&tid, NULL, worker1, shared_mbuf1);
    pthread_create(&tid2, NULL, worker2, shared_mbuf2);
    pthread_create(&tid3, NULL, worker3, shared_mbuf3);

    // All threads run and allocate the rest 300 buffers
    pthread_barrier_wait(&barrier2);

    pthread_join(tid, NULL);
    pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);

    for (int i = 0; i < SHARE_BUFFER_SIZE_1; i++) {
        jbpf_mbuf_free(shared_mbuf1[i], false);
    }
    for (int i = 0; i < SHARE_BUFFER_SIZE_2; i++) {
        jbpf_mbuf_free(shared_mbuf2[i], false);
    }
    for (int i = 0; i < SHARE_BUFFER_SIZE_3; i++) {
        jbpf_mbuf_free(shared_mbuf3[i], false);
    }
    // Check if all buffers are freed
    jbpf_mbuf_t* entry = mempool->data_array;
    for (int i = 0; i < mempool->num_elems; i++) {
        assert(entry->ref_cnt == 0);
        entry = (jbpf_mbuf_t*)((uint8_t*)entry + mempool->mbuf_size);
    }
    jbpf_free(ptr);
    jbpf_destroy_mempool(mempool);
    jbpf_destroy_memory();
    printf("Test passed sucessfully!\n");
    return 0;
}
