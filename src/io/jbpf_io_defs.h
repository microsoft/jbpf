// Copyright (c) Microsoft Corporation. All rights reserved.
#ifndef JBPF_IO_DEFS_H
#define JBPF_IO_DEFS_H

#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <linux/vm_sockets.h>
#include <sys/un.h>

#include "jbpf_mem_mgmt.h"
#include "jbpf_io_channel_defs.h"

#include "jbpf_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /////////////// Local primary defs ///////

    struct jbpf_io_mem_cfg
    {
        size_t memory_size;
    };

    struct jbpf_io_local_ctx
    {
        bool registered;
        jbpf_mem_ctx_t* mem_ctx;
    };

    struct jbpf_io_local_cfg
    {
        struct jbpf_io_mem_cfg mem_cfg;
    };

    /////////////// IPC defs ///////////////

#define JBPF_IO_IPC_MAX_NAMELEN (32U)

#define JBPF_IO_IPC_CTL_BACKLOG (32)

#define MAX_NUM_JBPF_IO_IPC_PEERS (64)

#define MAX_NUM_JBPF_IPC_TRY_ATTEMPTS (10)

#define MAX_JBPF_EPOLL_EVENTS (MAX_NUM_JBPF_IO_IPC_PEERS + 1)

#define JBPF_IO_IPC_VSOCK_DEFAULT_PORT (9999)

#define JBPF_IO_IPC_VSOCK_CID_DEFAULT VMADDR_CID_ANY

#define JBPF_IO_IPC_MAX_BASE_NAMELEN (64U)

#define JBPF_IO_IPC_MAX_MEM_NAMELEN (128U)

    typedef struct dipc_peer_list dipc_peer_list_t;

    typedef enum jbpf_io_ipc_reg_status
    {
        JBPF_IO_IPC_REG_INIT = 0,
        JBPF_IO_IPC_REG_ALLOC_FAIL,
        JBPF_IO_IPC_REG_NEG_MMAP,
        JBPF_IO_IPC_REG_NEG_MMAP_SUCC,
        JBPF_IO_IPC_REG_NEG_MMAP_FAIL,
        JBPF_IO_IPC_REG_SUCC,
        JBPF_IO_IPC_REG_FAIL,
    } dipc_reg_status_t;

    typedef enum jbpf_io_ipc_address_type
    {
        JBPF_IO_IPC_TYPE_UNIX = 0,
        JBPF_IO_IPC_TYPE_VSOCK,
        JBPF_IO_IPC_TYPE_UDP, // Currently unsupported
        JBPF_IO_IPC_TYPE_INVALID
    } dipc_addr_type_t;

    struct jbpf_io_ipc_un_addr
    {
        char socket_name[JBPF_IO_IPC_MAX_NAMELEN];
    };

    struct jbpf_io_ipc_vsock_addr
    {
        unsigned int port;
        unsigned int cid;
    };

    struct jbpf_io_ipc_address
    {
        char jbpf_io_ipc_name[JBPF_IO_IPC_MAX_NAMELEN];
    };

    struct jbpf_io_ipc_proto_addr
    {
        dipc_addr_type_t type;
        union
        {
            struct sockaddr_vm vsock_addr;
            struct sockaddr_un un_addr;
        };
    };

    struct jbpf_io_ipc_cfg
    {
        struct jbpf_io_ipc_address addr;
        struct jbpf_io_mem_cfg mem_cfg;
    };

    struct jbpf_io_ipc_reg_ctx
    {
        dipc_reg_status_t reg_status;
        int num_reg_attempts;
    };

    struct jbpf_io_ipc_secondary_desc
    {
        char mem_name[JBPF_IO_IPC_MAX_NAMELEN];
        int sock_fd;
        dipc_reg_status_t reg_status;
    };

    typedef struct jbpf_io_ipc_peer_ctx jbpf_io_ipc_peer_ctx_t;
    typedef struct local_channel_req_queue local_channel_req_queue_t;

    struct jbpf_io_ipc_ctx
    {
        struct jbpf_io_ipc_proto_addr addr;
        int dipc_ctrl_fd;
        int dipc_epoll_fd;
        bool dipc_ctrl_thread_run;

        dipc_peer_list_t* dipc_peer_list;
        pthread_t dipc_ctrl_thread_id;

        // This is used for local channel requests from the primary
        struct jbpf_io_local_ctx local_ctx;
        local_channel_req_queue_t* local_queue;

        pthread_mutex_t lock;
        pthread_cond_t cond;
    };

    //////////// Top level defs ///////////////

    typedef enum ioq_type
    {
        JBPF_IO_INPUTQ = 0,
        JBPF_IO_OUTPUTQ,
    } ioq_type_t;

    typedef void (*jbpf_io_create_channel_cb)(int chan_desc, uint32_t num_elems, uint32_t elem_size, ioq_type_t type);
    typedef void (*jbpf_io_destroy_channel_cb)(int chan_desc);

    typedef enum jbpf_io_type
    {
        JBPF_IO_LOCAL_PRIMARY = 0,
        JBPF_IO_IPC_PRIMARY,
        JBPF_IO_IPC_SECONDARY,
        JBPF_IO_UNKNOWN,
    } jbpf_io_type_t;

    struct jbpf_io_config
    {
        int type;
        char jbpf_path[JBPF_RUN_PATH_LEN];
        char jbpf_namespace[JBPF_NAMESPACE_LEN];
        union
        {
            struct jbpf_io_ipc_cfg ipc_config;
            struct jbpf_io_local_cfg local_config;
        };
    };

    // TODO Where to we store the channels

    struct jbpf_io_primary_ctx
    {
        union
        {
            struct jbpf_io_ipc_ctx ipc_ctx;
            struct jbpf_io_local_ctx local_ctx;
        };
        struct jbpf_io_channel_list io_channels;
    };

    struct jbpf_io_secondary_ctx
    {
        union
        {
            struct jbpf_io_ipc_secondary_desc ipc_sec_desc;
        };
    };

    struct jbpf_io_ctx
    {
        jbpf_io_type_t io_type;
        char jbpf_io_path[JBPF_PATH_NAMESPACE_LEN];
        char jbpf_mem_base_name[JBPF_IO_IPC_MAX_BASE_NAMELEN];
        union
        {
            struct jbpf_io_primary_ctx primary_ctx;
            struct jbpf_io_secondary_ctx secondary_ctx;
        };
    };

#ifdef __cplusplus
}
#endif

#endif
