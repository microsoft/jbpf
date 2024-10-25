#ifndef JBPF_IO_CHANNEL_INT_H
#define JBPF_IO_CHANNEL_INT_H

#include "jbpf_mem_mgmt.h"

int
jbpf_io_channel_list_init(struct jbpf_io_ctx* io_ctx);

int
jbpf_io_channel_list_destroy(struct jbpf_io_ctx* io_ctx);

bool
_jbpf_io_load_serde(struct jbpf_io_serde* serde, struct jbpf_io_stream_id* stream_id, char* lib_bin, size_t lib_len);

struct jbpf_io_channel*
_jbpf_io_create_channel(
    struct jbpf_io_channel_list* channel_list, struct jbpf_io_channel_request* chan_req, jbpf_mem_ctx_t* mem_ctx);

void
jbpf_io_destroy_out_channel(struct jbpf_io_channel_list* channel_list, struct jbpf_io_channel* io_channel);

void
jbpf_io_destroy_in_channel(struct jbpf_io_channel_list* channel_list, struct jbpf_io_channel* io_channel);

void
jbpf_io_wait_on_channel_update(struct jbpf_io_channel_list* channel_list);

#endif
