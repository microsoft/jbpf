// Copyright (c) Microsoft Corporation. All rights reserved.
#ifndef JBPF_IO_CHANNEL_H
#define JBPF_IO_CHANNEL_H

#include "jbpf_io_defs.h"
#include "jbpf_mem_mgmt.h"

#define JBPF_IO_BUFS_BATCH_SIZE 10

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void* jbpf_channel_buf_ptr;

    typedef void (*handle_channel_bufs_cb_t)(
        struct jbpf_io_channel* io_channel,
        struct jbpf_io_stream_id* stream_id,
        jbpf_channel_buf_ptr* bufs,
        int num_bufs,
        void* ctx);

    /**
     * @brief Finds a jbpf_io_channel. Can be called both from the primary and the secondary process.
     *
     * @param io_ctx A pointer to a jbpf_io ctx.
     * @param stream_id The stream id of the channel to find.
     * @param is_output true if looking for an output channel and false if looking for an input channel.
     * @return struct jbpf_io_channel* Returns a pointer to the jbpf_io_channel, if the channel is found,
     * or NULL, if the channel is not found.
     * @ingroup io
     */
    struct jbpf_io_channel*
    jbpf_io_find_channel(struct jbpf_io_ctx* io_ctx, struct jbpf_io_stream_id stream_id, bool is_output);

    /**
     * @brief Checks all the available output channels and calls handler to process any received packets.
     * Can only be called from the primary jbpf_io process and is thread safe.
     *
     * @param io_ctx A pointer to a jbpf_io ctx.
     * @param handle_channel_bufs A callback function that will be called for every output channel that
     * has data to process.
     * @param ctx A context to be passed to the callback handle_channel_bufs.
     * @return void
     */
    void
    jbpf_io_channel_handle_out_bufs(
        struct jbpf_io_ctx* io_ctx, handle_channel_bufs_cb_t handle_channel_bufs, void* ctx);

    /**
     * @brief Sends some data to one of the existing input channels.
     * Can only be called by the primary jbpf_io_process and is thread safe.
     *
     * @param io_ctx A pointer to a jbpf_io ctx.
     * @param stream_id The stream id of the target input channel.
     * @param data The data buffer to be sent.
     * @param size The size of the data buffer.
     * @return int 0 if message was sent, or -1 otherwise
     * @ingroup io
     */
    int
    jbpf_io_channel_send_msg(struct jbpf_io_ctx* io_ctx, struct jbpf_io_stream_id* stream_id, void* data, size_t size);

    /**
     * @brief Reserves a buffer of a jbpf_io_channel.
     * Every time that jbpf_io_channel_reserve_buf() is called by a thread,
     * a buffer is reserved and is stored in thread local storage, until jbpf_io_channel_submit_buf() or
     * jbpf_io_channel_release_buf() is called by the same thread.
     * If jbpf_io_channel_reserve_buf() is called several times without calling jbpf_io_channel_submit_buf() or
     * jbpf_io_channel_release_buf(), then the same pointer is returned.
     * For IPC communication, if this is an input channel, this can only be called by the primary process.
     * If this is an output channel, then it can only be called by the secondary process.
     *
     * @param channel A pointer to the target jbpf_io_channel.
     * @return jbpf_channel_buf_ptr A pointer to the reserved buffer.
     */
    jbpf_channel_buf_ptr
    jbpf_io_channel_reserve_buf(struct jbpf_io_channel* channel);

    /**
     * @brief Submits a buffer reserved by jbpf_io_channel_reserve_buf() to the channel queue.
     * This function has to be called by the same thread that reserved the buffer, which is
     * why the buffer pointer does not have to be passed as an argument.
     * For IPC communication, if this is an input channel, this can only be called by the primary process.
     * If this is an output channel, then it can only be called by the secondary process.
     *
     * @param channel A pointer to the target jbpf_io_channel.
     * @return int 0 if the buffer was submitted or -1 otherwise.
     * @ingroup io
     */
    int
    jbpf_io_channel_submit_buf(struct jbpf_io_channel* channel);

    /**
     * @brief Releases a buffer that was reserved by jbpf_io_channel_reserve_buf() and returns
     * it back to the memory pool. This function can be called by any thread and not necessarily from
     * the one that called jbpf_io_channel_reserve_buf(). For IPC communication, if this is an input channel,
     * this can only be called by the secondary process. If this is an output channel, then it can only be called by the
     * primary process.
     *
     * @param buf_ptr A pointer to the buffer to be released.
     * @return void
     */
    void
    jbpf_io_channel_release_buf(jbpf_channel_buf_ptr buf_ptr);

    /**
     * @brief Releases all the buffers of a channel that have been reserved with jbpf_io_channel_reserve_buf()
     * by any thread of the process. For IPC communication, if this is an input channel, this can only be called by
     * the secondary process. If this is an output channel, then it can only be called by the primary process.
     *
     * @param channel A pointer to the target jbpf_io_channel.
     */
    void
    jbpf_io_channel_release_all_buf(struct jbpf_io_channel* channel);

    /**
     * @brief Sends a buffer of data to an input or output channel.
     * This call combines jbpf_io_channel_reserve_buf() with jbpf_io_channel_submit_buf() and does
     * a memcpy() to copy the data to the reserved buffer. For IPC communication,
     * in the case of an input channel, this can only be called by the primary process, while
     * in the case of an output channel it can only be called by the secondary process.
     *
     * @param channel A pointer to the target jbpf_io_channel.
     * @param data A buffer with data to be sent through the channel.
     * @param size The size of the buffer.
     * @return int 0 if the data was sent or -1 otherwise.
     * @ingroup io
     */
    int
    jbpf_io_channel_send_data(struct jbpf_io_channel* channel, void* data, size_t size);

    /**
     * @brief Receives a batch of data buffers from a channel.
     * For IPC communication, in the case of an input channel, this can only be called by the secondary process, while
     * in the case of an output channel it can only be called by the primary process.
     *
     * @param channel A pointer to the target jbpf_io_channel.
     * @param data_ptrs An array of pointers to store the batch of the received data.
     * @param batch_size The maximum number of buffers to receive in a batch.
     * @return int The number of buffers that were actually received.
     * @ingroup io
     */
    int
    jbpf_io_channel_recv_data(struct jbpf_io_channel* channel, jbpf_channel_buf_ptr* data_ptrs, int batch_size);

    /**
     * @brief Same as jbpf_io_channel_recv_data(), with the only difference that it receives a single message, which it
     * copies on another buffer.
     *
     * @param channel A pointer to the target jbpf_io_channel.
     * @param buf The buffer to store the received data.
     * @param buf_size The size of the target buffer.
     * @return int 1 if data was received or -1 otherwise.
     * @ingroup io
     */
    int
    jbpf_io_channel_recv_data_copy(struct jbpf_io_channel* channel, void* buf, size_t buf_size);

    /**
     * @brief Serializes a data buffer of a jbpf_io_channel using the encoder that was passed
     * during the creation of the channel. For IPC, it can be called both by the primary and the secondary process.
     *
     * @param io_ctx A pointer to a jbpf_io ctx.
     * @param data A pointer to an allocated data buffer.
     * @param buf A destination buffer to store the serialized data.
     * @param buf_len The size of the destination buffer.
     * @return int A positive number indicating the size of the serialized message. If no encoder was given when the
     * channel was created, it will return -1 and it will return -2, if the serialization if the message failed.
     * @ingroup io
     */
    int
    jbpf_io_channel_pack_msg(struct jbpf_io_ctx* io_ctx, jbpf_channel_buf_ptr data, void* buf, size_t buf_len);

    /**
     * @brief De-seroalizes an encoded message of a jbpf_io_channel using the decoder that was passed during the
     * creation of the channel. For IPC, it can be called both by the primary and the secondary process.
     *
     * @param io_ctx A pointer to a jbpf_io ctx.
     * @param packed_data A pointer to the encoded data.
     * @param packed_data_size The size of the encoded data buffer.
     * @param stream_id Will store the stream_id of the channel in which the encoded message belongs to, as a return
     * value.
     * @return jbpf_channel_buf_ptr Returns a pointer to a buffer of type jpbf_channel_buf_ptr, that contains the
     * deserialized message.
     */
    jbpf_channel_buf_ptr
    jbpf_io_channel_unpack_msg(
        struct jbpf_io_ctx* io_ctx, void* packed_data, size_t packed_data_size, struct jbpf_io_stream_id* stream_id);

    /**
     * @brief Increments the reference counter of a reserved data pointer of a channel.
     *
     * @param data_ptr The reserved data pointer.
     * @return jbpf_channel_buf_ptr A pointer to the same buffer, with the reference count incremented by one.
     * @ingroup io
     */
    jbpf_channel_buf_ptr
    jbpf_io_channel_share_data_ptr(jbpf_channel_buf_ptr data_ptr);

#ifdef __cplusplus
}
#endif

#endif