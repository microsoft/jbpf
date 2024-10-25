/* Copyright (c) Microsoft Corporation. All rights reserved.

This test stress and evaluates the performance by spwaning two processes: Primary and Seconary.

1. Primary Process:
- Initializes as the primary IPC context.
- Waits for the secondary process to signal readiness.
- Processes buffers, validates data integrity, and tracks processed buffers.
- Prints checkpoints and performance metrics.

2. Secondary Process:
- Initializes as the secondary IPC context.
- Creates multiple output channels.
- Spawns multiple worker threads.
- Each worker thread reserves a buffer, populates it with data, and submits it to a random channel.
- Synchronizes thread start using barriers.
- Prints checkpoints for iterations.

3. Cleanup: Signals the primary process upon completion and destroys all channels. The main function waits for the
secondary process to finish and ensures it exits successfully.
*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>
#include <time.h>

#include "jbpf_io.h"
#include "jbpf_io_defs.h"
#include "jbpf_io_queue.h"
#include "jbpf_io_channel.h"
#include "jbpf_io_utils.h"

#define PRIMARY_SEM_NAME "/jbpf_io_stress_test_primary_sem"
#define SECONDARY_SEM_NAME "/jbpf_io_stress_test_secondary_sem"

sem_t *primary_sem, *secondary_sem;

#define NUMS_OF_ELEMENTS 1000
#define NUM_CHANNELS 254
#define NUM_OF_THREADS 20
#define MAX_ITER 10000
#define CHECK_POINT 1000

pthread_barrier_t barrier;

#define DATA_SIZE 200

typedef struct _test_struct
{
    uint32_t sender;
    uint32_t seq;
    char data[200];
} test_struct;

uint64_t num_processed = 0;

void
handle_channel_bufs(
    struct jbpf_io_channel* io_channel, struct jbpf_io_stream_id* stream_id, void** bufs, int num_bufs, void* ctx)
{

    for (int i = 0; i < num_bufs; i++) {
        struct _test_struct* s = bufs[i];
        JBPF_IO_UNUSED(s);
        for (int j = 1; j < DATA_SIZE - 1; j++) {
            assert(!(s->data[j] != 0 && s->data[j] != s->sender));
        }
        num_processed++;
        if (num_processed % CHECK_POINT == 0) {
            printf("Checkpoint: %lu\n", num_processed);
        }
        jbpf_io_channel_release_buf(bufs[i]);
    }
}

void
primary_process(char* jbpf_io_ipc_name)
{
    struct jbpf_io_config io_config = {0};
    struct jbpf_io_ctx* io_ctx;

    io_config.type = JBPF_IO_IPC_PRIMARY;

    strncpy(io_config.ipc_config.addr.jbpf_io_ipc_name, jbpf_io_ipc_name, JBPF_IO_IPC_MAX_NAMELEN - 1);
    io_ctx = jbpf_io_init(&io_config);
    if (io_ctx == NULL) {
        printf("Could not create successfully\n");
        exit(1);
    } else {
        printf("Socket created successfully!!!\n");
    }

    jbpf_io_register_thread();

    // Primary is initialized. Signal secondary and wait until ready
    sem_post(primary_sem);
    sem_wait(secondary_sem);

    time_t start_time = time(NULL); // Get the current time in seconds
    time_t current_time;

    jbpf_io_channel_handle_out_bufs(io_ctx, handle_channel_bufs, io_ctx);

    current_time = time(NULL);
    time_t elapsed_time = current_time - start_time;
    printf("Processed %lu buffers in total in %ld seconds\n", num_processed, elapsed_time);
}

// 240 channels, size of buffer/msg 100 bytes, num of elements 1000
// 10 threads,  for loop, pick a random channel (round bin) , reseve buf, write some data, commit, without/with sleep

struct thread_params
{
    int thread_id;
    struct jbpf_io_ctx* io_ctx;
    jbpf_io_channel_t** io_channel;
    int total_channels;
};

int
get_random_between_range_inclusive(int min, int max)
{
    return (rand() % (max - min + 1)) + min;
}

// thread function
void*
secondary_worker_thread(void* params)
{

    struct thread_params* ctx = (struct thread_params*)params;

    jbpf_io_register_thread();

    pthread_barrier_wait(&barrier);

    printf("Created worker with thread id %d\n", ctx->thread_id);

    int seq = 0;
    for (int a = 0; a < MAX_ITER; a++) {
        int selected_channel = get_random_between_range_inclusive(0, ctx->total_channels - 1);
        // printf("Thread %d will use channel %d\n", ctx->thread_id, selected_channel);
        jbpf_io_channel_t* io_channel = ctx->io_channel[selected_channel];

        struct _test_struct* data = jbpf_io_channel_reserve_buf(io_channel);
        if (!data) {
            // printf("Could not reserve buffer %d\n", ctx->thread_id);
            continue;
        }
        memset(data, 0, sizeof(test_struct));

        // populate the buffer
        data->data[ctx->thread_id] = ctx->thread_id;
        data->sender = ctx->thread_id;
        data->seq = seq++;

        if (jbpf_io_channel_submit_buf(io_channel) != 0) {
            printf("Data not sent %d\n", ctx->thread_id);
            exit(1);
        }

        if (a % CHECK_POINT == 0) {
            printf("Thread %d: Checkpoint: %d\n", ctx->thread_id, a);
        }
    }

    printf("Done with thread %ld\n", pthread_self());
    pthread_exit(0);
}

void
secondary_stress_test(char* jbpf_io_ipc_name)
{
    struct jbpf_io_ctx* io_ctx;
    struct jbpf_io_config io_config = {0};

    jbpf_io_channel_t* io_channels[NUM_CHANNELS];
    struct jbpf_io_stream_id stream_id[NUM_CHANNELS];

    // Populate some stream ids
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        stream_id[i].id[0] = i;
        stream_id[i].id[1] = i;
    }

    io_config.type = JBPF_IO_IPC_SECONDARY;
    io_config.ipc_config.mem_cfg.memory_size = 1024 * 1024 * 1024;
    strncpy(io_config.ipc_config.addr.jbpf_io_ipc_name, jbpf_io_ipc_name, JBPF_IO_IPC_MAX_NAMELEN - 1);

    // Wait until the primary is ready
    sem_wait(primary_sem);

    io_ctx = jbpf_io_init(&io_config);
    if (io_ctx == NULL) {
        printf("Could not create %s.\n", io_config.ipc_config.addr.jbpf_io_ipc_name);
        exit(1);
    } else {
        printf("Socket created successfully\n");
    }

    // Create some output channels
    for (int i = 0; i < NUM_CHANNELS; i++) {
        io_channels[i] = jbpf_io_create_channel(
            io_ctx,
            JBPF_IO_CHANNEL_OUTPUT,
            JBPF_IO_CHANNEL_QUEUE,
            NUMS_OF_ELEMENTS,
            sizeof(test_struct),
            stream_id[i],
            NULL,
            0);

        if (!io_channels[i]) {
            printf("Channel could not be created successfully\n");
            exit(3);
        }
    }

    // create threads
    pthread_barrier_init(&barrier, NULL, NUM_OF_THREADS);
    pthread_t threads[NUM_OF_THREADS];
    struct thread_params params[NUM_OF_THREADS];
    for (int i = 0; i < NUM_OF_THREADS; i++) {
        params[i].thread_id = i + 1;
        params[i].io_ctx = io_ctx;
        params[i].io_channel = io_channels;
        params[i].total_channels = NUM_CHANNELS;
        pthread_create(&threads[i], NULL, secondary_worker_thread, &params[i]);
    }

    // join threads
    for (int i = 0; i < NUM_OF_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    sem_post(secondary_sem);

    // Destroy all channels
    for (int i = 0; i < NUM_CHANNELS; i++) {
        jbpf_io_destroy_channel(io_ctx, io_channels[i]);
    }

    jbpf_io_stop();
}

int
main(int argc, char* argv[])
{

    pid_t child_pid;
    int secondary_status;

    // Remove the semaphore if the test did not finish gracefully
    sem_unlink(PRIMARY_SEM_NAME);
    sem_unlink(SECONDARY_SEM_NAME);

    primary_sem = sem_open(PRIMARY_SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0);
    if (primary_sem == SEM_FAILED) {
        exit(EXIT_FAILURE);
    }

    secondary_sem = sem_open(SECONDARY_SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0);
    if (secondary_sem == SEM_FAILED) {
        exit(EXIT_FAILURE);
    }

    child_pid = fork();
    assert(child_pid >= 0);

    char* name = "stress_test";
    if (child_pid == 0) {
        secondary_stress_test(name);
    } else {
        primary_process(name);
        wait(&secondary_status);
        assert(secondary_status == 0);

        printf("Test is now complete...\n");
    }

    return 0;
}
