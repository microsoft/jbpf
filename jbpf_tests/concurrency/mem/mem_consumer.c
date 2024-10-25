// Copyright (c) Microsoft Corporation. All rights reserved.
/*
    This test creates 10 threads, each thread allocates 500 buffers and shares all of them with other threads.
    The threads synchronize using barriers to ensure that all threads have allocated the buffers before proceeding to
   free them. The test checks that all buffers are freed and returned to the pool correctly. This test iterates itself
   1000 times. This test checks the ref count of the shared buffers.
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

#define NUM_THREADS 8
#define NUM_OF_ELEMENTS 10000
#define NUM_OF_SIZE 1000
#define MBUF_SIZE 8
#define MAX_ITER 1000

#define SHARED_MBUF_SIZE 500
#define MAX_ITER_PRODUCER_CONSUMER 1000
#define NUM_OF_THREADS_CONSUMER_THREAD 10

// 2GB memory
#define MEM_SIZE ((unsigned long long)2 * 1024 * 1024 * 1024)

pthread_barrier_t barrier;

struct thread_args_producer_consumer
{
    int thread_id;
    jbpf_mbuf_t* shared_mbuf[SHARED_MBUF_SIZE];
};

// worker_thread for multiple producers and single consumer
void*
worker_thread_consumer(void* args)
{
    struct thread_args_producer_consumer* targs = (struct thread_args_producer_consumer*)args;
    jbpf_mbuf_t** shared_mbuf = targs->shared_mbuf;
    pthread_barrier_wait(&barrier);

    for (int i = 0; i < SHARED_MBUF_SIZE; i++) {
        jbpf_mbuf_free(shared_mbuf[i], false);
    }
    pthread_exit(NULL);
}

int
main(int argc, char** argv)
{
    printf("Running test_jbpf_mem_mgmt_threading_consumers %d threads\n", NUM_OF_THREADS_CONSUMER_THREAD);
    jbpf_mempool_t* mempool;

    int res;

    JBPF_IO_UNUSED(res);

    pthread_t tid[NUM_OF_THREADS_CONSUMER_THREAD];
    struct thread_args_producer_consumer args[NUM_OF_THREADS_CONSUMER_THREAD] = {};
    jbpf_mbuf_t* mbuf[SHARED_MBUF_SIZE];

    res = jbpf_init_memory(MEM_SIZE, 0);
    assert(res == 0);

    mempool = jbpf_init_mempool(NUM_OF_ELEMENTS, NUM_OF_SIZE, JBPF_MEMPOOL_MP);
    assert(mempool != NULL);

    for (int x = 0; x < MAX_ITER_PRODUCER_CONSUMER; ++x) {
        int size_of_mempool = jbpf_get_mempool_size(mempool);
        int expected_size = round_up_pow_of_two(NUM_OF_ELEMENTS) - 1;
        assert(size_of_mempool == expected_size);
        int capacity_of_mempool = jbpf_get_mempool_capacity(mempool);
        assert(capacity_of_mempool == expected_size);

        for (int i = 0; i < SHARED_MBUF_SIZE; i++) {
            mbuf[i] = jbpf_mbuf_alloc(mempool);
        }

        pthread_barrier_init(&barrier, NULL, NUM_OF_THREADS_CONSUMER_THREAD);
        for (int i = 0; i < NUM_OF_THREADS_CONSUMER_THREAD; i++) {
            args[i].thread_id = i;
            for (int j = 0; j < SHARED_MBUF_SIZE; j++) {
                args[i].shared_mbuf[j] = jbpf_mbuf_share(mbuf[j]);
            }
            pthread_create(&tid[i], NULL, worker_thread_consumer, &args[i]);
        }

        for (int i = 0; i < NUM_OF_THREADS_CONSUMER_THREAD; i++) {
            pthread_join(tid[i], NULL);
        }

        // the ref count should be one as we still haven't free the ones in the main thread.
        for (int i = 0; i < SHARED_MBUF_SIZE; i++) {
            assert(mbuf[i]->ref_cnt == 1);
        }

        int size_of_mempool_after = jbpf_get_mempool_size(mempool);
        int capacity_of_mempool_after = jbpf_get_mempool_capacity(mempool);

        JBPF_IO_UNUSED(capacity_of_mempool_after);
        JBPF_IO_UNUSED(size_of_mempool_after);
        JBPF_IO_UNUSED(capacity_of_mempool);
        JBPF_IO_UNUSED(size_of_mempool);
        JBPF_IO_UNUSED(expected_size);

        assert(size_of_mempool_after == expected_size - SHARED_MBUF_SIZE);
        assert(capacity_of_mempool_after == capacity_of_mempool);

        for (int i = 0; i < SHARED_MBUF_SIZE; i++) {
            jbpf_mbuf_free(mbuf[i], false);
        }
        size_of_mempool_after = jbpf_get_mempool_size(mempool);
        capacity_of_mempool_after = jbpf_get_mempool_capacity(mempool);
        assert(size_of_mempool_after == expected_size);
        assert(capacity_of_mempool_after == capacity_of_mempool);
    }

    // Check if all buffers are freed
    jbpf_mbuf_t* entry = mempool->data_array;
    for (int i = 0; i < mempool->num_elems; i++) {
        assert(entry->ref_cnt == 0);
        entry = (jbpf_mbuf_t*)((uint8_t*)entry + mempool->mbuf_size);
    }

    printf("Cleaning up\n");
    jbpf_destroy_mempool(mempool);
    jbpf_destroy_memory();
    printf("Test passed sucessfully!\n");
    return 0;
}
