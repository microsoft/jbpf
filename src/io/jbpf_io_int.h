#ifndef JBPF_IO_INT_H
#define JBPF_IO_INT_H

#include <pthread.h>

#include "ck_ht.h"
#include "ck_ring.h"
#include "ck_array.h"
#include "ck_epoch.h"

#include "jbpf_common_types.h"

#include "jbpf_io_defs.h"
#include "jbpf_mem_mgmt.h"
#include "jbpf_io_thread_mgmt.h"

// IPC definitions

#define MAX_NUMBER_JBPF_IPC_LOCAL_REQS 32

extern ck_epoch_t list_epoch;
extern _Thread_local ck_epoch_record_t* local_list_epoch_record;
extern ck_epoch_record_t list_epoch_records[JBPF_IO_MAX_NUM_THREADS];

struct jbpf_io_ipc_peer_shm_ctx
{
    struct jbpf_mmap_info mmap_info;
    size_t alloc_mem_size;
    struct jbpf_mmap_info tmp_mmap[MAX_NUM_JBPF_IPC_TRY_ATTEMPTS];
    jbpf_mem_ctx_t* mem_ctx;
};

struct jbpf_io_ipc_peer_ctx
{
    bool registered;
    struct jbpf_io_ipc_reg_ctx reg_ctx;
    int sock_fd;
    struct jbpf_io_ipc_peer_shm_ctx peer_shm_ctx;
    struct jbpf_io_ctx* io_ctx;
    ck_epoch_entry_t epoch_entry;
    ck_ht_t in_channel_list;
    ck_ht_t out_channel_list;
};

struct dipc_peer_list
{
    ck_ht_t peers;
};

struct local_channel_req_queue
{
    ck_ring_t req_queue;
    ck_ring_buffer_t buffer[MAX_NUMBER_JBPF_IPC_LOCAL_REQS];
};

// Channel definitions

extern ck_epoch_t in_channel_list_epoch;
extern ck_epoch_t out_channel_list_epoch;

extern _Thread_local ck_epoch_record_t* local_in_channel_list_epoch_record;
extern _Thread_local ck_epoch_record_t* local_out_channel_list_epoch_record;

extern ck_epoch_record_t in_channel_list_epoch_records[JBPF_IO_MAX_NUM_THREADS];
extern ck_epoch_record_t out_channel_list_epoch_records[JBPF_IO_MAX_NUM_THREADS];

#define JBPF_IO_SERIALIZER_NAME "jbpf_io_serialize"
#define JBPF_IO_DESERIALIZER_NAME "jbpf_io_deserialize"

typedef int (*jbpf_io_serialize_cb)(
    void* input_msg_buf, size_t input_msg_buf_size, char* serialized_data_buf, size_t serialized_data_buf_size);
typedef int (*jbpf_io_deserialize_cb)(
    char* serialized_data_buf, size_t serialized_data_buf_size, void* output_msg_buf, size_t output_msg_buf_size);

struct jbpf_io_serde
{
    jbpf_io_serialize_cb serialize;
    jbpf_io_deserialize_cb deserialize;
    void* handle;
    int lib_fd;
    jbpf_io_channel_name_t name;
};

struct jbpf_io_channel
{
    void* channel_ptr;
    jbpf_io_channel_type type;
    jbpf_io_channel_direction direction;
    jbpf_io_channel_priority priority;
    struct jbpf_io_serde primary_serde;
    struct jbpf_io_serde secondary_serde;
    struct jbpf_io_stream_id stream_id;
    int elem_size;
    ck_epoch_entry_t epoch_entry;
};

struct jbpf_io_in_channel_list
{
    ck_ht_t in_ht;
    struct jbpf_io_channel* in_array[JBPF_IO_MAX_NUM_CHANNELS];
    int num_in_channels;
};

struct jbpf_io_out_channel_list
{
    ck_ht_t out_ht;
    struct jbpf_io_channel* out_array[JBPF_IO_MAX_NUM_CHANNELS];
    int num_out_channels;
};

struct jbpf_io_channel*
jbpf_io_channel_exists(struct jbpf_io_channel_list* channel_list, struct jbpf_io_stream_id* stream_id, bool is_output);

#endif
