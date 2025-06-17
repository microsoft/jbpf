// Copyright (c) Microsoft Corporation. All rights reserved.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <dlfcn.h>
#include <unistd.h>

#include "jbpf_io_hash.h"

#include "jbpf_io_channel.h"
#include "jbpf_io_queue.h"
#include "jbpf_io_utils.h"
#include "jbpf_io_int.h"
#include "jbpf_io_thread_mgmt.h"

#include "jbpf_mempool.h"
#include "jbpf_logging.h"

#define container_of(ptr, type, member)                   \
    ({                                                    \
        const typeof(((type*)0)->member)* __mptr = (ptr); \
        (type*)((char*)__mptr - offsetof(type, member));  \
    })

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

typedef struct jbpf_io_channel_elem
{
    struct jbpf_io_channel* io_channel;
    uint8_t data[];
} jbpf_io_channel_elem_t;

ck_epoch_t in_channel_list_epoch = {0};
ck_epoch_t out_channel_list_epoch = {0};

ck_epoch_record_t in_channel_list_epoch_records[JBPF_IO_MAX_NUM_THREADS];
ck_epoch_record_t out_channel_list_epoch_records[JBPF_IO_MAX_NUM_THREADS];

_Thread_local ck_epoch_record_t* local_in_channel_list_epoch_record;
_Thread_local ck_epoch_record_t* local_out_channel_list_epoch_record;

CK_EPOCH_CONTAINER(struct jbpf_io_channel, epoch_entry, epoch_container)

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
ht_hash_wrapper(struct ck_ht_hash* h, const void* key, size_t length, uint64_t seed)
{
    h->value = (unsigned long)MurmurHash64A(key, length, seed);
}

static void
io_channel_destructor(ck_epoch_entry_t* p)
{
    struct jbpf_io_channel* io_channel = epoch_container(p);

    if (io_channel->primary_serde.handle) {
        _jbpf_io_unload_lib(io_channel->primary_serde.handle);
        _jbpf_io_close_mem_fd(io_channel->primary_serde.lib_fd, io_channel->primary_serde.name);
    }

    if (io_channel->type == JBPF_IO_CHANNEL_QUEUE || io_channel->type == JBPF_IO_CHANNEL_RINGBUF) {
        jbpf_logger(JBPF_INFO, "Releasing queue\n");
        jbpf_io_queue_free(io_channel->channel_ptr);
    }
    jbpf_free(io_channel);
}

int
jbpf_io_channel_list_init(struct jbpf_io_ctx* io_ctx)
{

    unsigned int mode = CK_HT_MODE_BYTESTRING | CK_HT_WORKLOAD_DELETE;

    if (!io_ctx)
        return -1;

    if (io_ctx->io_type != JBPF_IO_IPC_PRIMARY && io_ctx->io_type != JBPF_IO_LOCAL_PRIMARY)
        return -1;

    io_ctx->primary_ctx.io_channels.in_channel_list = calloc(1, sizeof(struct jbpf_io_in_channel_list));
    if (!io_ctx->primary_ctx.io_channels.in_channel_list)
        return -1;

    io_ctx->primary_ctx.io_channels.out_channel_list = calloc(1, sizeof(struct jbpf_io_out_channel_list));
    if (!io_ctx->primary_ctx.io_channels.out_channel_list)
        goto free_in_channel;

    // Initialize channels
    if (ck_ht_init(
            &io_ctx->primary_ctx.io_channels.out_channel_list->out_ht,
            mode,
            ht_hash_wrapper,
            &m,
            JBPF_IO_MAX_NUM_CHANNELS,
            6602834) == false) {
        jbpf_logger(JBPF_ERROR, "Error creating list of out channels\n");
        goto free_out_channel;
    }

    if (ck_ht_init(
            &io_ctx->primary_ctx.io_channels.in_channel_list->in_ht,
            mode,
            ht_hash_wrapper,
            &m,
            JBPF_IO_MAX_NUM_CHANNELS,
            6602834) == false) {
        jbpf_logger(JBPF_ERROR, "Error creating list of in channels\n");
        goto free_out_ht;
    }

    ck_epoch_init(&in_channel_list_epoch);
    ck_epoch_init(&out_channel_list_epoch);

    for (int i = 0; i < JBPF_IO_MAX_NUM_THREADS; i++) {
        ck_epoch_register(&in_channel_list_epoch, &in_channel_list_epoch_records[i], NULL);
        ck_epoch_register(&out_channel_list_epoch, &out_channel_list_epoch_records[i], NULL);
    }

    return 0;

free_out_ht:
    ck_ht_destroy(&io_ctx->primary_ctx.io_channels.out_channel_list->out_ht);
free_out_channel:
    free(io_ctx->primary_ctx.io_channels.out_channel_list);
free_in_channel:
    free(io_ctx->primary_ctx.io_channels.in_channel_list);
    return -1;
}

int
jbpf_io_channel_list_destroy(struct jbpf_io_ctx* io_ctx)
{
    if (!io_ctx)
        return -1;

    if (io_ctx->io_type != JBPF_IO_IPC_PRIMARY && io_ctx->io_type != JBPF_IO_LOCAL_PRIMARY)
        return -1;

    ck_ht_destroy(&io_ctx->primary_ctx.io_channels.out_channel_list->out_ht);
    ck_ht_destroy(&io_ctx->primary_ctx.io_channels.in_channel_list->in_ht);

    free(io_ctx->primary_ctx.io_channels.out_channel_list);
    free(io_ctx->primary_ctx.io_channels.in_channel_list);

    return 0;
}

struct jbpf_io_channel*
jbpf_io_channel_exists(struct jbpf_io_channel_list* channel_list, struct jbpf_io_stream_id* stream_id, bool is_output)
{

    ck_ht_hash_t h;
    ck_ht_entry_t entry;
    struct jbpf_io_channel* io_channel = NULL;
    bool channel_exists;

    if (!channel_list || !stream_id) {
        return NULL;
    }

    if (is_output) {
        ck_epoch_begin(local_out_channel_list_epoch_record, NULL);

        ck_ht_hash(&h, &channel_list->out_channel_list->out_ht, stream_id->id, JBPF_IO_STREAM_ID_LEN);
        ck_ht_entry_key_set(&entry, stream_id->id, JBPF_IO_STREAM_ID_LEN);
        channel_exists = ck_ht_get_spmc(&channel_list->out_channel_list->out_ht, h, &entry);
        if (channel_exists) {
            io_channel = ck_ht_entry_value(&entry);
        }
        ck_epoch_end(local_out_channel_list_epoch_record, NULL);
    } else {
        ck_epoch_begin(local_in_channel_list_epoch_record, NULL);

        ck_ht_hash(&h, &channel_list->in_channel_list->in_ht, stream_id->id, JBPF_IO_STREAM_ID_LEN);
        ck_ht_entry_key_set(&entry, stream_id->id, JBPF_IO_STREAM_ID_LEN);
        channel_exists = ck_ht_get_spmc(&channel_list->in_channel_list->in_ht, h, &entry);

        if (channel_exists) {
            io_channel = ck_ht_entry_value(&entry);
        }

        ck_epoch_end(local_in_channel_list_epoch_record, NULL);
    }

    return io_channel;
}

bool
_jbpf_io_load_serde(struct jbpf_io_serde* serde, struct jbpf_io_stream_id* stream_id, char* lib_bin, size_t lib_len)
{

    if (lib_len > 0) {
        char sname[JBPF_IO_STREAM_ID_LEN * 3];
        _jbpf_io_tohex_str(stream_id->id, JBPF_IO_STREAM_ID_LEN, sname, JBPF_IO_STREAM_ID_LEN * 3);

        if (snprintf(serde->name, JBPF_IO_CHANNEL_NAME_LEN, "serde_memfd_%s", sname) < 0) {
            return false;
        }

        /* Load lib to memory using memfd */
        serde->lib_fd = _jbpf_io_open_mem_fd(serde->name);
        if (serde->lib_fd == -1) {
            jbpf_logger(JBPF_ERROR, "Unable to open memfd for serde lib %s\n", serde->name);
            return false;
        }

        if (_jbpf_io_write_lib(lib_bin, lib_len, serde->lib_fd) < 0) {
            jbpf_logger(JBPF_ERROR, "Unable to write serde lib %s to memory\n", serde->name);
            goto write_lib_error;
        }

        serde->handle = _jbpf_io_load_lib(serde->lib_fd, serde->name);
        if (!serde->handle) {
            jbpf_logger(JBPF_ERROR, "Unable to load serde lib %s\n", serde->name);
            goto write_lib_error;
        }

        serde->serialize = dlsym(serde->handle, JBPF_IO_SERIALIZER_NAME);
        if (!serde->serialize) {
            jbpf_logger(JBPF_DEBUG, "serde lib %s does contain serializer %s\n", serde->name, JBPF_IO_SERIALIZER_NAME);
        }

        serde->deserialize = dlsym(serde->handle, JBPF_IO_DESERIALIZER_NAME);
        if (!serde->deserialize) {
            jbpf_logger(
                JBPF_DEBUG, "serde lib %s does contain deserializer %s\n", serde->name, JBPF_IO_DESERIALIZER_NAME);
        }
    } else {
        serde->serialize = NULL;
        serde->deserialize = NULL;
    }

    return true;

serde_load_error:
    _jbpf_io_unload_lib(serde->handle);
write_lib_error:
    _jbpf_io_close_mem_fd(serde->lib_fd, serde->name);
    return false;
}

struct jbpf_io_channel*
_jbpf_io_create_channel(
    struct jbpf_io_channel_list* channel_list, struct jbpf_io_channel_request* chan_req, jbpf_mem_ctx_t* mem_ctx)
{

    int num_channels;
    bool is_output;
    ck_ht_hash_t h;
    ck_ht_entry_t entry;
    int elem_size;
    struct jbpf_io_channel* io_channel;

    char name_lib[JBPF_IO_STREAM_ID_LEN * 3];
    _jbpf_io_tohex_str(chan_req->stream_id.id, JBPF_IO_STREAM_ID_LEN, name_lib, JBPF_IO_STREAM_ID_LEN * 3);
    jbpf_logger(JBPF_INFO, "Creating channel %s\n", name_lib);

    if (chan_req->direction == JBPF_IO_CHANNEL_INPUT) {
        ck_epoch_begin(local_in_channel_list_epoch_record, NULL);
        num_channels = channel_list->in_channel_list->num_in_channels;
        is_output = false;
    } else if (chan_req->direction == JBPF_IO_CHANNEL_OUTPUT) {
        ck_epoch_begin(local_out_channel_list_epoch_record, NULL);
        num_channels = channel_list->out_channel_list->num_out_channels;
        is_output = true;
    } else {
        return NULL;
    }

    if (num_channels >= JBPF_IO_MAX_NUM_CHANNELS) {
        goto error_exit;
    }

    // Check if a channel with the given stream_id already exists and if yes, fail
    if (jbpf_io_channel_exists(channel_list, &chan_req->stream_id, is_output)) {
        jbpf_logger(JBPF_WARN, "Channel %s already exists\n", name_lib);
        goto error_exit;
    }

    io_channel = jbpf_calloc_ctx(mem_ctx, 1, sizeof(struct jbpf_io_channel));

    if (!io_channel) {
        goto error_exit;
    }

    io_channel->direction = chan_req->direction;
    io_channel->stream_id = chan_req->stream_id;
    io_channel->priority = LOW_IO_CHANNEL_PRIORITY;

    if ((is_output && (chan_req->type == JBPF_IO_CHANNEL_QUEUE || chan_req->type == JBPF_IO_CHANNEL_RINGBUF)) ||
        (!is_output && chan_req->type == JBPF_IO_CHANNEL_QUEUE)) {

        elem_size = chan_req->elem_size + sizeof(jbpf_io_channel_elem_t);
        io_channel->elem_size = chan_req->elem_size;
        io_channel->channel_ptr = jbpf_io_queue_create(chan_req->num_elems, elem_size, chan_req->direction, mem_ctx);

        io_channel->type = JBPF_IO_CHANNEL_QUEUE;
        if (!io_channel->channel_ptr) {
            goto free_io_channel;
        }
    } else {
        jbpf_logger(JBPF_WARN, "Invalid channel config option\n");
        goto free_io_channel;
    }

    if (!_jbpf_io_load_serde(
            &io_channel->primary_serde, &chan_req->stream_id, chan_req->descriptor, chan_req->descriptor_size)) {
        goto mem_error;
    }

    if (chan_req->direction == JBPF_IO_CHANNEL_INPUT) {
        ck_ht_hash(&h, &channel_list->in_channel_list->in_ht, io_channel->stream_id.id, JBPF_IO_STREAM_ID_LEN);
        ck_ht_entry_set(&entry, h, io_channel->stream_id.id, JBPF_IO_STREAM_ID_LEN, io_channel);
        ck_ht_put_spmc(&channel_list->in_channel_list->in_ht, h, &entry);

        for (int array_idx = 0; array_idx < JBPF_IO_MAX_NUM_CHANNELS; array_idx++) {
            if (!channel_list->in_channel_list->in_array[array_idx]) {
                __sync_lock_test_and_set(&channel_list->in_channel_list->in_array[array_idx], io_channel);
                channel_list->in_channel_list->num_in_channels++;
                break;
            }
        }

        ck_epoch_end(local_in_channel_list_epoch_record, NULL);
        ck_epoch_barrier(local_in_channel_list_epoch_record);
    } else {
        ck_ht_hash(&h, &channel_list->out_channel_list->out_ht, io_channel->stream_id.id, JBPF_IO_STREAM_ID_LEN);
        ck_ht_entry_set(&entry, h, io_channel->stream_id.id, JBPF_IO_STREAM_ID_LEN, io_channel);
        ck_ht_put_spmc(&channel_list->out_channel_list->out_ht, h, &entry);

        for (int array_idx = 0; array_idx < JBPF_IO_MAX_NUM_CHANNELS; array_idx++) {
            if (!channel_list->out_channel_list->out_array[array_idx]) {
                __sync_lock_test_and_set(&channel_list->out_channel_list->out_array[array_idx], io_channel);
                channel_list->out_channel_list->num_out_channels++;
                break;
            }
        }

        ck_epoch_end(local_out_channel_list_epoch_record, NULL);
        ck_epoch_barrier(local_out_channel_list_epoch_record);
    }

    return io_channel;

mem_error:
    jbpf_io_queue_free(io_channel->channel_ptr);
free_io_channel:
    jbpf_free(io_channel);
error_exit:
    if (chan_req->direction == JBPF_IO_CHANNEL_INPUT) {
        ck_epoch_end(local_in_channel_list_epoch_record, NULL);
    } else {
        ck_epoch_end(local_out_channel_list_epoch_record, NULL);
    }
    return NULL;
}

void
jbpf_io_destroy_out_channel(struct jbpf_io_channel_list* channel_list, struct jbpf_io_channel* io_channel)
{

    ck_ht_entry_t entry;
    ck_ht_hash_t h;

    if (!channel_list || !io_channel) {
        return;
    }

    ck_epoch_begin(local_out_channel_list_epoch_record, NULL);

    for (int array_idx = 0; array_idx < JBPF_IO_MAX_NUM_CHANNELS; array_idx++) {
        if (channel_list->out_channel_list->out_array[array_idx] == io_channel) {
            __sync_lock_test_and_set(&channel_list->out_channel_list->out_array[array_idx], NULL);
            channel_list->out_channel_list->num_out_channels--;
            break;
        }
    }

    ck_ht_hash(&h, &channel_list->out_channel_list->out_ht, io_channel->stream_id.id, JBPF_IO_STREAM_ID_LEN);
    ck_ht_entry_key_set(&entry, io_channel->stream_id.id, JBPF_IO_STREAM_ID_LEN);

    if (ck_ht_remove_spmc(&channel_list->out_channel_list->out_ht, h, &entry)) {
        jbpf_logger(JBPF_INFO, "Channel was removed from list successfully\n");
    } else {
        jbpf_logger(JBPF_ERROR, "Channel could not be found to be removed from list\n");
    }

    ck_epoch_end(local_out_channel_list_epoch_record, NULL);
    ck_epoch_call(local_out_channel_list_epoch_record, &io_channel->epoch_entry, io_channel_destructor);
    ck_epoch_barrier(local_out_channel_list_epoch_record);
    jbpf_logger(JBPF_INFO, "Barrier reached and channel %p was destroyed\n", io_channel);
}

void
jbpf_io_destroy_in_channel(struct jbpf_io_channel_list* channel_list, struct jbpf_io_channel* io_channel)
{

    ck_ht_entry_t entry;
    ck_ht_hash_t h;

    if (!channel_list || !io_channel) {
        return;
    }

    ck_epoch_begin(local_in_channel_list_epoch_record, NULL);

    for (int array_idx = 0; array_idx < JBPF_IO_MAX_NUM_CHANNELS; array_idx++) {
        if (channel_list->in_channel_list->in_array[array_idx] == io_channel) {
            __sync_lock_test_and_set(&channel_list->in_channel_list->in_array[array_idx], NULL);
            channel_list->in_channel_list->num_in_channels--;
            break;
        }
    }

    ck_ht_hash(&h, &channel_list->in_channel_list->in_ht, io_channel->stream_id.id, JBPF_IO_STREAM_ID_LEN);
    ck_ht_entry_key_set(&entry, io_channel->stream_id.id, JBPF_IO_STREAM_ID_LEN);

    if (ck_ht_remove_spmc(&channel_list->in_channel_list->in_ht, h, &entry)) {
        jbpf_logger(JBPF_INFO, "Channel was destroyed successfully\n");
    } else {
        jbpf_logger(JBPF_ERROR, "Channel could not be found to be deleted\n");
    }

    ck_epoch_end(local_in_channel_list_epoch_record, NULL);
    ck_epoch_call(local_in_channel_list_epoch_record, &io_channel->epoch_entry, io_channel_destructor);
    ck_epoch_barrier(local_in_channel_list_epoch_record);
}

void
jbpf_io_wait_on_channel_update(struct jbpf_io_channel_list* channel_list)
{
    ck_epoch_synchronize(local_in_channel_list_epoch_record);
    ck_epoch_synchronize(local_out_channel_list_epoch_record);
    jbpf_logger(JBPF_INFO, "Finished waiting on channel update\n");
}

void
jbpf_io_channel_handle_out_bufs(struct jbpf_io_ctx* io_ctx, handle_channel_bufs_cb_t handle_channel_bufs, void* ctx)
{

    struct jbpf_io_channel* io_channel_entry;
    struct jbpf_io_channel_list* channel_list;
    void* data_ptrs[JBPF_IO_BUFS_BATCH_SIZE] = {0};

    if (io_ctx->io_type == JBPF_IO_IPC_SECONDARY) {
        jbpf_logger(JBPF_WARN, "Warning: This can only be used by primary instances\n");
        return;
    }

    ck_epoch_begin(local_out_channel_list_epoch_record, NULL);

    channel_list = &io_ctx->primary_ctx.io_channels;

    for (int chan_idx = 0; chan_idx < JBPF_IO_MAX_NUM_CHANNELS; chan_idx++) {
        if (channel_list->out_channel_list->out_array[chan_idx] != NULL) {
            io_channel_entry = channel_list->out_channel_list->out_array[chan_idx];
            int recv = jbpf_io_channel_recv_data(io_channel_entry, data_ptrs, JBPF_IO_BUFS_BATCH_SIZE);

            if (recv > 0) {
                handle_channel_bufs(io_channel_entry, &io_channel_entry->stream_id, data_ptrs, recv, ctx);
            }
        }
    }

    ck_epoch_end(local_out_channel_list_epoch_record, NULL);
}

#ifdef JBPF_EXPERIMENTAL_FEATURES
int
jbpf_io_channel_pack_msg(struct jbpf_io_ctx* io_ctx, jbpf_channel_buf_ptr data, void* buf, size_t buf_len)
{
    struct jbpf_io_channel* io_channel;
    jbpf_io_channel_elem_t* elem;
    struct jbpf_io_serde* serde;
    int msg_size = 0, ser_buf_size;
    int res = 0;
    int bytes_written = 0;

    elem = container_of(data, jbpf_io_channel_elem_t, data);

    io_channel = elem->io_channel;

    if (io_ctx->io_type == JBPF_IO_IPC_PRIMARY || io_ctx->io_type == JBPF_IO_LOCAL_PRIMARY) {
        serde = &io_channel->primary_serde;
        if (io_channel->direction == JBPF_IO_CHANNEL_INPUT) {
            ck_epoch_begin(local_in_channel_list_epoch_record, NULL);
        } else {
            ck_epoch_begin(local_out_channel_list_epoch_record, NULL);
        }
    } else {
        serde = &io_channel->secondary_serde;
    }

    if (!serde->serialize || buf_len < JBPF_IO_STREAM_ID_LEN) {
        res = -1;
        jbpf_logger(
            JBPF_ERROR, "Error: Serializer not found or buffer length %zu is too small for stream ID\n", buf_len);
        goto out;
    }

    memcpy(buf, elem->io_channel->stream_id.id, JBPF_IO_STREAM_ID_LEN);
    msg_size += JBPF_IO_STREAM_ID_LEN;

    ser_buf_size = buf_len - msg_size;

    bytes_written = serde->serialize(data, io_channel->elem_size, (char*)buf + msg_size, ser_buf_size);

    if (bytes_written > 0) {
        msg_size += bytes_written;
        res = msg_size;
    } else {
        res = -2;
        jbpf_logger(JBPF_ERROR, "Error: Serialization failed, bytes written %d\n", bytes_written);
    }

out:
    if (io_ctx->io_type == JBPF_IO_IPC_PRIMARY || io_ctx->io_type == JBPF_IO_LOCAL_PRIMARY) {
        if (io_channel->direction == JBPF_IO_CHANNEL_INPUT) {
            ck_epoch_end(local_in_channel_list_epoch_record, NULL);
        } else {
            ck_epoch_end(local_out_channel_list_epoch_record, NULL);
        }
    }
    return res;
}

jbpf_channel_buf_ptr
jbpf_io_channel_unpack_msg(
    struct jbpf_io_ctx* io_ctx, void* in_data, size_t in_data_size, struct jbpf_io_stream_id* stream_id)
{

    int res = 0, decode_succ = 0;
    struct jbpf_io_channel* io_channel;
    struct jbpf_io_serde* serde;
    jbpf_channel_buf_ptr buf;

    memcpy(stream_id, in_data, JBPF_IO_STREAM_ID_LEN);

    // Check for both input and output channels
    io_channel = jbpf_io_find_channel(io_ctx, *stream_id, true);
    if (!io_channel) {
        io_channel = jbpf_io_find_channel(io_ctx, *stream_id, false);
    }

    if (!io_channel) {
        return NULL;
    }

    if (io_ctx->io_type == JBPF_IO_IPC_PRIMARY || io_ctx->io_type == JBPF_IO_LOCAL_PRIMARY) {
        serde = &io_channel->primary_serde;
        if (io_channel->direction == JBPF_IO_CHANNEL_INPUT) {
            ck_epoch_begin(local_in_channel_list_epoch_record, NULL);
        } else {
            ck_epoch_begin(local_out_channel_list_epoch_record, NULL);
        }
    } else {
        serde = &io_channel->secondary_serde;
    }

    if (!serde->deserialize) {
        res = -1;
        goto out;
    }

    int payload_size = in_data_size - JBPF_IO_STREAM_ID_LEN;

    buf = jbpf_io_channel_reserve_buf(io_channel);

    if (buf) {
        decode_succ = serde->deserialize(in_data + JBPF_IO_STREAM_ID_LEN, payload_size, buf, io_channel->elem_size);
        if (!decode_succ) {
            jbpf_io_channel_release_buf(buf);
            buf = NULL;
        }
    }

out:
    if (io_ctx->io_type == JBPF_IO_IPC_PRIMARY || io_ctx->io_type == JBPF_IO_LOCAL_PRIMARY) {
        if (io_channel->direction == JBPF_IO_CHANNEL_INPUT) {
            ck_epoch_end(local_in_channel_list_epoch_record, NULL);
        } else {
            ck_epoch_end(local_out_channel_list_epoch_record, NULL);
        }
    }
    return buf;
}
#else

int
jbpf_io_channel_pack_msg(struct jbpf_io_ctx* io_ctx, jbpf_channel_buf_ptr data, void* buf, size_t buf_len)
{
    return -1;
}

jbpf_channel_buf_ptr
jbpf_io_channel_unpack_msg(
    struct jbpf_io_ctx* io_ctx, void* in_data, size_t in_data_size, struct jbpf_io_stream_id* stream_id)
{
    return NULL;
}

#endif

int
jbpf_io_channel_send_data(struct jbpf_io_channel* channel, void* data, size_t size)
{

    int elem_size;
    if (!channel || !data)
        return -1;

    if (channel->type == JBPF_IO_CHANNEL_QUEUE) {
        elem_size = channel->elem_size;
        if (size > elem_size) {
            jbpf_logger(JBPF_ERROR, "Error. Data size %d is larger than the element size %d\n", size, elem_size);
            return -1;
        }

        jbpf_channel_buf_ptr data_buf = jbpf_io_channel_reserve_buf(channel);
        if (!data_buf)
            return -1;
        memcpy(data_buf, data, size);
        return jbpf_io_channel_submit_buf(channel);

    } else if (channel->type == JBPF_IO_CHANNEL_RINGBUF) {
        // TODO
        return -1;
    }

    return -1;
}

int
jbpf_io_channel_send_msg(struct jbpf_io_ctx* io_ctx, struct jbpf_io_stream_id* stream_id, void* data, size_t size)
{
    int res = 0;
    struct jbpf_io_channel* channel;
    ck_ht_entry_t entry;
    ck_ht_hash_t h;

    if (io_ctx->io_type == JBPF_IO_IPC_SECONDARY) {
        jbpf_logger(JBPF_WARN, "Warning: This can only be used by primary instances\n");
        return -1;
    }

    ck_epoch_begin(local_in_channel_list_epoch_record, NULL);

    ck_ht_hash(&h, &io_ctx->primary_ctx.io_channels.in_channel_list->in_ht, stream_id, JBPF_IO_STREAM_ID_LEN);
    ck_ht_entry_key_set(&entry, stream_id, JBPF_IO_STREAM_ID_LEN);

    if (ck_ht_get_spmc(&io_ctx->primary_ctx.io_channels.in_channel_list->in_ht, h, &entry)) {
        channel = ck_ht_entry_value(&entry);
        res = jbpf_io_channel_send_data(channel, data, size);
        goto out;
    } else {
        res = -1;
        goto out;
    }

out:
    ck_epoch_end(local_in_channel_list_epoch_record, NULL);
    return res;
}

jbpf_channel_buf_ptr
jbpf_io_channel_reserve_buf(struct jbpf_io_channel* channel)
{
    jbpf_io_channel_elem_t* elem;
    if (!channel || channel->type == JBPF_IO_CHANNEL_RINGBUF)
        return NULL;

    if (channel->type == JBPF_IO_CHANNEL_QUEUE) {
        elem = jbpf_io_queue_reserve(channel->channel_ptr);
        if (!elem) {
            return NULL;
        } else {
            elem->io_channel = channel;
            return elem->data;
        }
    }

    return NULL;
}

int
jbpf_io_channel_submit_buf(struct jbpf_io_channel* channel)
{
    if (!channel || channel->type == JBPF_IO_CHANNEL_RINGBUF)
        return -1;

    if (channel->type == JBPF_IO_CHANNEL_QUEUE) {
        return jbpf_io_queue_enqueue(channel->channel_ptr);
    }

    return -1;
}

void
jbpf_io_channel_release_all_buf(struct jbpf_io_channel* channel)
{
    jbpf_io_queue_release_all(channel->channel_ptr);
}

void
jbpf_io_channel_release_buf(jbpf_channel_buf_ptr buf_ptr)
{
    jbpf_io_channel_elem_t* elem;
    elem = container_of(buf_ptr, jbpf_io_channel_elem_t, data);

    jbpf_io_queue_release(elem, false);
}

int
jbpf_io_channel_recv_data(struct jbpf_io_channel* channel, jbpf_channel_buf_ptr* data_ptrs, int batch_size)
{

    jbpf_io_channel_elem_t* elem;
    int num_collected = 0;

    if (!channel || !data_ptrs)
        return -1;

    if (channel->type == JBPF_IO_CHANNEL_QUEUE) {
        for (num_collected = 0; num_collected < batch_size; num_collected++) {
            elem = jbpf_io_queue_dequeue(channel->channel_ptr);
            data_ptrs[num_collected] = elem ? elem->data : NULL;
            if (!data_ptrs[num_collected]) {
                return num_collected;
            }
        }

    } else if (channel->type == JBPF_IO_CHANNEL_RINGBUF) {
        // TODO
        return -1;
    }

    return num_collected;
}

int
jbpf_io_channel_recv_data_copy(struct jbpf_io_channel* channel, void* buf, size_t buf_size)
{

    void* data;
    jbpf_io_channel_elem_t* elem;
    int data_size = 0;

    if (!channel || !buf || (channel->type != JBPF_IO_CHANNEL_RINGBUF && channel->type != JBPF_IO_CHANNEL_QUEUE))
        return -1;

    data_size = channel->elem_size;
    elem = jbpf_io_queue_dequeue(channel->channel_ptr);

    if (!elem) {
        return -1;
    }

    data = elem->data;

    if (!data || data_size > buf_size) {
        return -1;
    }

    memcpy(buf, data, data_size);
    return 1;
}

jbpf_channel_buf_ptr
jbpf_io_channel_share_data_ptr(jbpf_channel_buf_ptr data_ptr)
{
    jbpf_io_channel_elem_t* elem;

    if (!data_ptr) {
        return NULL;
    }

    elem = container_of(data_ptr, jbpf_io_channel_elem_t, data);
    jbpf_mbuf_share_data_ptr(elem);

    return elem->data;
}
