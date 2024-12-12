// Copyright (c) Microsoft Corporation. All rights reserved.
/*
This test tests the single thead version of the jbpf_io_channel_share_data_ptr() function.
This test creates a channel: The size of the mempool is 255 and the capacity is 255.
It then reserves a buffer, share it (to increase the reference count), and then release it.
It then submits the buffer to the channel and repeats the process until the mempool is full.
It then destroys the channel and checks if it is gone.
The counter is used to check if the correct number of buffers were reserved.
*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>

#include "jbpf_io.h"
#include "jbpf_io_defs.h"
#include "jbpf_io_queue.h"
#include "jbpf_io_channel.h"
#include "jbpf_io_utils.h"

struct test_struct
{
    uint32_t counter_a;
    uint32_t counter_b;
};

struct jbpf_io_stream_id local_stream_id = {
    .id = {0xF5, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF, 0xFF, 0xFF, 0XFF, 0XFF}};

int
main(int argc, char* argv[])
{
    struct jbpf_io_config io_config = {0};
    struct jbpf_io_ctx* io_ctx;

    strncpy(io_config.ipc_config.addr.jbpf_io_ipc_name, "test", JBPF_IO_IPC_MAX_NAMELEN);
    io_ctx = jbpf_io_init(&io_config);

    assert(io_ctx);

    jbpf_io_register_thread();

    jbpf_io_channel_t* local_channel;
    struct test_struct* local_data;

    jbpf_io_register_thread();

    // Create an output channel
    local_channel = jbpf_io_create_channel(
        io_ctx,
        JBPF_IO_CHANNEL_OUTPUT,
        JBPF_IO_CHANNEL_QUEUE,
        200,
        sizeof(struct test_struct),
        local_stream_id,
        NULL,
        0);

    assert(local_channel);

    int i = 0;
    while (true) {
        local_data = jbpf_io_channel_reserve_buf(local_channel);
        if (!local_data) {
            break;
        }
        void* p = jbpf_io_channel_share_data_ptr(local_data);
        assert(p == local_data);
        jbpf_io_channel_release_buf(local_data);
        assert(jbpf_io_channel_submit_buf(local_channel) == 0);
        i++;
    }
    assert(i == 255);

    jbpf_io_destroy_channel(io_ctx, local_channel);
    assert(!jbpf_io_find_channel(io_ctx, local_stream_id, true));

    // Local channel should be gone by now
    assert(!jbpf_io_find_channel(io_ctx, local_stream_id, true));

    // cleanup
    jbpf_io_stop();

    return 0;
}
