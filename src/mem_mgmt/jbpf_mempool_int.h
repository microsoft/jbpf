#ifndef JBPF_MEMPOOL_INT_H
#define JBPF_MEMPOOL_INT_H

#include "ck_ring.h"

typedef struct jbpf_ring_ctx
{
    ck_ring_t ring CK_CC_CACHELINE;
    ck_ring_buffer_t* buf;
} jbpf_ring_ctx_t;

struct jbpf_mempool
{
    void* data_array;
    jbpf_ring_ctx_t ring;
    jbpf_ring_ctx_t ring_destroy;
    jbpf_ring_ctx_t* ring_alloc;
    jbpf_ring_ctx_t* ring_free;
    jbpf_mbuf_t* ring_marker;
    uint32_t type;
    uint16_t num_elems;
    uint16_t elem_size;
    uint16_t mbuf_size;
};

#endif