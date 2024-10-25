#ifndef JBPF_IO_IPC_H
#define JBPF_IO_IPC_H

#include "jbpf_io_defs.h"
#include "jbpf_io_ipc_msg.h"
#include "jbpf_mem_mgmt.h"

typedef struct local_req_resp local_req_resp_t;

int
jbpf_io_ipc_init(struct jbpf_io_ipc_cfg* dipc_cfg, struct jbpf_io_ctx* io_ctx);

void
jbpf_io_ipc_stop(struct jbpf_io_ctx* io_ctx);

int
jbpf_io_ipc_reg_neg(
    int sock_fd,
    struct jbpf_io_ctx* io_ctx,
    struct jbpf_io_ipc_reg_req* dipc_reg_req,
    struct jbpf_io_ipc_reg_resp* dipc_reg_resp);

int
jbpf_io_ipc_register(struct jbpf_io_ipc_cfg* dipc_cfg, struct jbpf_io_ctx* io_ctx);

void
jbpf_io_ipc_deregister(struct jbpf_io_ctx* io_ctx);

void
jbpf_io_ipc_remove_peer(int sock_fd, struct jbpf_io_ctx* io_ctx);

// TODO
void
jbpf_io_ipc_handle_peer_in_bufs();

// TODO
void
jbpf_io_ipc_handle_peer_out_bufs();

int
jbpf_io_ipc_handle_local_msg(struct jbpf_io_ctx* io_ctx, local_req_resp_t* req_resp);

int
jbpf_io_ipc_handle_msg(int sock_fd, struct jbpf_io_ctx* io_ctx, struct jbpf_io_ipc_msg* dipc_msg, bool is_init);

int
jbpf_io_ipc_handle_reg_req(
    int sock_fd, struct jbpf_io_ctx* io_ctx, struct jbpf_io_ipc_reg_req* dipc_reg_req, bool is_init);

void
jbpf_io_ipc_handle_dereg_req(int sock_fd, struct jbpf_io_ctx* io_ctx);

int
jbpf_io_ipc_handle_local_ch_create_req(
    struct jbpf_io_ctx* io_ctx,
    struct jbpf_io_ipc_ch_create_req* chan_req,
    struct jbpf_io_ipc_msg* dipc_ch_create_resp);

int
jbpf_io_ipc_handle_ch_create_req(int sock_fd, struct jbpf_io_ctx* io_ctx, struct jbpf_io_ipc_ch_create_req* chan_req);

void
jbpf_io_ipc_handle_local_ch_destroy(struct jbpf_io_ctx* io_ctx, struct jbpf_io_ipc_ch_destroy* destroy_req);

void
jbpf_io_ipc_handle_ch_destroy(int sock_fd, struct jbpf_io_ctx* io_ctx, struct jbpf_io_ipc_ch_destroy* destroy_req);

int
jbpf_io_ipc_handle_ch_find_req(int sock_fd, struct jbpf_io_ctx* io_ctx, struct jbpf_io_ipc_ch_find_req* find_req);

int
jbpf_io_ipc_reg_init(
    int sock_fd,
    struct jbpf_io_ctx* io_ctx,
    struct jbpf_io_ipc_reg_req* dipc_reg_req,
    struct jbpf_io_ipc_reg_resp* dipc_reg_resp);

int
jbpf_io_ipc_shm_create(char* meta_path_name, char* mem_name, size_t size, struct jbpf_mmap_info* mmap_info);

struct jbpf_io_channel*
jbpf_io_ipc_local_req_create_channel(
    jbpf_io_ctx_t* io_ctx,
    jbpf_io_channel_direction channel_direction,
    jbpf_io_channel_type channel_type,
    int num_elems,
    int elem_size,
    struct jbpf_io_stream_id stream_id,
    char* descriptor,
    size_t descriptor_size);

struct jbpf_io_channel*
jbpf_io_ipc_req_create_channel(
    jbpf_io_ctx_t* io_ctx,
    jbpf_io_channel_direction channel_direction,
    jbpf_io_channel_type channel_type,
    int num_elems,
    int elem_size,
    struct jbpf_io_stream_id stream_id,
    char* descriptor,
    size_t descriptor_size);

void
jbpf_io_ipc_req_destroy_channel(jbpf_io_ctx_t* io_ctx, struct jbpf_io_channel* io_channel);

void
jbpf_io_ipc_local_req_destroy_channel(jbpf_io_ctx_t* io_ctx, struct jbpf_io_channel* io_channel);

struct jbpf_io_channel*
jbpf_io_ipc_req_find_channel(jbpf_io_ctx_t* io_ctx, struct jbpf_io_stream_id* stream_id, bool is_output);

#endif