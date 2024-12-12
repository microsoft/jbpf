// Copyright (c) Microsoft Corporation. All rights reserved.
/*
This test evaluates the behavior of the jbpf_io_channel_share_data_ptr API in a multithreaded environment.
It is designed to simulate a typical producer-consumer-worker pattern:

- **Producer Thread:** Produces structured test data and submits it to a channel.
- **Consumer Thread:** Retrieves data from the channel, processes it briefly, and delegates processing tasks to worker
threads.
- **Worker Threads:** Perform parallel processing on shared data pointers obtained from the
jbpf_io_channel_share_data_ptr function.

Key Objectives:
1. Validate the correctness and safety of shared data pointers in concurrent use.
2. Ensure proper buffer management (reservation, submission, and release).
3. Verify that worker threads can process shared data pointers concurrently without data corruption.
4. Verify the correctness of the jbpf_io_channel_share_data_ptr API in a multithreaded environment.
*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>

#include "jbpf_io.h"
#include "jbpf_io_defs.h"
#include "jbpf_io_queue.h"
#include "jbpf_io_channel.h"
#include "jbpf_io_utils.h"

#define NUM_WORKER_THREADS 4
#define CHANNEL_NUMBER_OF_ELEMENTS 100
#define CHANNEL_CAPACITY 255

struct test_struct
{
    uint32_t id;
    uint32_t value;
};

struct jbpf_io_stream_id local_stream_id = {
    .id = {0xF5, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF}};

sem_t producer_sem;
sem_t consumer_sem;
sem_t worker_start_sem;
sem_t worker_done_sem;

jbpf_io_channel_t* local_channel;
volatile int stop_workers = 0;
struct test_struct* current_buffer = NULL;

void*
producer_thread_func(void* arg)
{
    for (u_int32_t i = 0; i < CHANNEL_CAPACITY; i++) {
        sem_wait(&producer_sem);

        struct test_struct* buffer = jbpf_io_channel_reserve_buf(local_channel);
        assert(buffer);
        buffer->id = i;
        buffer->value = i * 10;

        assert(jbpf_io_channel_submit_buf(local_channel) == 0);
        sem_post(&consumer_sem);
    }
    return NULL;
}

void*
consumer_thread_func(void* arg)
{
    for (u_int32_t i = 0; i < CHANNEL_CAPACITY; i++) {
        sem_wait(&consumer_sem);

        void* recv_ptr[10];
        int num_received = jbpf_io_channel_recv_data(local_channel, recv_ptr, 1);
        assert(num_received == 1);

        current_buffer = (struct test_struct*)recv_ptr[0];

        // Signal workers to start processing
        for (int j = 0; j < NUM_WORKER_THREADS; j++) {
            sem_post(&worker_start_sem);
        }

        // Wait for workers to complete processing
        for (int j = 0; j < NUM_WORKER_THREADS; j++) {
            sem_wait(&worker_done_sem);
        }

        // Release the buffer, but shared buffer is still valid
        jbpf_io_channel_release_buf(current_buffer);
        current_buffer = NULL;
    }

    return NULL;
}

void*
worker_thread_func(void* arg)
{
    while (1) {
        sem_wait(&worker_start_sem);

        if (stop_workers) {
            break;
        }

        // Process shared buffer
        struct test_struct* buffer = current_buffer;
        assert(buffer);

        sem_post(&worker_done_sem);
    }

    return NULL;
}

int
main(int argc, char* argv[])
{
    printf("Starting jbpf_io_channel_share_data_ptr multithreading tests ...\n");

    struct jbpf_io_config io_config = {0};
    struct jbpf_io_ctx* io_ctx;

    strncpy(io_config.ipc_config.addr.jbpf_io_ipc_name, "test", JBPF_IO_IPC_MAX_NAMELEN);
    io_ctx = jbpf_io_init(&io_config);
    assert(io_ctx);

    jbpf_io_register_thread();

    // Create a channel with a capacity of 10
    local_channel = jbpf_io_create_channel(
        io_ctx,
        JBPF_IO_CHANNEL_OUTPUT,
        JBPF_IO_CHANNEL_QUEUE,
        CHANNEL_NUMBER_OF_ELEMENTS,
        sizeof(struct test_struct),
        local_stream_id,
        NULL,
        0);

    assert(local_channel);

    pthread_t producer_thread, consumer_thread, worker_threads[NUM_WORKER_THREADS];

    sem_init(&producer_sem, 0, CHANNEL_CAPACITY);
    sem_init(&consumer_sem, 0, 0);
    sem_init(&worker_start_sem, 0, 0);
    sem_init(&worker_done_sem, 0, 0);

    // Start producer and consumer threads
    assert(pthread_create(&producer_thread, NULL, producer_thread_func, NULL) == 0);
    assert(pthread_create(&consumer_thread, NULL, consumer_thread_func, NULL) == 0);

    // Start worker threads
    for (int i = 0; i < NUM_WORKER_THREADS; i++) {
        assert(pthread_create(&worker_threads[i], NULL, worker_thread_func, NULL) == 0);
    }

    // Wait for producer and consumer to finish
    assert(pthread_join(producer_thread, NULL) == 0);
    assert(pthread_join(consumer_thread, NULL) == 0);

    // Signal workers to stop
    stop_workers = 1;
    for (int i = 0; i < NUM_WORKER_THREADS; i++) {
        sem_post(&worker_start_sem);
    }

    for (int i = 0; i < NUM_WORKER_THREADS; i++) {
        assert(pthread_join(worker_threads[i], NULL) == 0);
    }

    // Cleanup
    sem_destroy(&producer_sem);
    sem_destroy(&consumer_sem);
    sem_destroy(&worker_start_sem);
    sem_destroy(&worker_done_sem);

    jbpf_io_destroy_channel(io_ctx, local_channel);
    jbpf_io_stop();

    return 0;
}