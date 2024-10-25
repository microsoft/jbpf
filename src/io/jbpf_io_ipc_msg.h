// Copyright (c) Microsoft Corporation. All rights reserved.
#ifndef JBPF_IO_IPC_MSG_H_
#define JBPF_IO_IPC_MSG_H_

#include <stddef.h>
#include <stdint.h>

#include "jbpf_io_defs.h"

typedef int dipc_fd_t;
typedef int jbpf_ipc_chan_fd_t;

/**
 * @brief IPC message types
 * @ingroup io
 */
typedef enum jbpf_io_ipc_msg_type
{
    JBPF_IO_IPC_REG_REQ = 0,
    JBPF_IO_IPC_REG_RESP,
    JBPF_IO_IPC_DEREG_REQ,
    JBPF_IO_IPC_DEREG_RESP,
    JBPF_IO_IPC_CH_CREATE_REQ,
    JBPF_IO_IPC_CH_CREATE_RESP,
    JBPF_IO_IPC_CH_DESTROY,
    JBPF_IO_IPC_CH_FIND_REQ,
    JBPF_IO_IPC_CH_FIND_RESP,
} dipc_msg_type_t;

/**
 * @brief IPC channel status
 * @ingroup io
 */
typedef enum jbpf_io_ipc_chan_status
{
    JBPF_IO_IPC_CHAN_SUCCESS = 0,
    JBPF_IO_IPC_CHAN_FAIL,
} dipc_chan_status_t;

/**
 * @brief IPC register request
 * @ingroup io
 * @param status status
 * @param alloc_size size of allocated memory
 */
__attribute__((packed)) struct jbpf_io_ipc_reg_req
{
    dipc_reg_status_t status;
    size_t alloc_size;
};

/**
 * @brief IPC register response
 * @ingroup io
 * @param status status
 * @param base_addr base address
 * @param adjusted_alloc_size adjusted allocated size
 * @param mem_name memory name
 */
__attribute__((packed)) struct jbpf_io_ipc_reg_resp
{
    dipc_reg_status_t status;
    void* base_addr;
    size_t adjusted_alloc_size;
    char mem_name[JBPF_IO_IPC_MAX_MEM_NAMELEN];
};

/**
 * @brief IPC channel create request
 * @ingroup io
 * @param chan_request channel request
 */
__attribute__((packed)) struct jbpf_io_ipc_ch_create_req
{
    struct jbpf_io_channel_request chan_request;
};

/**
 * @brief IPC channel create response
 * @ingroup io
 * @param io_channel io channel
 * @param status status
 */
__attribute__((packed)) struct jbpf_io_ipc_ch_create_resp
{
    struct jbpf_io_channel* io_channel;
    dipc_chan_status_t status;
};

/**
 * @brief IPC channel destroy request
 * @ingroup io
 * @param io_channel io channel
 */
__attribute__((packed)) struct jbpf_io_ipc_ch_destroy
{
    struct jbpf_io_channel* io_channel;
};

/**
 * @brief IPC channel find request
 * @ingroup io
 * @param stream_id stream id
 * @param is_output is output?
 */
__attribute__((packed)) struct jbpf_io_ipc_ch_find_req
{
    struct jbpf_io_stream_id stream_id;
    bool is_output;
};

/**
 * @brief IPC channel find response
 * @ingroup io
 * @param io_channel io channel
 */
__attribute__((packed)) struct jbpf_io_ipc_ch_find_resp
{
    struct jbpf_io_channel* io_channel;
};

/**
 * @brief IPC message
 * @ingroup io
 * @param msg_type message type
 * @param msg message
 */
__attribute__((packed)) struct jbpf_io_ipc_msg
{
    dipc_msg_type_t msg_type;
    union
    {
        struct jbpf_io_ipc_reg_req dipc_reg_req;
        struct jbpf_io_ipc_reg_resp dipc_reg_resp;
        struct jbpf_io_ipc_ch_create_req dipc_ch_create_req;
        struct jbpf_io_ipc_ch_create_resp dipc_ch_create_resp;
        struct jbpf_io_ipc_ch_destroy dipc_ch_destroy;
        struct jbpf_io_ipc_ch_find_req dipc_ch_find_req;
        struct jbpf_io_ipc_ch_find_resp dipc_ch_find_resp;
    } msg;
};

#endif