// Copyright (c) Microsoft Corporation. All rights reserved.
/*
This test ensures each consumer thread sends data to a unique worker thread.*/

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

#define NUM_THREADS 4
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
sem_t consumer_sem[NUM_THREADS];
sem_t worker_start_sem[NUM_THREADS];
sem_t worker_done_sem[NUM_THREADS];

jbpf_io_channel_t* local_channel;

struct worker_thread_args
{
    struct test_struct** shared_buffer_ptr;
    int thread_index;
};

void*
producer_thread_func(void* arg)
{
    jbpf_io_register_thread();
    for (uint32_t i = 0; i < CHANNEL_CAPACITY; i++) {
        sem_wait(&producer_sem);

        struct test_struct* buffer = jbpf_io_channel_reserve_buf(local_channel);
        assert(buffer);
        buffer->id = i;
        buffer->value = i * 10;

        assert(jbpf_io_channel_submit_buf(local_channel) == 0);
        sem_post(&consumer_sem[i % NUM_THREADS]);
    }
    return NULL;
}

void*
consumer_thread_func(void* arg)
{
    jbpf_io_register_thread();
    struct worker_thread_args* args = (struct worker_thread_args*)arg;
    int thread_index = args->thread_index;

    sem_wait(&consumer_sem[thread_index]);

    void* recv_ptr[10];
    int num_received = jbpf_io_channel_recv_data(local_channel, recv_ptr, 1);
    assert(num_received == 1);

    *args->shared_buffer_ptr = (struct test_struct*)recv_ptr[0];
    struct test_struct** local_buffer_ptr = args->shared_buffer_ptr;

    // Test jbpf_io_channel_share_data_ptr functionality
    void* shared_ptr = jbpf_io_channel_share_data_ptr(*local_buffer_ptr);
    assert(shared_ptr == *local_buffer_ptr);

    // Signal the associated worker to start processing
    sem_post(&worker_start_sem[thread_index]);

    // Release the buffer
    jbpf_io_channel_release_buf(*local_buffer_ptr);

    // Wait for the worker to complete processing
    sem_wait(&worker_done_sem[thread_index]);

    return NULL;
}

void*
worker_thread_func(void* arg)
{
    jbpf_io_register_thread();
    struct worker_thread_args* args = (struct worker_thread_args*)arg;
    int thread_index = args->thread_index;

    sem_wait(&worker_start_sem[thread_index]);
    struct test_struct** local_buffer_ptr = args->shared_buffer_ptr;

    // Process shared buffer
    struct test_struct* buffer = *local_buffer_ptr;
    assert(buffer);
    uint32_t id = buffer->id;
    uint32_t value = buffer->value;
    assert(id * 10 == value);

    sem_post(&worker_done_sem[thread_index]);
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

    pthread_t producer_thread, consumer_threads[NUM_THREADS], worker_threads[NUM_THREADS];
    struct test_struct* local_current_buffer[NUM_THREADS];

    sem_init(&producer_sem, 0, CHANNEL_CAPACITY);
    for (int i = 0; i < NUM_THREADS; i++) {
        sem_init(&consumer_sem[i], 0, 0);
        sem_init(&worker_start_sem[i], 0, 0);
        sem_init(&worker_done_sem[i], 0, 0);
    }

    // Start producer thread
    assert(pthread_create(&producer_thread, NULL, producer_thread_func, NULL) == 0);

    // Start consumer and worker threads
    struct worker_thread_args worker_args[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        worker_args[i].shared_buffer_ptr = &local_current_buffer[i];
        worker_args[i].thread_index = i;

        assert(pthread_create(&consumer_threads[i], NULL, consumer_thread_func, &worker_args[i]) == 0);
        assert(pthread_create(&worker_threads[i], NULL, worker_thread_func, &worker_args[i]) == 0);
    }

    // Wait for producer to finish
    assert(pthread_join(producer_thread, NULL) == 0);

    // Signal workers to stop
    for (int i = 0; i < NUM_THREADS; i++) {
        sem_post(&worker_start_sem[i]);
    }

    // Wait for consumer and worker threads to finish
    for (int i = 0; i < NUM_THREADS; i++) {
        assert(pthread_join(consumer_threads[i], NULL) == 0);
        assert(pthread_join(worker_threads[i], NULL) == 0);
    }

    // Cleanup
    sem_destroy(&producer_sem);
    for (int i = 0; i < NUM_THREADS; i++) {
        sem_destroy(&consumer_sem[i]);
        sem_destroy(&worker_start_sem[i]);
        sem_destroy(&worker_done_sem[i]);
    }

    jbpf_io_destroy_channel(io_ctx, local_channel);
    jbpf_io_stop();

    return 0;
}
