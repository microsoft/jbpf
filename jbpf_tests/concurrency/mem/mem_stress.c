// Copyright (c) Microsoft Corporation. All rights reserved.
/*
    This test creates 4 threads, each thread allocates 20 buffers and 1MB memory and frees them in a loop (MAX_ITER
   iterations). The test checks that all buffers are freed and returned to the pool correctly.
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

#define NUM_THREADS 4
#define NUM_OF_ELEMENTS 10000
#define NUM_OF_SIZE 2
#define MBUF_SIZE 20
#define MAX_ITER 5000

pthread_barrier_t barrier;
jbpf_mempool_t* mempool;
// 2GB memory
#define MEM_SIZE ((unsigned long long)2 * 1024 * 1024 * 1024)

struct thread_args
{
    int thread_id;
};

// worker_thread
void*
worker_thread(void* args)
{
    pthread_barrier_wait(&barrier);
    struct thread_args* targs = (struct thread_args*)args;
    jbpf_mbuf_t* mb[MBUF_SIZE];
    for (int x = 0; x < MAX_ITER; x++) {
        for (int i = 0; i < MBUF_SIZE; i++) {
            mb[i] = jbpf_mbuf_alloc(mempool);
            assert(mb[i] != NULL);
        }
        void* ptr = jbpf_malloc(1024 * 1024);
        assert(ptr != NULL);
        jbpf_free(ptr);
        for (int i = 0; i < MBUF_SIZE; i++) {
            jbpf_mbuf_free(mb[i], false);
        }
    }
    printf("Finished Thread %d\n", targs->thread_id);
    pthread_exit(NULL);
}

int
main(int argc, char** argv)
{
    int flags = 0;
    int res;
    pthread_t tid[NUM_THREADS];

    JBPF_IO_UNUSED(res);

    res = jbpf_init_memory(MEM_SIZE, flags);
    assert(res == 0);

    pthread_barrier_init(&barrier, NULL, NUM_THREADS);

    mempool = jbpf_init_mempool(NUM_OF_ELEMENTS, NUM_OF_SIZE, JBPF_MEMPOOL_MP);
    if (mempool != NULL) {
        printf("Mempool created successfully\n");
    }

    struct thread_args args[NUM_THREADS] = {};

    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].thread_id = i;
        pthread_create(&tid[i], NULL, worker_thread, &args[i]);
    }

    printf("Joining %d threads\n", NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(tid[i], NULL);
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
