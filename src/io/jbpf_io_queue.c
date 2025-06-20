// Copyright (c) Microsoft Corporation. All rights reserved.
#include <stdio.h>

#include "ck_ring.h"

#include "jbpf_mempool.h"

#include "jbpf_io_queue.h"
#include "jbpf_io_queue_int.h"
#include "jbpf_io_thread_mgmt.h"
#include "jbpf_io_channel.h"
#include "jbpf_logging.h"

void*
jbpf_io_queue_create(int max_entries, int elem_size, int type, jbpf_mem_ctx_t* mem_ctx)
{

    struct jbpf_io_queue_ctx* ioq_ctx = NULL;
    int mem_type, num_elems;

    ioq_ctx = jbpf_calloc_ctx(mem_ctx, 1, sizeof(struct jbpf_io_queue_ctx));

    if (!ioq_ctx) {
        jbpf_logger(JBPF_ERROR, "Error allocating memory for IO queue context\n");
        goto out;
    }

    for (int i = 0; i < JBPF_IO_MAX_NUM_THREADS; i++) {
        ioq_ctx->alloc_ptr[i] = NULL;
    }

    if (type == JBPF_IO_CHANNEL_OUTPUT) {
        mem_type = JBPF_MEMPOOL_SP;
    } else {
        mem_type = JBPF_MEMPOOL_MP;
    }

    // Allocate memory for the requested elements + at least one for each thread
    num_elems = max_entries + JBPF_IO_MAX_NUM_THREADS;

    ioq_ctx->mempool = jbpf_init_mempool_ctx(mem_ctx, num_elems, elem_size, mem_type);

    if (!ioq_ctx->mempool) {
        jbpf_free(ioq_ctx);
        return NULL;
    }

    num_elems = jbpf_get_mempool_capacity(ioq_ctx->mempool);

    ioq_ctx->num_elems = num_elems;

    // Ring size must be power of 2
    // and needs to have one element more than those we are storing
    ioq_ctx->ring_size = round_up_pow_of_two(num_elems + 1);
    ioq_ctx->elem_size = elem_size;
    ioq_ctx->type = type;

    ck_ring_init(&ioq_ctx->ring, ioq_ctx->ring_size);

    if (!ck_ring_valid(&ioq_ctx->ring)) {
        jbpf_logger(JBPF_ERROR, "Ringbuffer is invalid\n");
        return NULL;
    }

    ioq_ctx->ringbuffer = jbpf_calloc_ctx(mem_ctx, 1, ioq_ctx->ring_size * sizeof(ck_ring_buffer_t));

    if (!ioq_ctx->ringbuffer) {
        jbpf_free(ioq_ctx);
        return NULL;
    }

out:
    return ioq_ctx;
}

void
jbpf_io_queue_free(jbpf_io_queue_ctx_t* ioq_ctx)
{
    if (!ioq_ctx) {
        jbpf_logger(JBPF_ERROR, "Invalid IO queue context for freeing\n");
        return;
    }

    void* data_ptr;

    while ((data_ptr = jbpf_io_queue_dequeue(ioq_ctx))) {
        jbpf_mbuf_free_from_data_ptr(data_ptr, false);
    }

    jbpf_destroy_mempool(ioq_ctx->mempool);
    jbpf_free(ioq_ctx->ringbuffer);
    jbpf_free(ioq_ctx);
}

void
jbpf_io_queue_release_all(jbpf_io_queue_ctx_t* ioq_ctx)
{
    int thread_id;
    jbpf_mbuf_t* mb;

    if (!ioq_ctx) {
        jbpf_logger(JBPF_ERROR, "Invalid IO queue context for releasing all buffers\n");
        return;
    }

    for (thread_id = 0; thread_id < JBPF_IO_MAX_NUM_THREADS; thread_id++) {
        if (ioq_ctx->alloc_ptr[thread_id]) {
            mb = ioq_ctx->alloc_ptr[thread_id];
            jbpf_mbuf_free(mb, false);
            ioq_ctx->alloc_ptr[thread_id] = NULL;
        }
    }
}

void*
jbpf_io_queue_reserve(jbpf_io_queue_ctx_t* ioq_ctx)
{
    int thread_id;
    jbpf_mbuf_t* mb;

    if (!ioq_ctx) {
        jbpf_logger(JBPF_ERROR, "Invalid IO queue context for reservation\n");
        return NULL;
    }

    thread_id = jbpf_io_get_thread_id();

    if (thread_id < 0) {
        jbpf_logger(JBPF_ERROR, "Invalid thread ID for IO queue reservation\n");
        return NULL;
    }

    if (ioq_ctx->alloc_ptr[thread_id]) {
        mb = ioq_ctx->alloc_ptr[thread_id];
        return mb->data;
    }

    mb = jbpf_mbuf_alloc(ioq_ctx->mempool);

    if (!mb) {
        jbpf_logger(JBPF_ERROR, "Error allocating memory for IO queue buffer\n");
        return NULL;
    }

    ioq_ctx->alloc_ptr[thread_id] = mb;

    return mb->data;
}

void
jbpf_io_queue_release(void* iobuf, bool clean)
{
    if (!iobuf) {
        jbpf_logger(JBPF_ERROR, "Invalid IO buffer for release\n");
        return;
    }

    jbpf_mbuf_free_from_data_ptr(iobuf, clean);
}

int
jbpf_io_queue_enqueue(jbpf_io_queue_ctx_t* ioq_ctx)
{
    int thread_id = jbpf_io_get_thread_id();

    if (thread_id < 0 || !ioq_ctx || !ioq_ctx->alloc_ptr[thread_id]) {
        jbpf_logger(JBPF_ERROR, "Invalid thread ID or IO queue context for enqueue\n");
        return -2;
    }

    jbpf_mbuf_t* mb = ioq_ctx->alloc_ptr[thread_id];

    if (ioq_ctx->type == JBPF_IO_CHANNEL_OUTPUT) {
        if (!ck_ring_enqueue_mpsc(&ioq_ctx->ring, ioq_ctx->ringbuffer, mb->data)) {
            jbpf_logger(JBPF_ERROR, "Error enqueuing data to the IO queue type %d\n", ioq_ctx->type);
            return -1;
        }
    } else {
        if (!ck_ring_enqueue_mpmc(&ioq_ctx->ring, ioq_ctx->ringbuffer, mb->data)) {
            jbpf_logger(JBPF_ERROR, "Error enqueuing data to the IO queue type %d\n", ioq_ctx->type);
            return -1;
        }
    }

    ioq_ctx->alloc_ptr[thread_id] = NULL;
    return 0;
}

void*
jbpf_io_queue_dequeue(jbpf_io_queue_ctx_t* ioq_ctx)
{
    void* data_ptr;

    if (!ioq_ctx)
        return NULL;

    if (ioq_ctx->type == JBPF_IO_CHANNEL_OUTPUT) {
        if (!ck_ring_dequeue_mpsc(&ioq_ctx->ring, ioq_ctx->ringbuffer, &data_ptr)) {
            jbpf_logger(JBPF_WARN, "Error dequeuing data from the IO queue: JBPF_IO_CHANNEL_OUTPUT\n");
            return NULL;
        }
    } else {
        if (!ck_ring_dequeue_mpmc(&ioq_ctx->ring, ioq_ctx->ringbuffer, &data_ptr)) {
            jbpf_logger(JBPF_WARN, "Error dequeuing data from the IO queue type %d\n", ioq_ctx->type);
            return NULL;
        }
    }

    return data_ptr;
}

int
jbpf_io_queue_get_elem_size(jbpf_io_queue_ctx_t* ioq_ctx)
{
    if (!ioq_ctx) {
        jbpf_logger(JBPF_ERROR, "Invalid IO queue context for getting element size\n");
        return -1;
    }

    return ioq_ctx->elem_size;
}

int
jbpf_io_queue_get_num_elems(jbpf_io_queue_ctx_t* ioq_ctx)
{
    if (!ioq_ctx) {
        jbpf_logger(JBPF_ERROR, "Invalid IO queue context for getting number of elements\n");
        return -1;
    }

    return ioq_ctx->num_elems;
}
