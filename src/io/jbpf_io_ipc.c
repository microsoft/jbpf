// Copyright (c) Microsoft Corporation. All rights reserved.
#include <sys/epoll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#include "jbpf_io_hash.h"

#include "jbpf_io.h"
#include "jbpf_io_ipc.h"
#include "jbpf_io_int.h"
#include "jbpf_io_queue.h"
#include "jbpf_io_channel.h"
#include "jbpf_io_channel_int.h"
#include "jbpf_io_utils.h"
#include "jbpf_io_ipc_int.h"
#include "jbpf_logging.h"

ck_epoch_t list_epoch = {0};
ck_epoch_record_t list_epoch_records[JBPF_IO_MAX_NUM_THREADS];
_Thread_local ck_epoch_record_t* local_list_epoch_record;

jbpf_io_type_t ptype = JBPF_IO_UNKNOWN;

struct local_req_resp
{
    struct jbpf_io_ipc_msg req;
    struct jbpf_io_ipc_msg resp;
    bool request_pending;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

CK_EPOCH_CONTAINER(struct jbpf_io_ipc_peer_ctx, epoch_entry, epoch_container)

static inline bool
_jbpf_io_ipc_add_ctx(struct dipc_peer_list* peer_list, struct jbpf_io_ipc_peer_ctx* peer_ctx)
{

    ck_ht_entry_t entry;
    ck_ht_hash_t h;

    ck_ht_hash_direct(&h, &peer_list->peers, peer_ctx->sock_fd);
    ck_ht_entry_set_direct(&entry, h, peer_ctx->sock_fd, (uintptr_t)peer_ctx);
    return ck_ht_set_spmc(&peer_list->peers, h, &entry);
}

static inline bool
_jbpf_io_ipc_remove_ctx(struct dipc_peer_list* peer_list, struct jbpf_io_ipc_peer_ctx* peer_ctx)
{

    ck_ht_entry_t entry;
    ck_ht_hash_t h;

    ck_ht_hash_direct(&h, &peer_list->peers, peer_ctx->sock_fd);
    ck_ht_entry_key_set_direct(&entry, peer_ctx->sock_fd);
    return ck_ht_remove_spmc(&peer_list->peers, h, &entry);
}

static inline struct jbpf_io_ipc_peer_ctx*
_jbpf_io_ipc_get_ctx(struct dipc_peer_list* peer_list, int reg_fd)
{

    ck_ht_entry_t entry;
    ck_ht_hash_t h;

    ck_ht_hash_direct(&h, &peer_list->peers, reg_fd);
    ck_ht_entry_key_set_direct(&entry, reg_fd);
    if (ck_ht_get_spmc(&peer_list->peers, h, &entry) == true) {
        jbpf_logger(JBPF_INFO, "Found one context with sockfd %d\n", reg_fd);
        return (struct jbpf_io_ipc_peer_ctx*)ck_ht_entry_value_direct(&entry);
    }

    return NULL;
}

static void
_jbpf_io_ipc_force_remove_all_peer_channels(struct jbpf_io_ipc_peer_ctx* peer_ctx)
{
    ck_ht_iterator_t in_iterator = CK_HT_ITERATOR_INITIALIZER;
    ck_ht_iterator_t out_iterator = CK_HT_ITERATOR_INITIALIZER;
    ck_ht_entry_t* cursor;
    struct jbpf_io_channel* io_channel_entry;

    while (ck_ht_next(&peer_ctx->in_channel_list, &in_iterator, &cursor) == true) {
        io_channel_entry = ck_ht_entry_value(cursor);
        jbpf_io_destroy_in_channel(&peer_ctx->io_ctx->primary_ctx.io_channels, io_channel_entry);
    }

    while (ck_ht_next(&peer_ctx->out_channel_list, &out_iterator, &cursor) == true) {
        io_channel_entry = ck_ht_entry_value(cursor);
        jbpf_io_destroy_out_channel(&peer_ctx->io_ctx->primary_ctx.io_channels, io_channel_entry);
    }

    jbpf_io_wait_on_channel_update(&peer_ctx->io_ctx->primary_ctx.io_channels);
}

static void
peer_ctx_destructor(ck_epoch_entry_t* p)
{
    struct jbpf_io_ipc_peer_ctx* peer_ctx = epoch_container(p);

    if (peer_ctx->peer_shm_ctx.mem_ctx)
        jbpf_destroy_mem_ctx(peer_ctx->peer_shm_ctx.mem_ctx, peer_ctx->io_ctx->jbpf_io_path);

    ck_ht_destroy(&peer_ctx->in_channel_list);
    ck_ht_destroy(&peer_ctx->out_channel_list);

    jbpf_free(peer_ctx);
    return;
}

static void
my_free(void* p, size_t m, bool d)
{
    (void)m;
    (void)d;

    jbpf_free(p);
    return;
}

static void*
my_malloc(size_t b)
{
    return jbpf_malloc(b);
}

static void*
my_realloc(void* r, size_t a, size_t b, bool d)
{
    (void)a;
    (void)d;

    return jbpf_realloc(r, b);
}

static struct ck_malloc m = {.malloc = my_malloc, .free = my_free, .realloc = my_realloc};

static void
hash_function(ck_ht_hash_t* h, const void* key, size_t key_length, uint64_t seed)
{
    const uintptr_t* value = key;

    (void)key_length;
    (void)seed;
    h->value = *value;
    return;
}

static void
ht_hash_wrapper(struct ck_ht_hash* h, const void* key, size_t length, uint64_t seed)
{
    h->value = (unsigned long)MurmurHash64A(key, length, seed);
}

void*
dipc_ctrl_thread(void* args)
{
    struct jbpf_io_ipc_cfg* dipc_cfg = args;
    int incoming_req;
    struct sockaddr_vm client_address = {0};
    socklen_t client_address_len;
    struct epoll_event events[MAX_JBPF_EPOLL_EVENTS] = {0};
    int event_count;
    struct jbpf_io_ctx* io_ctx;
    struct jbpf_io_ipc_ctx* ipc_ctx;
    local_req_resp_t* req_resp;

    io_ctx = jbpf_io_get_ctx();
    ipc_ctx = &io_ctx->primary_ctx.ipc_ctx;

    jbpf_io_register_thread();

    char mem_name[JBPF_IO_IPC_MAX_MEM_NAMELEN];
    snprintf(mem_name, sizeof(mem_name), "%s_%s", io_ctx->jbpf_mem_base_name, "local");

    ipc_ctx->local_ctx.registered = true;
    ipc_ctx->local_ctx.mem_ctx =
        jbpf_create_mem_ctx(dipc_cfg->mem_cfg.memory_size, io_ctx->jbpf_mem_base_name, mem_name, 0);
    if (!ipc_ctx->local_ctx.mem_ctx) {
        jbpf_logger(JBPF_ERROR, "Error initializing local memory context\n");
        return NULL;
    }

    pthread_mutex_lock(&ipc_ctx->lock);

    (void)__sync_lock_test_and_set(&ipc_ctx->dipc_ctrl_thread_run, true);

    pthread_cond_signal(&ipc_ctx->cond);

    pthread_mutex_unlock(&ipc_ctx->lock);

    while (ipc_ctx->dipc_ctrl_thread_run) {
        event_count = epoll_wait(ipc_ctx->dipc_epoll_fd, events, MAX_JBPF_EPOLL_EVENTS, 500);

        // First check events coming from the UNIX socket
        for (int i = 0; i < event_count; i++) {
            bool init_req = false;
            if (events->data.fd == ipc_ctx->dipc_ctrl_fd) {
                jbpf_logger(JBPF_INFO, "Received new incoming connection from control channel\n");
                init_req = true;
                client_address_len = sizeof(client_address);
                incoming_req = accept(ipc_ctx->dipc_ctrl_fd, (struct sockaddr*)&client_address, &client_address_len);
                if (incoming_req == -1) {
                    perror("accept");
                    exit(-1);
                }
            } else {
                incoming_req = events->data.fd;
            }
            struct jbpf_io_ipc_msg ipc_msg_recv;
            ssize_t num_bytes = recv_all(incoming_req, &ipc_msg_recv, sizeof(struct jbpf_io_ipc_msg), MSG_WAITALL);
            if (num_bytes != sizeof(struct jbpf_io_ipc_msg)) {
                jbpf_logger(JBPF_INFO, "Peer is down. Tearing down the connection\n");
                if (!init_req)
                    jbpf_io_ipc_remove_peer(incoming_req, io_ctx);
                close(incoming_req);
                continue;
            }
            jbpf_io_ipc_handle_msg(incoming_req, io_ctx, &ipc_msg_recv, init_req);
        }

        // Now we check local requests
        while (ck_ring_dequeue_mpmc(&ipc_ctx->local_queue->req_queue, ipc_ctx->local_queue->buffer, &req_resp)) {
            jbpf_logger(JBPF_INFO, "Got a local request\n");
            pthread_mutex_lock(&req_resp->mutex);
            jbpf_io_ipc_handle_local_msg(io_ctx, req_resp);
            req_resp->request_pending = false;
            pthread_cond_signal(&req_resp->cond);
            pthread_mutex_unlock(&req_resp->mutex);
        }
    }
    jbpf_destroy_mem_ctx(ipc_ctx->local_ctx.mem_ctx, io_ctx->jbpf_io_path);
    free(dipc_cfg);
    return NULL;
}

// Parser for the IPC address
// The address should be in the format <protocol>://<address>, where protocol is one of "vsock" or "unix" (case
// insensitive). If <protocol> is unix, then <address> is a string with the name of the socket. If <protocol> is vsock,
// then <address> has the format cid:port. If CID is a name or 0, then VMADDR_CID_ANY is used instead, while if the port
// is omitted, JBPF_IO_IPC_VSOCK_DEFAULT_PORT is used instead return 0 on success or -1 if failed.
int
_jbpf_io_ipc_parse_addr(const char* jbpf_run_path, const char* addr, struct jbpf_io_ipc_proto_addr* dipc_primary_addr)
{
    size_t protocol_length;
    char *protocol, *cid;

    if (!dipc_primary_addr) {
        return -1;
    }

    char* protocol_start = strstr(addr, "://");

    if (!protocol_start) {
        // If no protocol is given, then assume UNIX socket
        dipc_primary_addr->type = JBPF_IO_IPC_TYPE_UNIX;
        dipc_primary_addr->un_addr.sun_family = AF_UNIX;
        snprintf(
            dipc_primary_addr->un_addr.sun_path,
            sizeof(dipc_primary_addr->un_addr.sun_path),
            "%s/%s",
            jbpf_run_path,
            addr);
        return -1;
    }

    protocol_length = protocol_start - addr;
    protocol_start += 3; // Move after the :// characters

    protocol = calloc(1, protocol_length + 1);
    if (!protocol) {
        dipc_primary_addr->type = JBPF_IO_IPC_TYPE_INVALID;
        return -1;
    }

    strncpy(protocol, addr, protocol_length);
    protocol[protocol_length] = '\0';

    if (strcasecmp(protocol, "unix") == 0) {
        dipc_primary_addr->type = JBPF_IO_IPC_TYPE_UNIX;
        dipc_primary_addr->un_addr.sun_family = AF_UNIX;
        snprintf(
            dipc_primary_addr->un_addr.sun_path,
            sizeof(dipc_primary_addr->un_addr.sun_path),
            "%s/%s",
            jbpf_run_path,
            protocol_start);
        jbpf_logger(JBPF_INFO, "Using UNIX socket with path %s\n", dipc_primary_addr->un_addr.sun_path);
    } else if (strcasecmp(protocol, "vsock") == 0) {
        dipc_primary_addr->type = JBPF_IO_IPC_TYPE_VSOCK;
        dipc_primary_addr->vsock_addr.svm_family = AF_VSOCK;
        // dipc_primary_addr->vsock_addr.svm_cid = JBPF_IO_IPC_CID_DEFAULT;

        char* port_separator = strchr(protocol_start, ':');

        if (!port_separator) {
            cid = calloc(1, strlen(protocol_start) + 1);
            strcpy(cid, protocol_start);
            dipc_primary_addr->vsock_addr.svm_cid = atoi(cid);
            free(cid);
            if (!dipc_primary_addr->vsock_addr.svm_cid) {
                dipc_primary_addr->vsock_addr.svm_cid = JBPF_IO_IPC_VSOCK_CID_DEFAULT;
            }
            dipc_primary_addr->vsock_addr.svm_port = JBPF_IO_IPC_VSOCK_DEFAULT_PORT;
        } else {
            cid = calloc(1, port_separator - protocol_start + 1);
            if (!cid) {
                dipc_primary_addr->type = JBPF_IO_IPC_TYPE_INVALID;
                goto out;
            }
            strncpy(cid, protocol_start, port_separator - protocol_start);
            dipc_primary_addr->vsock_addr.svm_cid = atoi(cid);
            free(cid);
            if (!dipc_primary_addr->vsock_addr.svm_cid) {
                dipc_primary_addr->vsock_addr.svm_cid = JBPF_IO_IPC_VSOCK_CID_DEFAULT;
            }
            dipc_primary_addr->vsock_addr.svm_port = atoi(port_separator + 1);

            if (!dipc_primary_addr->vsock_addr.svm_port) {
                dipc_primary_addr->type = JBPF_IO_IPC_TYPE_INVALID;
                goto out;
            }
        }
        jbpf_logger(
            JBPF_INFO,
            "Using a vsock with CID:port %d:%d\n",
            dipc_primary_addr->vsock_addr.svm_cid,
            dipc_primary_addr->vsock_addr.svm_port);
    } else {
        dipc_primary_addr->type = JBPF_IO_IPC_TYPE_INVALID;
    }

out:
    free(protocol);
    return dipc_primary_addr->type == JBPF_IO_IPC_TYPE_INVALID ? -1 : 0;
}

int
jbpf_io_ipc_init(struct jbpf_io_ipc_cfg* dipc_cfg, struct jbpf_io_ctx* io_ctx)
{

    int ret = 0;
    struct jbpf_io_ipc_proto_addr dipc_primary_addr = {0};
    struct epoll_event event = {0};
    struct jbpf_io_ipc_ctx* ipc_ctx;
    struct jbpf_io_ipc_cfg* ipc_cfg;
    struct sockaddr* saddr;
    size_t saddr_size = 0;
    int sock_domain;

    ipc_cfg = calloc(1, sizeof(struct jbpf_io_ipc_cfg));

    if (!ipc_cfg) {
        return -1;
    }

    memcpy(ipc_cfg, dipc_cfg, sizeof(struct jbpf_io_ipc_cfg));

    io_ctx->io_type = JBPF_IO_IPC_PRIMARY;
    ipc_ctx = &io_ctx->primary_ctx.ipc_ctx;

    if (pthread_cond_init(&ipc_ctx->cond, NULL) != 0) {
        ret = -1;
        goto out;
    }

    if (pthread_mutex_init(&ipc_ctx->lock, NULL) != 0) {
        ret = -1;
        goto out;
    }

    ipc_ctx->dipc_peer_list = calloc(1, sizeof(struct dipc_peer_list));

    if (ck_ht_init(
            &ipc_ctx->dipc_peer_list->peers,
            CK_HT_MODE_DIRECT,
            hash_function,
            &m,
            MAX_NUM_JBPF_IO_IPC_PEERS,
            lrand48()) == false) {
        jbpf_logger(JBPF_ERROR, "Error creating list of peer contexts\n");
        ret = -1;
        goto out;
    }

    io_ctx->primary_ctx.ipc_ctx.local_queue = calloc(1, sizeof(struct local_channel_req_queue));
    if (!io_ctx->primary_ctx.ipc_ctx.local_queue) {
        jbpf_logger(JBPF_ERROR, "Error allocating memory for local channel request queue\n");
        goto out;
    }

    // Initialize local queue of channel creation requests
    ck_ring_init(&io_ctx->primary_ctx.ipc_ctx.local_queue->req_queue, MAX_NUMBER_JBPF_IPC_LOCAL_REQS + 1);

    ck_epoch_init(&list_epoch);

    for (int i = 0; i < JBPF_IO_MAX_NUM_THREADS; i++) {
        ck_epoch_register(&list_epoch, &list_epoch_records[i], NULL);
    }

    (void)__sync_lock_test_and_set(&ipc_ctx->dipc_ctrl_thread_run, false);

    _jbpf_io_ipc_parse_addr(io_ctx->jbpf_io_path, ipc_cfg->addr.jbpf_io_ipc_name, &dipc_primary_addr);

    if (dipc_primary_addr.type == JBPF_IO_IPC_TYPE_UNIX) {
        saddr = (struct sockaddr*)&dipc_primary_addr.un_addr;
        saddr_size = sizeof(dipc_primary_addr.un_addr);
        sock_domain = AF_UNIX;
        unlink(dipc_primary_addr.un_addr.sun_path); // Remove any existing socket file
    } else if (dipc_primary_addr.type == JBPF_IO_IPC_TYPE_VSOCK) {
        saddr = (struct sockaddr*)&dipc_primary_addr.vsock_addr;
        saddr_size = sizeof(dipc_primary_addr.vsock_addr);
        sock_domain = AF_VSOCK;
    } else {
        jbpf_logger(JBPF_ERROR, "Unsupported protocol type %s\n", ipc_cfg->addr.jbpf_io_ipc_name);
        ret = -1;
        goto out;
    }

    ipc_ctx->dipc_ctrl_fd = socket(sock_domain, SOCK_STREAM, 0);
    if (ipc_ctx->dipc_ctrl_fd == -1) {
        jbpf_logger(JBPF_ERROR, "Error opening control socket\n");
        ret = -1;
        goto out;
    }

    ipc_ctx->dipc_epoll_fd = epoll_create1(0);

    if (ipc_ctx->dipc_epoll_fd == -1) {
        jbpf_logger(JBPF_ERROR, "Error creating epoll_fd\n");
        ret = -1;
        goto close_sock;
    }

    event.events = EPOLLIN | EPOLLRDHUP;
    event.data.fd = ipc_ctx->dipc_ctrl_fd;

    if (epoll_ctl(ipc_ctx->dipc_epoll_fd, EPOLL_CTL_ADD, ipc_ctx->dipc_ctrl_fd, &event)) {
        jbpf_logger(JBPF_ERROR, "Error adding socket fd to epoll\n");
        ret = -1;
        goto close_epoll;
    }

    if (bind(ipc_ctx->dipc_ctrl_fd, saddr, saddr_size) == -1) {
        jbpf_logger(JBPF_ERROR, "Error binding to socket: Please make sure you have access rights.\n");
        ret = -1;
        goto close_epoll;
    }

    if (listen(ipc_ctx->dipc_ctrl_fd, JBPF_IO_IPC_CTL_BACKLOG) == -1) {
        ret = -1;
        goto close_epoll;
    }

    if (dipc_primary_addr.type == JBPF_IO_IPC_TYPE_UNIX) {
        if (chmod(dipc_primary_addr.un_addr.sun_path, S_IRWXU | S_IRWXG) == -1) {
            ret = -1;
            goto unlink_socket;
        }
    }

    /* Create pthread for the control channel */
    pthread_create(&ipc_ctx->dipc_ctrl_thread_id, NULL, dipc_ctrl_thread, (void*)ipc_cfg);

    pthread_mutex_lock(&ipc_ctx->lock);

    /* Wait for thread to be ready */
    while (!ipc_ctx->dipc_ctrl_thread_run) {
        pthread_cond_wait(&ipc_ctx->cond, &ipc_ctx->lock);
    }

    pthread_mutex_unlock(&ipc_ctx->lock);

    jbpf_logger(JBPF_DEBUG, "Primary initialization successful\n");

    ptype = JBPF_IO_IPC_PRIMARY;
    goto out;

unlink_socket:
    if (dipc_primary_addr.type == JBPF_IO_IPC_TYPE_UNIX) {
        unlink(dipc_primary_addr.un_addr.sun_path);
    }
close_epoll:
    close(ipc_ctx->dipc_epoll_fd);
close_sock:
    close(ipc_ctx->dipc_ctrl_fd);
out:
    return ret;
}

void
jbpf_io_ipc_stop(struct jbpf_io_ctx* io_ctx)
{
    // TODO
    jbpf_logger(JBPF_WARN, "Stop of primary is not implemented yet\n");
}

void
jbpf_io_ipc_remove_peer(int sock_fd, struct jbpf_io_ctx* io_ctx)
{

    struct jbpf_io_ipc_peer_ctx* peer_ctx;

    ck_epoch_begin(local_list_epoch_record, NULL);

    epoll_ctl(io_ctx->primary_ctx.ipc_ctx.dipc_epoll_fd, EPOLL_CTL_DEL, sock_fd, NULL);

    peer_ctx = _jbpf_io_ipc_get_ctx(io_ctx->primary_ctx.ipc_ctx.dipc_peer_list, sock_fd);

    if (!peer_ctx) {
        jbpf_logger(JBPF_ERROR, "Could not find peer context for fd %d\n", sock_fd);
        return;
    }

    _jbpf_io_ipc_remove_ctx(io_ctx->primary_ctx.ipc_ctx.dipc_peer_list, peer_ctx);
    _jbpf_io_ipc_force_remove_all_peer_channels(peer_ctx);

    ck_epoch_end(local_list_epoch_record, NULL);

    ck_epoch_call(local_list_epoch_record, &peer_ctx->epoch_entry, peer_ctx_destructor);
    ck_epoch_barrier(local_list_epoch_record);

    close(sock_fd);
}

int
jbpf_io_ipc_handle_local_msg(struct jbpf_io_ctx* io_ctx, local_req_resp_t* req_resp)
{
    int res = -1;

    switch (req_resp->req.msg_type) {
    case JBPF_IO_IPC_REG_REQ:
    case JBPF_IO_IPC_DEREG_REQ:
        jbpf_logger(JBPF_ERROR, "Local request of type %d not supported\n", req_resp->req.msg_type);
        break;
    case JBPF_IO_IPC_CH_CREATE_REQ:
        res = jbpf_io_ipc_handle_local_ch_create_req(io_ctx, &req_resp->req.msg.dipc_ch_create_req, &req_resp->resp);
        break;
    case JBPF_IO_IPC_CH_DESTROY:
        jbpf_io_ipc_handle_local_ch_destroy(io_ctx, &req_resp->req.msg.dipc_ch_destroy);
        res = 1;
        break;
    default:
        break;
    }

    return res;
}

int
jbpf_io_ipc_handle_msg(int sock_fd, struct jbpf_io_ctx* io_ctx, struct jbpf_io_ipc_msg* dipc_msg, bool is_init)
{
    int res = -1;

    if (is_init && dipc_msg->msg_type != JBPF_IO_IPC_REG_REQ) {
        jbpf_logger(JBPF_ERROR, "Unexpected message during initialization for fd %d\n", sock_fd);
        close(sock_fd);
        return res;
    }

    switch (dipc_msg->msg_type) {
    case JBPF_IO_IPC_REG_REQ:
        res = jbpf_io_ipc_handle_reg_req(sock_fd, io_ctx, &dipc_msg->msg.dipc_reg_req, is_init);
        break;
    case JBPF_IO_IPC_DEREG_REQ:
        jbpf_io_ipc_handle_dereg_req(sock_fd, io_ctx);
        res = 1;
        break;
    case JBPF_IO_IPC_CH_CREATE_REQ:
        res = jbpf_io_ipc_handle_ch_create_req(sock_fd, io_ctx, &dipc_msg->msg.dipc_ch_create_req);
        break;
    case JBPF_IO_IPC_CH_DESTROY:
        jbpf_io_ipc_handle_ch_destroy(sock_fd, io_ctx, &dipc_msg->msg.dipc_ch_destroy);
        res = 1;
        break;
    case JBPF_IO_IPC_CH_FIND_REQ:
        res = jbpf_io_ipc_handle_ch_find_req(sock_fd, io_ctx, &dipc_msg->msg.dipc_ch_find_req);
        break;
    default:
        break;
    }

    return res;
}

int
jbpf_io_ipc_handle_reg_req(
    int sock_fd, struct jbpf_io_ctx* io_ctx, struct jbpf_io_ipc_reg_req* dipc_reg_req, bool is_init)
{

    int res = 0;
    struct jbpf_io_ipc_msg dipc_reg_resp = {0};

    if (is_init && dipc_reg_req->status != JBPF_IO_IPC_REG_INIT) {
        jbpf_logger(JBPF_ERROR, "Malformed init message from fd %d\n", sock_fd);
        close(sock_fd);
        return -1;
    }

    dipc_reg_resp.msg_type = JBPF_IO_IPC_REG_RESP;

    switch (dipc_reg_req->status) {
    case JBPF_IO_IPC_REG_INIT:
        if (!is_init) {
            close(sock_fd);
            return -1;
        } else {
            res = jbpf_io_ipc_reg_init(sock_fd, io_ctx, dipc_reg_req, &dipc_reg_resp.msg.dipc_reg_resp);
        }
        break;
    case JBPF_IO_IPC_REG_NEG_MMAP_SUCC:
    case JBPF_IO_IPC_REG_NEG_MMAP_FAIL:
        res = jbpf_io_ipc_reg_neg(sock_fd, io_ctx, dipc_reg_req, &dipc_reg_resp.msg.dipc_reg_resp);
        break;
    default:
        jbpf_logger(JBPF_ERROR, "Unexpected registration message from sock_fd %d\n", sock_fd);
        close(sock_fd);
        res = -1;
        goto out;
        break;
    }
    if (send_all(sock_fd, &dipc_reg_resp, sizeof(struct jbpf_io_ipc_msg), 0) != sizeof(struct jbpf_io_ipc_msg)) {
        jbpf_logger(JBPF_ERROR, "Error while sending response to fd %d\n", sock_fd);
        close(sock_fd);
        res = -1;
    }
out:
    if (res < 0)
        jbpf_io_ipc_remove_peer(sock_fd, io_ctx);

    return res;
}

void
jbpf_io_ipc_handle_dereg_req(int sock_fd, struct jbpf_io_ctx* io_ctx)
{
    struct jbpf_io_ipc_msg dipc_dereg_resp;

    dipc_dereg_resp.msg_type = JBPF_IO_IPC_DEREG_RESP;

    jbpf_io_ipc_remove_peer(sock_fd, io_ctx);

    send_all(sock_fd, &dipc_dereg_resp, sizeof(struct jbpf_io_ipc_msg), 0);
    close(sock_fd);
}

int
jbpf_io_ipc_handle_local_ch_create_req(
    struct jbpf_io_ctx* io_ctx, struct jbpf_io_ipc_ch_create_req* chan_req, struct jbpf_io_ipc_msg* dipc_ch_create_resp)
{

    int res = 0;

    struct jbpf_io_ipc_ch_create_resp* chan_resp;
    chan_resp = &dipc_ch_create_resp->msg.dipc_ch_create_resp;

    ck_epoch_begin(local_list_epoch_record, NULL);

    dipc_ch_create_resp->msg_type = JBPF_IO_IPC_CH_CREATE_RESP;
    chan_resp->io_channel = _jbpf_io_create_channel(
        &io_ctx->primary_ctx.io_channels, &chan_req->chan_request, io_ctx->primary_ctx.ipc_ctx.local_ctx.mem_ctx);

    if (!chan_resp->io_channel) {
        chan_resp->status = JBPF_IO_IPC_CHAN_FAIL;
        res = -1;
    } else {
        chan_resp->status = JBPF_IO_IPC_CHAN_SUCCESS;
    }

    ck_epoch_end(local_list_epoch_record, NULL);
    return res;
}

int
jbpf_io_ipc_handle_ch_create_req(int sock_fd, struct jbpf_io_ctx* io_ctx, struct jbpf_io_ipc_ch_create_req* chan_req)
{

    struct jbpf_io_ipc_msg dipc_ch_create_resp = {0};
    int res = -1;
    struct jbpf_io_ipc_peer_ctx* peer_ctx;
    struct jbpf_io_ipc_ch_create_resp* chan_resp;
    ck_ht_hash_t h;
    ck_ht_entry_t entry;

    jbpf_logger(JBPF_INFO, "Received channel create request\n");
    dipc_ch_create_resp.msg_type = JBPF_IO_IPC_CH_CREATE_RESP;
    chan_resp = &dipc_ch_create_resp.msg.dipc_ch_create_resp;

    ck_epoch_begin(local_list_epoch_record, NULL);

    peer_ctx = _jbpf_io_ipc_get_ctx(io_ctx->primary_ctx.ipc_ctx.dipc_peer_list, sock_fd);
    if (!peer_ctx) {
        chan_resp->status = JBPF_IO_IPC_CHAN_FAIL;
        res = -1;
        goto out;
    }

    chan_resp->io_channel = _jbpf_io_create_channel(
        &io_ctx->primary_ctx.io_channels, &chan_req->chan_request, peer_ctx->peer_shm_ctx.mem_ctx);

    if (!chan_resp->io_channel) {
        chan_resp->status = JBPF_IO_IPC_CHAN_FAIL;
    } else {
        chan_resp->status = JBPF_IO_IPC_CHAN_SUCCESS;
        // Add channel to IPC list
        if (chan_req->chan_request.direction == JBPF_IO_CHANNEL_OUTPUT) {

            ck_ht_hash(&h, &peer_ctx->out_channel_list, chan_resp->io_channel->stream_id.id, JBPF_IO_STREAM_ID_LEN);
            ck_ht_entry_set(
                &entry, h, chan_resp->io_channel->stream_id.id, JBPF_IO_STREAM_ID_LEN, chan_resp->io_channel);
            ck_ht_put_spmc(&peer_ctx->out_channel_list, h, &entry);
        } else if (chan_req->chan_request.direction == JBPF_IO_CHANNEL_INPUT) {
            ck_ht_hash(&h, &peer_ctx->in_channel_list, chan_resp->io_channel->stream_id.id, JBPF_IO_STREAM_ID_LEN);
            ck_ht_entry_set(
                &entry, h, chan_resp->io_channel->stream_id.id, JBPF_IO_STREAM_ID_LEN, chan_resp->io_channel);
            ck_ht_put_spmc(&peer_ctx->in_channel_list, h, &entry);
        }
    }

out:
    if (send_all(sock_fd, &dipc_ch_create_resp, sizeof(struct jbpf_io_ipc_msg), 0) != sizeof(struct jbpf_io_ipc_msg)) {
        jbpf_logger(JBPF_ERROR, "Error while sending response to fd %d\n", sock_fd);
        close(sock_fd);
        res = -1;
    }

    ck_epoch_end(local_list_epoch_record, NULL);

    return res;
}

int
jbpf_io_ipc_handle_ch_find_req(int sock_fd, struct jbpf_io_ctx* io_ctx, struct jbpf_io_ipc_ch_find_req* find_req)
{
    struct jbpf_io_ipc_msg dipc_ch_find_resp;
    struct jbpf_io_ipc_ch_find_resp* find_resp;
    struct jbpf_io_ipc_peer_ctx* peer_ctx;
    ck_ht_hash_t h;
    ck_ht_entry_t entry;
    int res = 0;
    bool channel_exists;

    dipc_ch_find_resp.msg_type = JBPF_IO_IPC_CH_FIND_RESP;
    find_resp = &dipc_ch_find_resp.msg.dipc_ch_find_resp;

    // TODO: Need to check if this channel is accessible from secondary. Otherwise we should return NULL;
    ck_epoch_begin(local_list_epoch_record, NULL);

    peer_ctx = _jbpf_io_ipc_get_ctx(io_ctx->primary_ctx.ipc_ctx.dipc_peer_list, sock_fd);
    if (!peer_ctx) {
        find_resp->io_channel = NULL;
        res = -1;
    }

    if (find_req->is_output) {
        ck_ht_hash(&h, &peer_ctx->out_channel_list, find_req->stream_id.id, JBPF_IO_STREAM_ID_LEN);
        ck_ht_entry_key_set(&entry, find_req->stream_id.id, JBPF_IO_STREAM_ID_LEN);
        channel_exists = ck_ht_get_spmc(&peer_ctx->out_channel_list, h, &entry);
        if (channel_exists) {
            find_resp->io_channel = ck_ht_entry_value(&entry);
        } else {
            find_resp->io_channel = NULL;
        }
    } else {
        ck_ht_hash(&h, &peer_ctx->in_channel_list, find_req->stream_id.id, JBPF_IO_STREAM_ID_LEN);
        ck_ht_entry_key_set(&entry, find_req->stream_id.id, JBPF_IO_STREAM_ID_LEN);
        channel_exists = ck_ht_get_spmc(&peer_ctx->in_channel_list, h, &entry);
        if (channel_exists) {
            find_resp->io_channel = ck_ht_entry_value(&entry);
        } else {
            find_resp->io_channel = NULL;
        }
    }

    if (send_all(sock_fd, &dipc_ch_find_resp, sizeof(struct jbpf_io_ipc_msg), 0) != sizeof(struct jbpf_io_ipc_msg)) {
        printf("Error while sending response to fd %d\n", sock_fd);
        close(sock_fd);
        res = -1;
    }

    ck_epoch_end(local_list_epoch_record, NULL);

    return res;
}

void
jbpf_io_ipc_handle_local_ch_destroy(struct jbpf_io_ctx* io_ctx, struct jbpf_io_ipc_ch_destroy* destroy_req)
{
    ck_epoch_begin(local_list_epoch_record, NULL);

    if (destroy_req->io_channel->direction == JBPF_IO_CHANNEL_OUTPUT) {
        jbpf_io_destroy_out_channel(&io_ctx->primary_ctx.io_channels, destroy_req->io_channel);
    } else if (destroy_req->io_channel->direction == JBPF_IO_CHANNEL_INPUT) {
        jbpf_io_destroy_in_channel(&io_ctx->primary_ctx.io_channels, destroy_req->io_channel);
    }

    ck_epoch_end(local_list_epoch_record, NULL);
}

void
jbpf_io_ipc_handle_ch_destroy(int sock_fd, struct jbpf_io_ctx* io_ctx, struct jbpf_io_ipc_ch_destroy* destroy_req)
{
    struct jbpf_io_ipc_peer_ctx* peer_ctx;
    ck_ht_entry_t entry;
    ck_ht_hash_t h;

    ck_epoch_begin(local_list_epoch_record, NULL);

    peer_ctx = _jbpf_io_ipc_get_ctx(io_ctx->primary_ctx.ipc_ctx.dipc_peer_list, sock_fd);

    if (!peer_ctx) {
        ck_epoch_end(local_list_epoch_record, NULL);
        return;
    }

    if (destroy_req->io_channel->direction == JBPF_IO_CHANNEL_OUTPUT) {
        // Remove the entry from the peer_ctx
        ck_ht_hash(&h, &peer_ctx->out_channel_list, destroy_req->io_channel->stream_id.id, JBPF_IO_STREAM_ID_LEN);
        ck_ht_entry_key_set(&entry, destroy_req->io_channel->stream_id.id, JBPF_IO_STREAM_ID_LEN);
        ck_ht_remove_spmc(&peer_ctx->out_channel_list, h, &entry);

        // jbpf_io_destroy_out_channel(&io_ctx->primary_ctx.io_channels, destroy_req->io_channel);
    } else if (destroy_req->io_channel->direction == JBPF_IO_CHANNEL_INPUT) {
        ck_ht_hash(&h, &peer_ctx->in_channel_list, destroy_req->io_channel->stream_id.id, JBPF_IO_STREAM_ID_LEN);
        ck_ht_entry_key_set(&entry, destroy_req->io_channel->stream_id.id, JBPF_IO_STREAM_ID_LEN);
        ck_ht_remove_spmc(&peer_ctx->in_channel_list, h, &entry);

        // jbpf_io_destroy_in_channel(&io_ctx->primary_ctx.io_channels, destroy_req->io_channel);
    }

    jbpf_io_ipc_handle_local_ch_destroy(io_ctx, destroy_req);

    ck_epoch_end(local_list_epoch_record, NULL);
}

int
jbpf_io_ipc_reg_init(
    int sock_fd,
    struct jbpf_io_ctx* io_ctx,
    struct jbpf_io_ipc_reg_req* dipc_reg_req,
    struct jbpf_io_ipc_reg_resp* dipc_reg_resp)
{

    struct epoll_event event;
    struct jbpf_io_ipc_ctx* ipc_ctx;
    struct jbpf_io_ipc_peer_ctx* peer_ctx;
    int shm_res;
    int num_peers;

    unsigned int mode = CK_HT_MODE_BYTESTRING | CK_HT_WORKLOAD_DELETE;

    ipc_ctx = &io_ctx->primary_ctx.ipc_ctx;

    // size_t adjusted_alloc_size = dipc_adjust_alloc_size(dipc_reg_req->alloc_size);

    ck_epoch_begin(local_list_epoch_record, NULL);

    // num_peers = ck_array_length(&ipc_ctx->dipc_peer_list->peers);
    num_peers = ck_ht_count(&ipc_ctx->dipc_peer_list->peers);

    if (num_peers >= MAX_NUM_JBPF_IO_IPC_PEERS) {
        ck_epoch_end(local_list_epoch_record, NULL);
        return -1;
    }

    peer_ctx = jbpf_calloc(1, sizeof(struct jbpf_io_ipc_peer_ctx));

    if (!peer_ctx) {
        ck_epoch_end(local_list_epoch_record, NULL);
        return -1;
    }

    peer_ctx->io_ctx = io_ctx;

    if (ck_ht_init(&peer_ctx->in_channel_list, mode, ht_hash_wrapper, &m, JBPF_IO_MAX_NUM_CHANNELS, 6602834) == false) {
        jbpf_logger(JBPF_ERROR, "Error creating list of in channels\n");
        goto error;
    }

    if (ck_ht_init(&peer_ctx->out_channel_list, mode, ht_hash_wrapper, &m, JBPF_IO_MAX_NUM_CHANNELS, 6602834) ==
        false) {
        jbpf_logger(JBPF_ERROR, "Error creating list of in channels\n");
        goto error;
    }

    jbpf_logger(JBPF_INFO, "Registered new peer with socket fd %d\n", sock_fd);

    peer_ctx->reg_ctx.reg_status = JBPF_IO_IPC_REG_NEG_MMAP;
    peer_ctx->reg_ctx.num_reg_attempts = 1;

    // Add epoll_fd
    event.events = EPOLLIN | EPOLLRDHUP;
    event.data.fd = sock_fd;

    if (epoll_ctl(ipc_ctx->dipc_epoll_fd, EPOLL_CTL_ADD, sock_fd, &event)) {
        jbpf_logger(JBPF_ERROR, "Error adding socket fd %d to epoll\n", sock_fd);
        goto error;
    }

    snprintf(dipc_reg_resp->mem_name, JBPF_IO_IPC_MAX_MEM_NAMELEN, "%s_%d", io_ctx->jbpf_mem_base_name, sock_fd);

    // Open the shared memory to use with the remote process
    shm_res = jbpf_io_ipc_shm_create(
        io_ctx->jbpf_io_path, dipc_reg_resp->mem_name, dipc_reg_req->alloc_size, &peer_ctx->peer_shm_ctx.mmap_info);

    if (shm_res < 0)
        goto error;

    peer_ctx->peer_shm_ctx.tmp_mmap[peer_ctx->reg_ctx.num_reg_attempts - 1] = peer_ctx->peer_shm_ctx.mmap_info;
    dipc_reg_resp->base_addr = peer_ctx->peer_shm_ctx.mmap_info.addr;
    dipc_reg_resp->adjusted_alloc_size = peer_ctx->peer_shm_ctx.mmap_info.len;
    dipc_reg_resp->status = JBPF_IO_IPC_REG_NEG_MMAP;
    peer_ctx->peer_shm_ctx.alloc_mem_size = dipc_reg_req->alloc_size;
    peer_ctx->sock_fd = sock_fd;
    jbpf_logger(JBPF_INFO, "Initiating negotiation with address %p\n", dipc_reg_resp->base_addr);

    if (!_jbpf_io_ipc_add_ctx(ipc_ctx->dipc_peer_list, peer_ctx)) {
        jbpf_logger(JBPF_ERROR, "Error adding peer context for fd %d\n", sock_fd);
        goto error;
    } else {
        jbpf_logger(JBPF_INFO, "Added context for sock_fd %d\n", sock_fd);
    }

    ck_epoch_end(local_list_epoch_record, NULL);
    ck_epoch_barrier(local_list_epoch_record);

    return 1;

error:
    ck_epoch_end(local_list_epoch_record, NULL);
    ck_epoch_call(local_list_epoch_record, &peer_ctx->epoch_entry, peer_ctx_destructor);
    ck_epoch_barrier(local_list_epoch_record);
    dipc_reg_resp->status = JBPF_IO_IPC_REG_FAIL;
    return -1;
}

int
jbpf_io_ipc_reg_neg(
    int sock_fd,
    struct jbpf_io_ctx* io_ctx,
    struct jbpf_io_ipc_reg_req* dipc_reg_req,
    struct jbpf_io_ipc_reg_resp* dipc_reg_resp)
{

    struct jbpf_io_ipc_ctx* ipc_ctx;
    struct jbpf_io_ipc_peer_ctx* peer_ctx;
    int shm_res;
    int res = 1;
    int num_unused_mmaped_regions = 0;
    ipc_ctx = &io_ctx->primary_ctx.ipc_ctx;
    bool release_shm = false;

    ck_epoch_begin(local_list_epoch_record, NULL);

    peer_ctx = _jbpf_io_ipc_get_ctx(ipc_ctx->dipc_peer_list, sock_fd);

    jbpf_logger(JBPF_INFO, "Looked for context for sock_fd %d\n", sock_fd);

    assert(peer_ctx);

    jbpf_logger(JBPF_INFO, "Received negotiation response %d for reg_idx %d\n", dipc_reg_req->status, sock_fd);

    if (dipc_reg_req->status == JBPF_IO_IPC_REG_NEG_MMAP_SUCC) {
        jbpf_logger(JBPF_INFO, "Negotiation was successful\n");
        dipc_reg_resp->status = JBPF_IO_IPC_REG_SUCC;
        dipc_reg_resp->base_addr = peer_ctx->peer_shm_ctx.mmap_info.addr;
        snprintf(dipc_reg_resp->mem_name, JBPF_IO_IPC_MAX_MEM_NAMELEN, "%s_%d", io_ctx->jbpf_mem_base_name, sock_fd);
        dipc_reg_resp->adjusted_alloc_size = peer_ctx->peer_shm_ctx.mmap_info.len;
        peer_ctx->reg_ctx.reg_status = JBPF_IO_IPC_REG_SUCC;
        num_unused_mmaped_regions = peer_ctx->reg_ctx.num_reg_attempts - 1;

        peer_ctx->peer_shm_ctx.mem_ctx =
            jbpf_create_mem_ctx_from_mmap(&peer_ctx->peer_shm_ctx.mmap_info, io_ctx->jbpf_io_path);

        if (!peer_ctx->peer_shm_ctx.mem_ctx) {
            jbpf_logger(JBPF_ERROR, "There seems to be a problem here\n");
            dipc_reg_resp->status = JBPF_IO_IPC_REG_FAIL;
            res = -1;
            num_unused_mmaped_regions = peer_ctx->reg_ctx.num_reg_attempts;
            release_shm = false;
            goto free_unused_mapped_regions;
        } else {
            jbpf_logger(JBPF_INFO, "Heap was created successfully\n");
        }

    } else if (dipc_reg_req->status == JBPF_IO_IPC_REG_NEG_MMAP_FAIL) {
        jbpf_logger(JBPF_INFO, "Negotiation still ongoing\n");
        peer_ctx->reg_ctx.num_reg_attempts++;
        if (peer_ctx->reg_ctx.num_reg_attempts > MAX_NUM_JBPF_IPC_TRY_ATTEMPTS) {
            jbpf_logger(
                JBPF_ERROR, "Negotiation exceeded max number of attempts (%d)\n", MAX_NUM_JBPF_IPC_TRY_ATTEMPTS);
            dipc_reg_resp->status = JBPF_IO_IPC_REG_FAIL;
            res = -1;
            num_unused_mmaped_regions = MAX_NUM_JBPF_IPC_TRY_ATTEMPTS;
            release_shm = true;
            goto free_unused_mapped_regions;
        } else {
            snprintf(
                dipc_reg_resp->mem_name, JBPF_IO_IPC_MAX_MEM_NAMELEN, "%s_%d", io_ctx->jbpf_mem_base_name, sock_fd);

            // Try to mmap a new address without unmapping the old one. Otherwise, the system will mmap the same address
            // space
            shm_res = jbpf_io_ipc_shm_create(
                io_ctx->jbpf_io_path,
                dipc_reg_resp->mem_name,
                peer_ctx->peer_shm_ctx.alloc_mem_size,
                &peer_ctx->peer_shm_ctx.mmap_info);

            if (shm_res < 0) {
                res = -1;
                num_unused_mmaped_regions = peer_ctx->reg_ctx.num_reg_attempts - 1;
                release_shm = true;
                goto free_unused_mapped_regions;
            }

            peer_ctx->peer_shm_ctx.tmp_mmap[peer_ctx->reg_ctx.num_reg_attempts - 1] = peer_ctx->peer_shm_ctx.mmap_info;
            jbpf_logger(JBPF_INFO, "New mmapped region is %p\n", peer_ctx->peer_shm_ctx.mmap_info.addr);

            dipc_reg_resp->base_addr = peer_ctx->peer_shm_ctx.mmap_info.addr;
            dipc_reg_resp->adjusted_alloc_size = peer_ctx->peer_shm_ctx.mmap_info.len;
            dipc_reg_resp->status = JBPF_IO_IPC_REG_NEG_MMAP;
            goto out;
        }
    } else {
        // Abort
        jbpf_logger(JBPF_ERROR, "Negotiation failed due to unknown request\n");
        dipc_reg_resp->status = JBPF_IO_IPC_REG_FAIL;
        res = -1;
    }

free_unused_mapped_regions:
    for (int i = 0; i < num_unused_mmaped_regions && i < MAX_NUM_JBPF_IPC_TRY_ATTEMPTS; i++) {
        int status;

        status = jbpf_free_memory(&peer_ctx->peer_shm_ctx.tmp_mmap[i], io_ctx->jbpf_io_path, release_shm);
        if (status != 0) {
            jbpf_logger(JBPF_ERROR, "Failed to unmap %p\n", peer_ctx->peer_shm_ctx.tmp_mmap[i].addr);
        }
    }

out:
    ck_epoch_end(local_list_epoch_record, NULL);
    return res;
}

int
jbpf_io_ipc_shm_create(char* meta_path_name, char* mem_name, size_t size, struct jbpf_mmap_info* mmap_info)
{

    int flags = 0;
    void* addr;

    flags = JBPF_USE_SHARED_MEM | JBPF_USE_PERS_HUGE_PAGES;

    addr = jbpf_allocate_memory(size, meta_path_name, mem_name, mmap_info, flags);

    if (!addr)
        return -1;

    return 0;
}

int
jbpf_io_ipc_register(struct jbpf_io_ipc_cfg* dipc_cfg, struct jbpf_io_ctx* io_ctx)
{
    struct jbpf_io_ipc_proto_addr primary_address = {0};
    struct jbpf_io_ipc_msg ipc_reg_req = {0}, ipc_reg_resp = {0};
    size_t alloc_size;
    struct jbpf_io_ipc_secondary_desc* ipc_desc;
    struct sockaddr* saddr;
    size_t saddr_len;
    void* addr;
    int counter;
    int socket_domain;

    alloc_size = dipc_cfg->mem_cfg.memory_size;

    if (ptype != JBPF_IO_UNKNOWN) {
        jbpf_logger(JBPF_ERROR, "Cannot register a primary process\n");
    }

    ptype = JBPF_IO_IPC_SECONDARY;
    io_ctx->io_type = JBPF_IO_IPC_SECONDARY;

    ipc_desc = &io_ctx->secondary_ctx.ipc_sec_desc;

    _jbpf_io_ipc_parse_addr(io_ctx->jbpf_io_path, dipc_cfg->addr.jbpf_io_ipc_name, &primary_address);

    if (primary_address.type == JBPF_IO_IPC_TYPE_UNIX) {
        socket_domain = AF_UNIX;
        saddr = (struct sockaddr*)&primary_address.un_addr;
        saddr_len = sizeof(primary_address.un_addr);
    } else if (primary_address.type == JBPF_IO_IPC_TYPE_VSOCK) {
        socket_domain = AF_VSOCK;
        saddr = (struct sockaddr*)&primary_address.vsock_addr;
        saddr_len = sizeof(primary_address.vsock_addr);
    } else {
        jbpf_logger(JBPF_ERROR, "Invalid address\n");
        return -1;
    }

    // Open the socket to the primary process and make a request to register
    ipc_desc->sock_fd = socket(socket_domain, SOCK_STREAM, 0);

    if (ipc_desc->sock_fd == -1) {
        jbpf_logger(JBPF_ERROR, "Error opening socket\n");
        return -1;
    }

    if (connect(ipc_desc->sock_fd, saddr, saddr_len) == -1) {
        // printf("Error connecting to primary at port %d. Errno: %d\n", JBPF_IO_IPC_PORT, errno);
        jbpf_logger(
            JBPF_ERROR,
            "Error connecting to primary (path=%s) (ipc_name=%s)\n",
            io_ctx->jbpf_io_path,
            dipc_cfg->addr.jbpf_io_ipc_name);
        goto sock_close;
    }

    jbpf_logger(JBPF_INFO, "Process connected. Initiating registration\n");

    ipc_reg_req.msg_type = JBPF_IO_IPC_REG_REQ;
    ipc_reg_req.msg.dipc_reg_req.alloc_size = alloc_size;
    int send_res = send_all(ipc_desc->sock_fd, &ipc_reg_req, sizeof(struct jbpf_io_ipc_msg), 0);
    if (send_res == -1) {
        jbpf_logger(
            JBPF_ERROR,
            "Error while sending registration request %d: %d\n",
            send_res,
            (int)sizeof(struct jbpf_io_ipc_msg));
        goto sock_close;
    }

    jbpf_logger(JBPF_INFO, "Sent registration request %d bytes\n", sizeof(struct jbpf_io_ipc_msg));
    int recv_res = recv_all(ipc_desc->sock_fd, &ipc_reg_resp, sizeof(struct jbpf_io_ipc_msg), MSG_WAITALL);
    if (recv_res != sizeof(struct jbpf_io_ipc_msg)) {
        jbpf_logger(
            JBPF_ERROR,
            "Error while receiving registration response %d: %d\n",
            recv_res,
            (int)sizeof(struct jbpf_io_ipc_msg));
        goto sock_close;
    }

    if (ipc_reg_resp.msg_type != JBPF_IO_IPC_REG_RESP ||
        ipc_reg_resp.msg.dipc_reg_resp.status != JBPF_IO_IPC_REG_NEG_MMAP)
        goto sock_close;

    jbpf_logger(JBPF_INFO, "Received registration response with status %d\n", ipc_reg_resp.msg.dipc_reg_resp.status);

    counter = 0;
    addr = NULL;
    while ((ipc_reg_resp.msg.dipc_reg_resp.status == JBPF_IO_IPC_REG_NEG_MMAP) &&
           (counter < MAX_NUM_JBPF_IPC_TRY_ATTEMPTS)) {
        struct jbpf_io_ipc_msg ipc_neg_req;

        jbpf_logger(JBPF_INFO, "Trying to mmap memory in address %p\n", ipc_reg_resp.msg.dipc_reg_resp.base_addr);

        addr = jbpf_attach_memory(
            ipc_reg_resp.msg.dipc_reg_resp.base_addr,
            ipc_reg_resp.msg.dipc_reg_resp.adjusted_alloc_size,
            io_ctx->jbpf_io_path,
            ipc_reg_resp.msg.dipc_reg_resp.mem_name,
            sizeof(ipc_reg_resp.msg.dipc_reg_resp.mem_name));

        //        addr = NULL;

        counter++;
        // This didn't work. Let's try a different address
        if (!addr) {
            jbpf_logger(JBPF_ERROR, "Memory mapping failed\n");
            ipc_neg_req.msg.dipc_reg_req.status = JBPF_IO_IPC_REG_NEG_MMAP_FAIL;
        } else {
            jbpf_logger(JBPF_INFO, "Memory mapping succeeded\n");
            ipc_neg_req.msg.dipc_reg_req.status = JBPF_IO_IPC_REG_NEG_MMAP_SUCC;
        }
        ipc_neg_req.msg_type = JBPF_IO_IPC_REG_REQ;
        jbpf_logger(JBPF_INFO, "Sending notification: %d bytes\n", sizeof(struct jbpf_io_ipc_msg));
        int send_res = send_all(ipc_desc->sock_fd, &ipc_neg_req, sizeof(struct jbpf_io_ipc_msg), 0);
        if (send_res != sizeof(struct jbpf_io_ipc_msg)) {
            jbpf_logger(
                JBPF_ERROR, "Error while sending notification %d: %d\n", send_res, (int)sizeof(struct jbpf_io_ipc_msg));
            goto sock_close;
        }

        jbpf_logger(JBPF_INFO, "Waiting for peer update: %d bytes\n", sizeof(struct jbpf_io_ipc_msg));
        int recv_res = recv_all(ipc_desc->sock_fd, &ipc_reg_resp, sizeof(struct jbpf_io_ipc_msg), MSG_WAITALL);
        if (recv_res != sizeof(struct jbpf_io_ipc_msg)) {
            jbpf_logger(
                JBPF_ERROR,
                "Error while receiving notification %d: %d\n",
                recv_res,
                (int)sizeof(struct jbpf_io_ipc_msg));
            goto sock_close;
        }

        jbpf_logger(JBPF_INFO, "Received peer update with status %d\n", ipc_reg_resp.msg.dipc_reg_resp.status);

        if (ipc_reg_resp.msg_type != JBPF_IO_IPC_REG_RESP)
            goto sock_close;

        if (ipc_reg_resp.msg.dipc_reg_resp.status == JBPF_IO_IPC_REG_SUCC) {
            jbpf_logger(
                JBPF_INFO,
                "Registration succeeded: memory name is %s base address is %p\n",
                ipc_reg_resp.msg.dipc_reg_resp.mem_name,
                ipc_reg_resp.msg.dipc_reg_resp.base_addr);
            strncpy(ipc_desc->mem_name, ipc_reg_resp.msg.dipc_reg_resp.mem_name, sizeof(ipc_desc->mem_name));
            ipc_desc->reg_status = JBPF_IO_IPC_REG_SUCC;
            return 0;
        } else if (ipc_reg_resp.msg.dipc_reg_resp.status == JBPF_IO_IPC_REG_FAIL) {
            jbpf_logger(JBPF_ERROR, "Registration failed\n");
        }
    }

sock_close:
    close(ipc_desc->sock_fd);
    return -1;
}

void
jbpf_io_ipc_deregister(struct jbpf_io_ctx* io_ctx)
{

    struct jbpf_io_ipc_msg ipc_dereg_req = {0}, ipc_dereg_resp = {0};

    if (io_ctx == NULL)
        return;

    if (ptype != JBPF_IO_IPC_SECONDARY) {
        jbpf_logger(JBPF_ERROR, "Cannot deregister a primary process\n");
    }
    ipc_dereg_req.msg_type = JBPF_IO_IPC_DEREG_REQ;
    if (send_all(io_ctx->secondary_ctx.ipc_sec_desc.sock_fd, &ipc_dereg_req, sizeof(struct jbpf_io_ipc_msg), 0) !=
        sizeof(struct jbpf_io_ipc_msg)) {
        jbpf_logger(
            JBPF_ERROR, "Ungraceful deregister of peer with sockfd %d\n", io_ctx->secondary_ctx.ipc_sec_desc.sock_fd);
        return;
    }
    if (recv_all(
            io_ctx->secondary_ctx.ipc_sec_desc.sock_fd, &ipc_dereg_resp, sizeof(struct jbpf_io_ipc_msg), MSG_WAITALL) !=
        sizeof(struct jbpf_io_ipc_msg)) {
        jbpf_logger(JBPF_INFO, "Peer deregistered successfully\n");
    }
}

struct jbpf_io_channel*
jbpf_io_ipc_local_req_create_channel(
    jbpf_io_ctx_t* io_ctx,
    jbpf_io_channel_direction channel_direction,
    jbpf_io_channel_type channel_type,
    int num_elems,
    int elem_size,
    struct jbpf_io_stream_id stream_id,
    char* descriptor,
    size_t descriptor_size)
{

    local_req_resp_t req_resp = {0};
    req_resp.request_pending = true;
    pthread_mutex_init(&req_resp.mutex, NULL);
    pthread_cond_init(&req_resp.cond, NULL);

    req_resp.req.msg_type = JBPF_IO_IPC_CH_CREATE_REQ;
    req_resp.req.msg.dipc_ch_create_req.chan_request.type = channel_type;
    req_resp.req.msg.dipc_ch_create_req.chan_request.num_elems = num_elems;
    req_resp.req.msg.dipc_ch_create_req.chan_request.elem_size = elem_size;
    req_resp.req.msg.dipc_ch_create_req.chan_request.direction = channel_direction;
    if (descriptor) {
        req_resp.req.msg.dipc_ch_create_req.chan_request.descriptor_size = descriptor_size;
        memcpy(req_resp.req.msg.dipc_ch_create_req.chan_request.descriptor, descriptor, descriptor_size);
    } else {
        req_resp.req.msg.dipc_ch_create_req.chan_request.descriptor_size = 0;
    }

    req_resp.req.msg.dipc_ch_create_req.chan_request.stream_id = stream_id;

    // Place request in the ringbuffer and wait for response
    while (ck_ring_enqueue_mpmc(
               &io_ctx->primary_ctx.ipc_ctx.local_queue->req_queue,
               io_ctx->primary_ctx.ipc_ctx.local_queue->buffer,
               &req_resp) == false) {
        ck_pr_stall();
    }

    pthread_cond_signal(&req_resp.cond);

    // Wait for the response
    while (req_resp.request_pending) {
        pthread_cond_wait(&req_resp.cond, &req_resp.mutex);
    }

    if (req_resp.resp.msg_type != JBPF_IO_IPC_CH_CREATE_RESP) {
        jbpf_logger(JBPF_ERROR, "Received wrong message type\n");
        return NULL;
    }

    if (req_resp.resp.msg.dipc_ch_create_resp.status == JBPF_IO_IPC_CHAN_SUCCESS) {
        return req_resp.resp.msg.dipc_ch_create_resp.io_channel;
    } else {
        jbpf_logger(JBPF_ERROR, "Channel was not created\n");
        return NULL;
    }

    return NULL;
}

struct jbpf_io_channel*
jbpf_io_ipc_req_create_channel(
    jbpf_io_ctx_t* io_ctx,
    jbpf_io_channel_direction channel_direction,
    jbpf_io_channel_type channel_type,
    int num_elems,
    int elem_size,
    struct jbpf_io_stream_id stream_id,
    char* descriptor,
    size_t descriptor_size)
{

    struct jbpf_io_channel* io_channel;

    if (!io_ctx) {
        return NULL;
    }

    if (io_ctx->io_type != JBPF_IO_IPC_SECONDARY && ptype != JBPF_IO_IPC_SECONDARY) {
        return NULL;
    }

    // Send a request to the primary process
    struct jbpf_io_ipc_msg ipc_ch_create_req = {0}, ipc_ch_create_resp = {0};

    ipc_ch_create_req.msg_type = JBPF_IO_IPC_CH_CREATE_REQ;
    ipc_ch_create_req.msg.dipc_ch_create_req.chan_request.type = channel_type;
    ipc_ch_create_req.msg.dipc_ch_create_req.chan_request.num_elems = num_elems;
    ipc_ch_create_req.msg.dipc_ch_create_req.chan_request.elem_size = elem_size;
    ipc_ch_create_req.msg.dipc_ch_create_req.chan_request.direction = channel_direction;
    if (descriptor) {
        ipc_ch_create_req.msg.dipc_ch_create_req.chan_request.descriptor_size = descriptor_size;
        memcpy(ipc_ch_create_req.msg.dipc_ch_create_req.chan_request.descriptor, descriptor, descriptor_size);
    } else {
        ipc_ch_create_req.msg.dipc_ch_create_req.chan_request.descriptor_size = 0;
    }

    ipc_ch_create_req.msg.dipc_ch_create_req.chan_request.stream_id = stream_id;

    jbpf_logger(
        JBPF_INFO, "Requesting the creation of new channel for peer %d\n", io_ctx->secondary_ctx.ipc_sec_desc.sock_fd);
    if (send_all(io_ctx->secondary_ctx.ipc_sec_desc.sock_fd, &ipc_ch_create_req, sizeof(struct jbpf_io_ipc_msg), 0) !=
        sizeof(struct jbpf_io_ipc_msg)) {
        return NULL;
    }

    if (recv_all(
            io_ctx->secondary_ctx.ipc_sec_desc.sock_fd,
            &ipc_ch_create_resp,
            sizeof(struct jbpf_io_ipc_msg),
            MSG_WAITALL) != sizeof(struct jbpf_io_ipc_msg)) {
        jbpf_logger(JBPF_ERROR, "Something went wrong\n");
        return NULL;
    }

    if (ipc_ch_create_resp.msg_type != JBPF_IO_IPC_CH_CREATE_RESP) {
        jbpf_logger(JBPF_ERROR, "Received wrong message type\n");
        return NULL;
    }

    if (ipc_ch_create_resp.msg.dipc_ch_create_resp.status == JBPF_IO_IPC_CHAN_SUCCESS) {
        io_channel = ipc_ch_create_resp.msg.dipc_ch_create_resp.io_channel;
        // Channel was created. Let's load the serde for the secondary process
        _jbpf_io_load_serde(&io_channel->secondary_serde, &io_channel->stream_id, descriptor, descriptor_size);
        return io_channel;
    } else {
        jbpf_logger(JBPF_ERROR, "Channel was not created\n");
        return NULL;
    }
}

void
jbpf_io_ipc_local_req_destroy_channel(jbpf_io_ctx_t* io_ctx, struct jbpf_io_channel* io_channel)
{
    if (!io_ctx || !io_channel) {
        return;
    }

    char sname[JBPF_IO_STREAM_ID_LEN * 3];
    _jbpf_io_tohex_str(io_channel->stream_id.id, JBPF_IO_STREAM_ID_LEN, sname, JBPF_IO_STREAM_ID_LEN * 3);

    if (io_ctx->io_type != JBPF_IO_IPC_PRIMARY)
        return;

    local_req_resp_t req_resp;
    req_resp.request_pending = true;
    pthread_mutex_init(&req_resp.mutex, NULL);
    pthread_cond_init(&req_resp.cond, NULL);

    req_resp.req.msg_type = JBPF_IO_IPC_CH_DESTROY;
    req_resp.req.msg.dipc_ch_destroy.io_channel = io_channel;

    // Place request in the ringbuffer and wait for response
    while (ck_ring_enqueue_mpmc(
               &io_ctx->primary_ctx.ipc_ctx.local_queue->req_queue,
               io_ctx->primary_ctx.ipc_ctx.local_queue->buffer,
               &req_resp) == false) {
        ck_pr_stall();
    }

    pthread_cond_signal(&req_resp.cond);

    // Wait for the response
    while (req_resp.request_pending) {
        pthread_cond_wait(&req_resp.cond, &req_resp.mutex);
    }

    jbpf_logger(JBPF_INFO, "Local channel destroy request completed\n");
}

void
jbpf_io_ipc_req_destroy_channel(jbpf_io_ctx_t* io_ctx, struct jbpf_io_channel* io_channel)
{

    char sname[JBPF_IO_STREAM_ID_LEN * 3];

    if (!io_ctx || !io_channel) {
        return;
    }

    _jbpf_io_tohex_str(io_channel->stream_id.id, JBPF_IO_STREAM_ID_LEN, sname, JBPF_IO_STREAM_ID_LEN * 3);

    if (io_ctx->io_type != JBPF_IO_IPC_SECONDARY && ptype != JBPF_IO_IPC_SECONDARY) {
        return;
    }

    // Unload the local serde library (if loaded)
    if (io_channel->secondary_serde.handle) {
        _jbpf_io_unload_lib(io_channel->secondary_serde.handle);
        _jbpf_io_close_mem_fd(io_channel->secondary_serde.lib_fd, io_channel->secondary_serde.name);
    }

    // Send a request to the primary process
    struct jbpf_io_ipc_msg ipc_ch_destroy_req = {0};

    ipc_ch_destroy_req.msg_type = JBPF_IO_IPC_CH_DESTROY;
    ipc_ch_destroy_req.msg.dipc_ch_destroy.io_channel = io_channel;

    jbpf_logger(
        JBPF_INFO,
        "Requesting the destruction of channel %s for peer %d\n",
        sname,
        io_ctx->secondary_ctx.ipc_sec_desc.sock_fd);
    send_all(io_ctx->secondary_ctx.ipc_sec_desc.sock_fd, &ipc_ch_destroy_req, sizeof(struct jbpf_io_ipc_msg), 0);
}

struct jbpf_io_channel*
jbpf_io_ipc_req_find_channel(jbpf_io_ctx_t* io_ctx, struct jbpf_io_stream_id* stream_id, bool is_output)
{
    if (!io_ctx) {
        return NULL;
    }

    if (io_ctx->io_type != JBPF_IO_IPC_SECONDARY && ptype != JBPF_IO_IPC_SECONDARY) {
        return NULL;
    }

    // Send a request to the primary process
    struct jbpf_io_ipc_msg ipc_ch_find_req = {0}, ipc_ch_find_resp = {0};

    ipc_ch_find_req.msg_type = JBPF_IO_IPC_CH_FIND_REQ;
    ipc_ch_find_req.msg.dipc_ch_find_req.is_output = is_output;
    ipc_ch_find_req.msg.dipc_ch_find_req.stream_id = *stream_id;

    if (send_all(io_ctx->secondary_ctx.ipc_sec_desc.sock_fd, &ipc_ch_find_req, sizeof(struct jbpf_io_ipc_msg), 0) !=
        sizeof(struct jbpf_io_ipc_msg)) {
        return NULL;
    }

    if (recv_all(
            io_ctx->secondary_ctx.ipc_sec_desc.sock_fd,
            &ipc_ch_find_resp,
            sizeof(struct jbpf_io_ipc_msg),
            MSG_WAITALL) != sizeof(struct jbpf_io_ipc_msg)) {
        printf("Something went wrong \n");
        return NULL;
    }

    if (ipc_ch_find_resp.msg_type != JBPF_IO_IPC_CH_FIND_RESP) {
        printf("Received wrong message type\n");
        return NULL;
    }

    return ipc_ch_find_resp.msg.dipc_ch_find_resp.io_channel;
}
