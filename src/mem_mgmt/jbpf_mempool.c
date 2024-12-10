// Copyright (c) Microsoft Corporation. All rights reserved.
#include <string.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#include "jbpf_mempool.h"
#include "jbpf_mempool_int.h"
#include "jbpf_mem_mgmt_utils.h"
#include "jbpf_logging.h"

#include "ck_stack.h"

#ifdef JBPF_IO_POISON
#pragma message "Poisoning mempool pointers"
#include <sanitizer/asan_interface.h>
#endif

jbpf_mempool_t*
jbpf_init_mempool_ctx(jbpf_mem_ctx_t* mem_ctx, uint32_t n_elems, size_t elem_size, int mempool_type)
{

    struct jbpf_mempool* mempool_ctx = NULL;

    // One additional element is needed by the ringbuffer
    int num_elems = round_up_pow_of_two(n_elems + 1);
    num_elems -= 1;

    int mbuf_size = _jbpf_round_up_mem(elem_size + sizeof(jbpf_mbuf_t), 16);

    if (!mem_ctx)
        mempool_ctx = jbpf_calloc(1, sizeof(struct jbpf_mempool));
    else
        mempool_ctx = jbpf_calloc_ctx(mem_ctx, 1, sizeof(struct jbpf_mempool));

    if (!mempool_ctx)
        return NULL;

    mempool_ctx->type = mempool_type;

    mempool_ctx->num_elems = num_elems;
    mempool_ctx->mbuf_size = mbuf_size;
    mempool_ctx->elem_size = elem_size;

    // We create one extra element, that is only used when we want to destroy the stack
    if (!mem_ctx) {
        mempool_ctx->data_array = jbpf_calloc(num_elems, mbuf_size);
    } else {
        mempool_ctx->data_array = jbpf_calloc_ctx(mem_ctx, num_elems, mbuf_size);
    }

    if (!mempool_ctx->data_array) {
        jbpf_free(mempool_ctx);
        return NULL;
    }

    // We create one extra element, that is only used when we want to destroy the stack
    if (!mem_ctx) {
        mempool_ctx->ring.buf = jbpf_calloc(num_elems + 1, sizeof(ck_ring_buffer_t));
    } else {
        mempool_ctx->ring.buf = jbpf_calloc_ctx(mem_ctx, num_elems + 1, sizeof(ck_ring_buffer_t));
    }

    if (!mempool_ctx->ring.buf) {
        jbpf_free(mempool_ctx->data_array);
        jbpf_free(mempool_ctx);
        return NULL;
    }

    // We create one extra element, that is only used when we want to destroy the stack
    if (!mem_ctx) {
        mempool_ctx->ring_destroy.buf = jbpf_calloc(num_elems + 1, sizeof(ck_ring_buffer_t));
    } else {
        mempool_ctx->ring_destroy.buf = jbpf_calloc_ctx(mem_ctx, num_elems + 1, sizeof(ck_ring_buffer_t));
    }

    if (!mempool_ctx->ring_destroy.buf) {
        jbpf_free(mempool_ctx->ring.buf);
        jbpf_free(mempool_ctx->data_array);
        jbpf_free(mempool_ctx);
        return NULL;
    }

    ck_ring_init(&mempool_ctx->ring.ring, mempool_ctx->num_elems + 1);
    ck_ring_init(&mempool_ctx->ring_destroy.ring, mempool_ctx->num_elems + 1);

    // Initially we allocate and free from the same ring
    mempool_ctx->ring_alloc = &mempool_ctx->ring;
    mempool_ctx->ring_free = &mempool_ctx->ring;

    // We create one extra element, that is only used when we want to destroy the pool
    if (!mem_ctx) {
        mempool_ctx->ring_marker = jbpf_calloc(1, mbuf_size);
    } else {
        mempool_ctx->ring_marker = jbpf_calloc_ctx(mem_ctx, 1, mbuf_size);
    }

    if (!mempool_ctx->ring_marker) {
        jbpf_free(mempool_ctx->ring.buf);
        jbpf_free(mempool_ctx->ring_destroy.buf);
        jbpf_free(mempool_ctx->data_array);
        jbpf_free(mempool_ctx);
        return NULL;
    }

    jbpf_mbuf_t* entry = mempool_ctx->data_array;

    for (int i = 0; i < num_elems; i++) {
        if (!ck_ring_enqueue_mpmc(&mempool_ctx->ring_alloc->ring, mempool_ctx->ring_alloc->buf, entry)) {
            jbpf_logger(JBPF_ERROR, "Could not add element to the ringbuf\n");
            return NULL;
        }
        entry = (jbpf_mbuf_t*)((uint8_t*)entry + mbuf_size);
    }
    jbpf_logger(JBPF_INFO, "Added %d elements to the ringbuf\n", num_elems);

#ifdef JBPF_IO_POISON
    ASAN_POISON_MEMORY_REGION(mempool_ctx->data_array, num_elems * mbuf_size);
#endif

    jbpf_logger(
        JBPF_INFO,
        "The size of the mempool is %d and the capacity is %d\n",
        jbpf_get_mempool_size(mempool_ctx),
        jbpf_get_mempool_capacity(mempool_ctx));

    return mempool_ctx;
}

jbpf_mempool_t*
jbpf_init_mempool(uint32_t n_elems, size_t elem_size, int mempool_type)
{
    return jbpf_init_mempool_ctx(NULL, n_elems, elem_size, mempool_type);
}

void
jbpf_destroy_mempool(jbpf_mempool_t* mempool)
{

    if (!mempool)
        return;

    // Do not allow any thread to allocate memory from this point on.
    // Just wait for all the memory to return to the original ring, so that
    // we can free it.
    atomic_exchange(&mempool->ring_alloc, &mempool->ring_destroy);

    // If the ring was full, it is time to destroy it. Else, some other thread will do it
    if (!ck_ring_enqueue_mpmc(&mempool->ring_free->ring, mempool->ring_free->buf, mempool->ring_marker)) {
        jbpf_logger(JBPF_INFO, "mempool destroyed from jbpf_destroy_mempool()\n");
        jbpf_free(mempool->ring_marker);
        jbpf_free(mempool->ring.buf);
        jbpf_free(mempool->ring_destroy.buf);
#ifdef JBPF_IO_POISON
        ASAN_UNPOISON_MEMORY_REGION(mempool->data_array, mempool->num_elems * mempool->mbuf_size);
#endif
        jbpf_free(mempool->data_array);
        jbpf_free(mempool);
    } else {
        jbpf_logger(JBPF_INFO, "Mempool not ready to be destroyed during jbpf_destroy_mempool()\n");
    }
}

jbpf_mbuf_t*
jbpf_mbuf_alloc(jbpf_mempool_t* mempool)
{

    jbpf_mbuf_t* mb;

    if (!mempool) {
        return NULL;
    }

    if (!ck_ring_dequeue_mpmc(&mempool->ring_alloc->ring, mempool->ring_alloc->buf, &mb)) {
        return NULL;
    }

#ifdef JBPF_IO_POISON
    ASAN_UNPOISON_MEMORY_REGION(mb, mempool->mbuf_size);
#endif

    mb->mempool = mempool;
    mb->ref_cnt = 1;
    return mb;
}

void
jbpf_mbuf_free_from_data_ptr(void* data_ptr, bool reset)
{

    if (!data_ptr)
        return;

    struct jbpf_mbuf* mb = (struct jbpf_mbuf*)((uint8_t*)data_ptr - offsetof(struct jbpf_mbuf, data));
    jbpf_mbuf_free(mb, reset);
}

void
jbpf_mbuf_free(jbpf_mbuf_t* mbuf, bool reset)
{

    jbpf_mempool_t* mempool;
    size_t ref_cnt;

    if (!mbuf) {
        return;
    }

    ref_cnt = __atomic_sub_fetch(&mbuf->ref_cnt, 1, __ATOMIC_SEQ_CST);

    if (ref_cnt > 0) {
        return;
    }

    mempool = mbuf->mempool;

    if (reset) {
        memset(mbuf, 0, mempool->mbuf_size);
    }

    // The ring_marker has been added to the ring and the ring is full.
    // This is an indication that we need to destroy the ring
    if (!ck_ring_enqueue_mpmc(&mempool->ring_free->ring, mempool->ring_free->buf, mbuf)) {
        jbpf_logger(JBPF_INFO, "mempool destroyed from jbpf_mbuf_free()\n");
        jbpf_free(mempool->ring_marker);
        jbpf_free(mempool->ring.buf);
        jbpf_free(mempool->ring_destroy.buf);
#ifdef JBPF_IO_POISON
        ASAN_UNPOISON_MEMORY_REGION(mempool->data_array, mempool->num_elems * mempool->mbuf_size);
#endif
        jbpf_free(mempool->data_array);
        jbpf_free(mempool);
    }
}

jbpf_mbuf_t*
jbpf_mbuf_share(jbpf_mbuf_t* mbuf)
{

    if (!mbuf)
        return NULL;

    atomic_fetch_add(&mbuf->ref_cnt, 1);
    return mbuf;
}

void*
jbpf_mbuf_share_data_ptr(void* data_ptr)
{

    if (!data_ptr)
        return NULL;

    struct jbpf_mbuf* mb = container_of(data_ptr, struct jbpf_mbuf, data);
    atomic_fetch_add(&mb->ref_cnt, 1);
    return data_ptr;
}

int
jbpf_get_mempool_capacity(jbpf_mempool_t* mempool)
{
    if (!mempool)
        return -1;

    return mempool->num_elems;
}

int
jbpf_get_mempool_size(jbpf_mempool_t* mempool)
{
    if (!mempool)
        return -1;

    return ck_ring_size(&mempool->ring_alloc->ring);
}