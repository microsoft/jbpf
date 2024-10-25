// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef JBPF_IO_CHANNEL_DEFS_H
#define JBPF_IO_CHANNEL_DEFS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief The maximum number of channels
 * @ingroup io
 */
#define JBPF_IO_MAX_NUM_CHANNELS (512)

/**
 * @brief The length of the stream id
 * @ingroup io
 */
#define JBPF_IO_STREAM_ID_LEN (16U)

/**
 * @brief The maximum length of the descriptor
 * @ingroup io
 */
#define JBPF_IO_MAX_DESCRIPTOR_SIZE (1048560)

    struct jbpf_io_stream_id
    {
        uint8_t id[JBPF_IO_STREAM_ID_LEN];
    };

    typedef int jbpf_io_chan_id;
    typedef struct jbpf_io_channel_ctx jbpf_io_channel_ctx_t;

    typedef enum
    {
        JBPF_IO_CHANNEL_OUTPUT = 0,
        JBPF_IO_CHANNEL_INPUT
    } jbpf_io_channel_direction;

    typedef enum
    {
        HIGH_IO_CHANNEL_PRIORITY = 0,
        LOW_IO_CHANNEL_PRIORITY
    } jbpf_io_channel_priority;

    typedef enum
    {
        JBPF_IO_CHANNEL_RINGBUF = 0,
        JBPF_IO_CHANNEL_QUEUE
    } jbpf_io_channel_type;

    struct jbpf_io_channel_request
    {
        uint32_t elem_size;
        uint32_t num_elems;
        jbpf_io_channel_direction direction;
        jbpf_io_channel_type type;
        struct jbpf_io_stream_id stream_id;
        size_t descriptor_size;
        char descriptor[JBPF_IO_MAX_DESCRIPTOR_SIZE];
    };

    typedef struct jbpf_io_channel jbpf_io_channel_t;

    typedef struct jbpf_io_in_channel_list jbpf_in_channel_list;
    typedef struct jbpf_io_out_channel_list jbpf_out_channel_list;

    struct jbpf_io_channel_list
    {
        struct jbpf_io_in_channel_list* in_channel_list;
        struct jbpf_io_out_channel_list* out_channel_list;
    };

#ifdef __cplusplus
}
#endif

#endif