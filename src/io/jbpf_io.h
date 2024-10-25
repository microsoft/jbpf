// Copyright (c) Microsoft Corporation. All rights reserved.
#ifndef JBPF_IO_H
#define JBPF_IO_H

#include "jbpf_io_defs.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct jbpf_io_ctx jbpf_io_ctx_t;

    /**
     * @brief Initializes a jbpf_io primary or secondary process
     *
     * @param io_config The configuration for the jbpf_io initialization.
     * @return jbpf_io_ctx_t* An opaque pointer to the jbpf_io context or NULL
     * if the initialization failed.
     * @ingroup io
     */
    jbpf_io_ctx_t*
    jbpf_io_init(struct jbpf_io_config* io_config);

    /**
     * @brief Stops and tears down the context of a jbpf_io instance.
     * Can be called from both the primary and the secondary.
     * @ingroup io
     */
    void
    jbpf_io_stop(void);

    /**
     * @brief Get a pointer to the local jbpf_io context.
     *
     * @return struct jbpf_io_ctx* An opaque pointer to the jbpf_io context or
     * NULL, if jbpf_io has not been initialized.
     * @ingroup io
     */
    struct jbpf_io_ctx*
    jbpf_io_get_ctx(void);

    /**
     * @brief Creates a jbpf_io_channel. Can be called from both the primary and the secondary process.
     *
     * @param io_ctx An opaque pointer to the jbpf_io context.
     * @param channel_direction Can be JBPF_IO_CHANNEL_INPUT or JBPF_IO_CHANNEL_OUTPUT.
     * In the case of IPC communication, JBPF_IO_CHANNEL_OUTPUT implies that buffers are reserved
     * by the secondary process and are received by the primary process. Similarly, JBPF_IO_CHANNEL_INPUT
     * implies that buffers are reserved by the primary process and are received by the secondary process.
     * @param channel_type Currently only JBPF_IO_CHANNEL_QUEUE is supported.
     * @param num_elems The (minimum) number of elements of this channel. The actual number of
     * elements can be greater than num_elems, and depends on the implementation.
     * @param elem_size The size of each element of this channel.
     * @param stream_id The stream_id should be unique for this channel.
     * @param descriptor A shared library containing an encoder and a decoder function for the elements of this channel.
     * The descriptor is stored in a byte array.
     * @param descriptor_size The size of the descriptor array.
     * @return struct jbpf_io_channel* Returns an opaque pointer to a jbpf_io_channel or NULL,
     * if the channel was not created.
     * @ingroup io
     */
    struct jbpf_io_channel*
    jbpf_io_create_channel(
        jbpf_io_ctx_t* io_ctx,
        jbpf_io_channel_direction channel_direction,
        jbpf_io_channel_type channel_type,
        int num_elems,
        int elem_size,
        struct jbpf_io_stream_id stream_id,
        char* descriptor,
        size_t descriptor_size);

    /**
     * @brief Destroys a jbpf_io_channel. Can be called from both the primary and the secondary process.
     *
     * @param io_ctx An opaque pointer to the jbpf_io context.
     * @param io_channel An opaque pointer to the jbpf_io_channel to be destroyed.
     * @ingroup io
     */
    void
    jbpf_io_destroy_channel(jbpf_io_ctx_t* io_ctx, struct jbpf_io_channel* io_channel);

    /**
     * @brief Registers a thread to jbpf_io. Must be called by all threads that will call the
     * jbpf_io_channel API (reserve, release, submit).
     *
     * @return true If the thread was registered successfuly.
     * @return false If the thread failed to register.
     * @ingroup io
     */
    bool
    jbpf_io_register_thread(void);

    /**
     * @brief Removes a thread that was registered with jbpf_io_register_thread().
     * @ingroup io
     */
    void
    jbpf_io_remove_thread(void);

#ifdef __cplusplus
}
#endif

#endif
