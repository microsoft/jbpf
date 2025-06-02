#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "jbpf_io.h"
#include "jbpf_io_defs.h"
#include "jbpf_io_queue.h"
#include "jbpf_io_channel.h"
#include "jbpf_io_utils.h"
#include "jbpf_logging.h"

#define IPC_NAME "test_channel_ipc"
#define NUM_INSERTIONS 100

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

int
main(int argc, char* argv[])
{

    struct jbpf_io_config io_config = {0};
    struct jbpf_io_ctx* io_ctx;
    jbpf_io_channel_t *io_channel, *io_channel2;

    // Designate the data collection framework as a primary for the IPC
    io_config.type = JBPF_IO_IPC_PRIMARY;

    strncpy(io_config.jbpf_path, JBPF_DEFAULT_RUN_PATH, JBPF_RUN_PATH_LEN - 1);
    io_config.jbpf_path[JBPF_RUN_PATH_LEN - 1] = '\0';

    strncpy(io_config.jbpf_namespace, JBPF_DEFAULT_NAMESPACE, JBPF_NAMESPACE_LEN - 1);
    io_config.jbpf_namespace[JBPF_NAMESPACE_LEN - 1] = '\0';

    strncpy(io_config.ipc_config.addr.jbpf_io_ipc_name, IPC_NAME, JBPF_IO_IPC_MAX_NAMELEN);

    // Configure memory size for the IO buffer
    io_config.ipc_config.mem_cfg.memory_size = JBPF_HUGEPAGE_SIZE_2MB;

    // Configure the jbpf agent to operate in shared memory mode
    io_ctx = jbpf_io_init(&io_config);

    assert(io_ctx);

    jbpf_io_register_thread();

    // Create an output channel
    io_channel = jbpf_io_create_channel(
        io_ctx,
        JBPF_IO_CHANNEL_OUTPUT,
        JBPF_IO_CHANNEL_QUEUE,
        NUM_INSERTIONS,
        4,
        stream_id1,
        NULL,
        0);

    assert(io_channel);

    usleep(1000); // Give some time for the channel to be created
 
    // Create a second output channel
    jbpf_logger(JBPF_INFO, "Creating second channel.\n");
    io_channel2 = jbpf_io_create_channel(
        io_ctx,
        JBPF_IO_CHANNEL_OUTPUT,
        JBPF_IO_CHANNEL_QUEUE,
        NUM_INSERTIONS,
        4,
        stream_id2,
        NULL,
        0);

    assert(io_channel2);

    usleep(1000); // Give some time for the channel to be created

}