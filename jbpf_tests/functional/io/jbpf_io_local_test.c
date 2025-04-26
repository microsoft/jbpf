/*
 * The purpose of this test is to check the correct functionality of the io lib, when used in standalone mode (i.e.,
 * JBPF_IO_LOCAL_PRIMARY).
 *
 * This test does the following:
 * 1. It initializes the io library with a local primary.
 * 2. It loads two (de)serialization libraries (serde and serde2).
 * 3. It creates two output channels that are linked to the serialization libraries and one that does not have a
 * serialization library.
 * 4. It asserts the following:
 *  - All the channels are created successfully.
 *  - We cannot create two channels with the same stream id.
 *  - Serialization and deserialization works as expected for the channels that have a serde library registered.
 *  - Serialization fails for the channel that does not have a serde library registered.
 *  - Serialization fails, if the provided output buffer is not large enough.
 * 5. Finally, it sends some data to the output channels and asserts that the callback function receives them as
 * expected.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jbpf_io.h"
#include "jbpf_io_defs.h"
#include "jbpf_io_queue.h"
#include "jbpf_io_channel.h"
#include "jbpf_io_utils.h"
#include "jbpf_test_lib.h"

#define NUM_INSERTIONS 4

struct test_struct
{
    uint32_t counter_a;
    uint32_t counter_b;
};

struct test_struct2
{
    int counter_a;
};

struct jbpf_io_stream_id stream_id1 = {
    .id = {0xF1, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XB1}};

struct jbpf_io_stream_id stream_id2 = {
    .id = {0xF2, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XF1}};

struct jbpf_io_stream_id stream_id3 = {
    .id = {0xF2, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XA1}};

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

void
assert_output_data(
    struct jbpf_io_channel* io_channel, struct jbpf_io_stream_id* stream_id, void** bufs, int num_bufs, void* ctx)
{
    struct test_struct* s1;
    struct test_struct2* s2;

    JBPF_IO_UNUSED(s1);
    JBPF_IO_UNUSED(s2);

    assert(io_channel);
    assert(stream_id);

    assert(num_bufs == NUM_INSERTIONS);

    for (int i = 0; i < num_bufs; i++) {
        if (memcmp(stream_id, &stream_id1, sizeof(stream_id1)) == 0) {
            s1 = bufs[i];
            assert(s1);
            assert(s1->counter_a == i);
            assert(s1->counter_b == i + 1);
        } else if (memcmp(stream_id, &stream_id1, sizeof(stream_id2))) {
            s2 = bufs[i];
            assert(s2->counter_a == i + 100);
        }
    }
}

int
main(int argc, char* argv[])
{

    struct jbpf_io_config io_config = {0};
    struct jbpf_io_ctx* io_ctx;
    jbpf_io_channel_t *io_channel, *io_channel2, *io_channel3;
    struct jbpf_io_stream_id stream_id_deserialized;
    char descriptor[65535] = {0};
    struct test_struct *test_data1, test_data_tmp;
    struct test_struct2* test_data2;
    int serialized_size;

#ifdef JBPF_EXPERIMENTAL_FEATURES
    char serialized[1024];
    char serialized_output[1024];
#endif

    JBPF_IO_UNUSED(serialized_size);
    JBPF_IO_UNUSED(stream_id_deserialized);

    io_config.type = JBPF_IO_LOCAL_PRIMARY;
    io_config.local_config.mem_cfg.memory_size = 1024 * 1024 * 1024;
    strncpy(io_config.jbpf_path, JBPF_DEFAULT_RUN_PATH, JBPF_RUN_PATH_LEN - 1);
    io_config.jbpf_path[JBPF_RUN_PATH_LEN - 1] = '\0';

    strncpy(io_config.jbpf_namespace, JBPF_DEFAULT_NAMESPACE, JBPF_NAMESPACE_LEN - 1);
    io_config.jbpf_namespace[JBPF_NAMESPACE_LEN - 1] = '\0';

    long lib_size, lib2_size;
    void *serde_lib, *serde2_lib;

    serde_lib = read_serde_lib(argv[1], &lib_size);
    serde2_lib = read_serde_lib(argv[2], &lib2_size);

    io_ctx = jbpf_io_init(&io_config);
    assert(io_ctx);

    jbpf_io_register_thread();

    // Create an output channel
    io_channel = jbpf_io_create_channel(
        io_ctx,
        JBPF_IO_CHANNEL_OUTPUT,
        JBPF_IO_CHANNEL_QUEUE,
        NUM_INSERTIONS,
        sizeof(struct test_struct),
        stream_id1,
        serde_lib,
        lib_size);

    assert(io_channel);

    // Create a second output channel
    io_channel2 = jbpf_io_create_channel(
        io_ctx,
        JBPF_IO_CHANNEL_OUTPUT,
        JBPF_IO_CHANNEL_QUEUE,
        NUM_INSERTIONS,
        sizeof(struct test_struct2),
        stream_id2,
        serde2_lib,
        lib2_size);

    assert(io_channel2);

    // Try to create a third channel with the same stream_id
    io_channel3 = jbpf_io_create_channel(
        io_ctx,
        JBPF_IO_CHANNEL_OUTPUT,
        JBPF_IO_CHANNEL_QUEUE,
        NUM_INSERTIONS,
        sizeof(struct test_struct2),
        stream_id2,
        descriptor,
        0);

    assert(!io_channel3);

    io_channel3 = jbpf_io_create_channel(
        io_ctx,
        JBPF_IO_CHANNEL_OUTPUT,
        JBPF_IO_CHANNEL_QUEUE,
        NUM_INSERTIONS,
        sizeof(struct test_struct2),
        stream_id3,
        NULL,
        0);

    // Channel with stream id 1 exists
    __assert__(jbpf_io_find_channel(io_ctx, stream_id2, true) != NULL);

    // Channel with stream id 1 is output channel
    __assert__(jbpf_io_find_channel(io_ctx, stream_id2, false) == NULL);

    // Channel with stream id 2 exists
    __assert__(jbpf_io_find_channel(io_ctx, stream_id2, true) != NULL);

    // Channel with stream id 3 does not exist
    __assert__(jbpf_io_find_channel(io_ctx, stream_id2, true) != NULL);

    for (int i = 0; i < NUM_INSERTIONS - 1; i++) {
        test_data1 = jbpf_io_channel_reserve_buf(io_channel);
        assert(test_data1);

        test_data1->counter_a = i;
        test_data1->counter_b = i + 1;

#ifdef JBPF_EXPERIMENTAL_FEATURES
        // Check that the message was serialized successfully
        serialized_size = jbpf_io_channel_pack_msg(io_ctx, test_data1, serialized, sizeof(serialized));
        assert(serialized_size > 0);
        snprintf(
            serialized_output,
            1024,
            "{ \"counter_a\": %u, \"counter_b\": %u }",
            test_data1->counter_a,
            test_data1->counter_b);
        assert(
            strncmp(serialized + JBPF_IO_STREAM_ID_LEN, serialized_output, serialized_size - JBPF_IO_STREAM_ID_LEN) ==
            0);

        // Check that the message was deserialized successfully
        jbpf_channel_buf_ptr buf = NULL;
        buf = jbpf_io_channel_unpack_msg(io_ctx, serialized, serialized_size, &stream_id_deserialized);
        assert(buf != NULL);
        assert(memcmp(&stream_id_deserialized, &stream_id1, JBPF_IO_STREAM_ID_LEN) == 0);

        JBPF_IO_UNUSED(buf);

        // Check that serialization will fail if we don't give big enough buffer
        __assert__(jbpf_io_channel_pack_msg(io_ctx, test_data1, serialized, 4) == -1);
#endif
        __assert__(jbpf_io_channel_submit_buf(io_channel) == 0);
    }

    test_data_tmp.counter_a = NUM_INSERTIONS - 1;
    test_data_tmp.counter_b = NUM_INSERTIONS;

    jbpf_io_channel_send_data(io_channel, &test_data_tmp, sizeof(test_data_tmp));

    __assert__(jbpf_io_channel_submit_buf(io_channel) == -2);

    for (int i = 0; i < NUM_INSERTIONS; i++) {
        test_data2 = jbpf_io_channel_reserve_buf(io_channel2);
        assert(test_data2);

        test_data2->counter_a = i + 100;

#ifdef JBPF_EXPERIMENTAL_FEATURES
        // Check that the message was serialized successfully
        serialized_size = jbpf_io_channel_pack_msg(io_ctx, test_data2, serialized, sizeof(serialized));
        assert(serialized_size > 0);
        snprintf(serialized_output, 1024, "{ \"counter_a\": %d }", test_data2->counter_a);
        assert(
            strncmp(serialized + JBPF_IO_STREAM_ID_LEN, serialized_output, serialized_size - JBPF_IO_STREAM_ID_LEN) ==
            0);
#endif
        __assert__(jbpf_io_channel_submit_buf(io_channel2) == 0);
    }

    // Write some data to each channel and make sure you get the same data out
    jbpf_io_channel_handle_out_bufs(io_ctx, assert_output_data, NULL);

    // Check that serialization fails for io_channel3, since it has no serializer
    test_data2 = jbpf_io_channel_reserve_buf(io_channel3);
    assert(test_data2);
#ifdef JBPF_EXPERIMENTAL_FEATURES
    serialized_size = jbpf_io_channel_pack_msg(io_ctx, test_data2, serialized, sizeof(serialized));
    assert(serialized_size == -1);
#endif
    free(serde_lib);
    free(serde2_lib);
}