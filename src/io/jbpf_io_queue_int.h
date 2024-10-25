#ifndef JBPF_IO_QUEUE_INT_H
#define JBPF_IO_QUEUE_INT_H

#include "ck_ring.h"
#include "jbpf_mempool.h"
#include "jbpf_io_queue.h"
#include "jbpf_io_thread_mgmt.h"
#include "jbpf_io_channel.h"
#include "jbpf_io_utils.h"

struct jbpf_io_queue_ctx
{
    void* alloc_ptr[JBPF_IO_MAX_NUM_THREADS];
    jbpf_mempool_t* mempool;
    ck_ring_buffer_t* ringbuffer;
    ck_ring_t ring CK_CC_CACHELINE;
    uint32_t type;
    uint32_t elem_size;
    uint32_t num_elems;
    uint32_t ring_size;
};

#endif // JBPF_IO_QUEUE_INT_H