/*
 * The purpose of this test is to check the correct functionality of the io lib, when used in IPC shm mode (i.e.,
 * JBPF_IO_IPC_PRIMARY and JBPF_IO_IPC_SECONDARY).
 *
 * This test does the following:
 * 1. It forks two processes, one that acts as the IO primary and one that acts as the IO secondary.
 * 2. It creates a new thread in the primary process, to also test channels used for intra-process communication.
 * 3. The secondary process loads two (de)serialization libraries (serde and serde2).
 * 4. The secondary process creates two output channels that are linked to the serialization libraries and one that does
 * not have a serialization library.
 * 4. The secondary process asserts the following:
 *  - A secondary process can access the IO channels that it created, but not those created by another process (the
 * primary).
 *  - Serialization and deserialization works as expected for the channels that have a serde library registered.
 *  - It reserves buffers in the IO channels successfully and sends the data to the primary process.
 *  - It receives some control input data from the primary process.
 *  - IO channels are destroyed successfully and once destroyed, they can no longer be accessed.
 * 5. The primary process asserts the following:
 *  - The data sent by the secondary process is received successfully.
 *  - It can send a control input to an input channel that was created by the secondary process.
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

#include "jbpf_io.h"
#include "jbpf_io_defs.h"
#include "jbpf_io_queue.h"
#include "jbpf_io_channel.h"
#include "jbpf_io_utils.h"

#define NUM_ITERS 3
#define PRIMARY_SEM_NAME "/jbpf_io_ipc_test_primary_sem"
#define SECONDARY_SEM_NAME "/jbpf_io_ipc_test_secondary_sem"

struct test_struct
{
    uint32_t counter_a;
    uint32_t counter_b;
};

struct test_struct2
{
    int counter_a;
};

sem_t *primary_sem, *secondary_sem;

bool output_done = false;
bool local_output_sent = false;

struct jbpf_io_stream_id input_stream_id = {
    .id = {0xA3, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XF2}};

struct jbpf_io_stream_id stream_id1 = {
    .id = {0xF1, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF}};

struct jbpf_io_stream_id stream_id2 = {
    .id = {0xF2, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XF1}};

struct jbpf_io_stream_id stream_id3 = {
    .id = {0xF3, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XF2}};

struct jbpf_io_stream_id local_stream_id = {
    .id = {0xF5, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF}};

unsigned char*
read_serde_lib(const char* filename, long* filesize)
{
    FILE* file = fopen(filename, "rb"); // Open the file in binary read mode
    if (file == NULL) {
        perror("Failed to open file");
        return NULL;
    }

    // Move the file pointer to the end of the file to determine the file size
    fseek(file, 0, SEEK_END);
    *filesize = ftell(file);
    fseek(file, 0, SEEK_SET); // Move the file pointer back to the beginning

    // Allocate memory to hold the file contents
    unsigned char* buffer = (unsigned char*)malloc(*filesize);
    if (buffer == NULL) {
        perror("Failed to allocate memory");
        fclose(file);
        return NULL;
    }

    // Read the file contents into the buffer
    size_t bytesRead = fread(buffer, 1, *filesize, file);
    if (bytesRead != *filesize) {
        perror("Failed to read file");
        free(buffer);
        fclose(file);
        return NULL;
    }

    // Clean up and return the buffer
    fclose(file);
    return buffer;
}

int channel1_index = 0;
int channel1_results[] = {200, 400, 7000};

int channel3_index = 0;
int channel3_results[] = {4444, 0, 1, 2};

int total_processed = 0;

void
handle_channel_bufs(
    struct jbpf_io_channel* io_channel, struct jbpf_io_stream_id* stream_id, void** bufs, int num_bufs, void* ctx)
{

    struct test_struct* s;
    struct test_struct2* s2;

    JBPF_IO_UNUSED(s);
    JBPF_IO_UNUSED(s2);

    for (int i = 0; i < num_bufs; i++) {
        if (memcmp(stream_id, &local_stream_id, sizeof(local_stream_id)) == 0) {
            s = bufs[i];
            assert(s);
            assert(s->counter_a == 19821924);
            total_processed++;
        } else if (memcmp(stream_id, &stream_id1, sizeof(stream_id1)) == 0) {
            s = bufs[i];
            assert(s);
            assert(s->counter_a == channel1_results[channel1_index]);
            assert(s->counter_b == channel1_results[channel1_index] * 2);
            channel1_index++;
            total_processed++;
        } else if (memcmp(stream_id, &stream_id2, sizeof(stream_id2)) == 0) {
            s2 = bufs[i];
            assert(s2);
            assert(s2->counter_a == 1234);
            total_processed++;
        } else if (memcmp(stream_id, &stream_id3, sizeof(stream_id3)) == 0) {
            s = bufs[i];
            assert(s);
            assert(s->counter_a == channel3_results[channel3_index]);
            assert(s->counter_b == channel3_results[channel3_index] * 2);
            channel3_index++;
            total_processed++;
        }
    }
}

void*
local_channel_thread(void* args)
{
    jbpf_io_channel_t *local_channel, *local_channel2;
    struct test_struct* local_data;
    struct jbpf_io_ctx* io_ctx;
    io_ctx = args;

    JBPF_IO_UNUSED(local_channel2);

    jbpf_io_register_thread();

    // Create an output channel
    local_channel = jbpf_io_create_channel(
        io_ctx,
        JBPF_IO_CHANNEL_OUTPUT,
        JBPF_IO_CHANNEL_QUEUE,
        100,
        sizeof(struct test_struct),
        local_stream_id,
        NULL,
        0);

    assert(local_channel);

    local_data = jbpf_io_channel_reserve_buf(local_channel);

    assert(local_data);

    local_data->counter_a = 19821924;

    assert(jbpf_io_channel_submit_buf(local_channel) == 0);

    local_output_sent = true;

    // Create an output channel
    local_channel2 = jbpf_io_create_channel(
        io_ctx,
        JBPF_IO_CHANNEL_OUTPUT,
        JBPF_IO_CHANNEL_QUEUE,
        100,
        sizeof(struct test_struct),
        local_stream_id,
        NULL,
        0);

    // Channel was not created, because the stream id is the same as an existing channel
    assert(!local_channel2);

    while (!output_done) {
        sleep(1);
    }

    jbpf_io_destroy_channel(io_ctx, local_channel);
    assert(!jbpf_io_find_channel(io_ctx, local_stream_id, true));

    return NULL;
}

void
run_primary(char* serde1, char* serde2)
{
    struct jbpf_io_config io_config = {0};
    struct jbpf_io_ctx* io_ctx;

    struct test_struct test_input;
    test_input.counter_a = 9999;
    pthread_t local_channel_test;

    JBPF_IO_UNUSED(test_input);

    io_config.type = JBPF_IO_IPC_PRIMARY;

    strncpy(io_config.ipc_config.addr.jbpf_io_ipc_name, "test", JBPF_IO_IPC_MAX_NAMELEN);
    io_ctx = jbpf_io_init(&io_config);

    assert(io_ctx);

    jbpf_io_register_thread();

    pthread_create(&local_channel_test, NULL, local_channel_thread, io_ctx);

    // Primary is initialized. Signal secondary and wait until ready
    sem_post(primary_sem);
    sem_wait(secondary_sem);
    while (!local_output_sent) {
        sleep(1);
    }

    // Check outputs of other threads. All messages should have been sent by now
    jbpf_io_channel_handle_out_bufs(io_ctx, handle_channel_bufs, io_ctx);

    // Check that all the messages were received
    assert(total_processed == 9);

    output_done = true;
    pthread_join(local_channel_test, NULL);

    // Local channel should be gone by now
    assert(!jbpf_io_find_channel(io_ctx, local_stream_id, true));

    // Now send a control input and notify the secondary
    assert(jbpf_io_channel_send_msg(io_ctx, &input_stream_id, (char*)&test_input, sizeof(struct test_struct)) == 0);
    sem_post(primary_sem);
    pthread_join(local_channel_test, NULL);
}

void
run_secondary(char* serde1, char* serde2)
{
    struct jbpf_io_ctx* io_ctx;
    struct jbpf_io_config io_config = {0};
    jbpf_io_channel_t *io_channel, *io_channel2, *io_channel3;
    jbpf_io_channel_t* input_channel;
    void *serde_lib, *serde2_lib;
    long lib_size, lib2_size;
    int serialized_len;
    struct jbpf_io_stream_id tmp_sid;

    char* serialized_output1 = "{ \"counter_a\": 200, \"counter_b\": 400 }";

    // JBPF_IO_UNUSED(tmp_s);
    JBPF_IO_UNUSED(tmp_sid);
    JBPF_IO_UNUSED(serialized_len);
    JBPF_IO_UNUSED(serialized_len);
    JBPF_IO_UNUSED(serialized_output1);

    io_config.type = JBPF_IO_IPC_SECONDARY;
    io_config.ipc_config.mem_cfg.memory_size = 1024 * 1024 * 1024;
    strncpy(io_config.ipc_config.addr.jbpf_io_ipc_name, "test", JBPF_IO_IPC_MAX_NAMELEN);

    // Wait until the primary is ready
    sem_wait(primary_sem);

    io_ctx = jbpf_io_init(&io_config);
    assert(io_ctx);

    jbpf_io_register_thread();

    serde_lib = read_serde_lib(serde1, &lib_size);
    serde2_lib = read_serde_lib(serde2, &lib2_size);

    // Create an output channel
    io_channel = jbpf_io_create_channel(
        io_ctx,
        JBPF_IO_CHANNEL_OUTPUT,
        JBPF_IO_CHANNEL_QUEUE,
        100,
        sizeof(struct test_struct),
        stream_id1,
        serde_lib,
        lib_size);

    assert(io_channel);

    // Reserve a buffer and populate it
    struct test_struct* data = jbpf_io_channel_reserve_buf(io_channel);

    assert(data);

    data->counter_a = 200;
    data->counter_b = 400;

#ifdef JBPF_EXPERIMENTAL_FEATURES
    char serialized_data[1024];

    // Serialize the buffer and check that it was serialized successfully
    serialized_len = jbpf_io_channel_pack_msg(io_ctx, data, serialized_data, 1024);

    assert(serialized_len > 0);
    assert(strcmp(serialized_output1, serialized_data + JBPF_IO_STREAM_ID_LEN) == 0);
    assert(memcmp(serialized_data, &stream_id1, JBPF_IO_STREAM_ID_LEN) == 0);

    // Deserialize the data and check if the output is the expected one
    jbpf_channel_buf_ptr buf = jbpf_io_channel_unpack_msg(io_ctx, serialized_data, serialized_len, &tmp_sid);
    assert(buf != NULL);
    struct test_struct* tmp_s = (struct test_struct*)buf;
    assert(tmp_s->counter_a == 200);
    assert(tmp_s->counter_b == 400);
#endif

    assert(jbpf_io_channel_submit_buf(io_channel) == 0);

    // Repeat with a second buffer
    data = jbpf_io_channel_reserve_buf(io_channel);

    assert(data);

    data->counter_a = 400;
    data->counter_b = 800;

    assert(jbpf_io_channel_submit_buf(io_channel) == 0);

    // Create 2 more output channels
    io_channel2 = jbpf_io_create_channel(
        io_ctx,
        JBPF_IO_CHANNEL_OUTPUT,
        JBPF_IO_CHANNEL_QUEUE,
        100,
        sizeof(struct test_struct2),
        stream_id2,
        serde2_lib,
        lib2_size);

    assert(io_channel2);

    io_channel3 = jbpf_io_create_channel(
        io_ctx, JBPF_IO_CHANNEL_OUTPUT, JBPF_IO_CHANNEL_QUEUE, 6000, sizeof(struct test_struct), stream_id3, NULL, 0);

    assert(io_channel3);

    input_channel = jbpf_io_create_channel(
        io_ctx,
        JBPF_IO_CHANNEL_INPUT,
        JBPF_IO_CHANNEL_QUEUE,
        100,
        sizeof(struct test_struct),
        input_stream_id,
        NULL,
        0);

    assert(input_channel);

    // Done with the channels.
    // Signal the primary that all is ready for the test

    // This is a channel of a different process, so assertion should fail
    assert(!jbpf_io_find_channel(io_ctx, local_stream_id, true));

    // Those channels are local, so they should be found
    assert(jbpf_io_find_channel(io_ctx, stream_id1, true));
    assert(jbpf_io_find_channel(io_ctx, input_stream_id, false));

    // Send some data from the new channels
    struct test_struct2* data2;
    data2 = jbpf_io_channel_reserve_buf(io_channel2);

    assert(data2);

    data2->counter_a = 1234;

    assert(jbpf_io_channel_submit_buf(io_channel2) == 0);

    data = jbpf_io_channel_reserve_buf(io_channel3);

    assert(data);

    data->counter_a = 4444;
    data->counter_b = 8888;

    assert(jbpf_io_channel_submit_buf(io_channel3) == 0);

    // Send some data from the remaining channels
    data = jbpf_io_channel_reserve_buf(io_channel);

    assert(data);

    data->counter_a = 7000;
    data->counter_b = 14000;

    assert(jbpf_io_channel_submit_buf(io_channel) == 0);

    for (int i = 0; i < NUM_ITERS; i++) {
        data = jbpf_io_channel_reserve_buf(io_channel3);
        assert(data);
        data->counter_a = i;
        data->counter_b = i * 2;
        assert(jbpf_io_channel_submit_buf(io_channel3) == 0);
    }

    sem_post(secondary_sem);

    // Wait until the input was sent to the input channel
    sem_wait(primary_sem);

    void* recv_ptr[10];
    int num_recv = jbpf_io_channel_recv_data(input_channel, recv_ptr, 1);
    assert(num_recv == 1);

    struct test_struct* t = (struct test_struct*)recv_ptr[0];

    JBPF_IO_UNUSED(t);
    JBPF_IO_UNUSED(num_recv);

    assert(t->counter_a == 9999);
    jbpf_io_channel_release_buf(recv_ptr[0]);

    // Destroy all channels
    jbpf_io_destroy_channel(io_ctx, io_channel);
    jbpf_io_destroy_channel(io_ctx, io_channel2);
    jbpf_io_destroy_channel(io_ctx, io_channel3);

    // The channel was destroyed, so we shouldn't be able to find it
    assert(!jbpf_io_find_channel(io_ctx, stream_id1, true));
    assert(!jbpf_io_find_channel(io_ctx, stream_id2, true));
    assert(!jbpf_io_find_channel(io_ctx, stream_id3, true));

    jbpf_io_stop();

    free(serde_lib);
    free(serde2_lib);
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

    if (child_pid == 0) {
        run_secondary(argv[1], argv[2]);
    } else {
        run_primary(argv[1], argv[2]);
        wait(&secondary_status);
        assert(secondary_status == 0);

        printf("Test is now complete...\n");
    }

    return 0;
}