#ifndef JBPF_IO_QUEUE_H
#define JBPF_IO_QUEUE_H

#include <stdbool.h>

typedef struct jbpf_io_queue_ctx jbpf_io_queue_ctx_t;

void*
jbpf_io_queue_create(int max_entries, int elem_size, int type, jbpf_mem_ctx_t* mem_ctx);

/* This assumes that no-one is using the queue any longer.
    However, it is still possible to have elements in the queue*/
void
jbpf_io_queue_free(jbpf_io_queue_ctx_t* ioq_ctx);

void*
jbpf_io_queue_reserve(jbpf_io_queue_ctx_t* ioq_ctx);

void
jbpf_io_queue_release_all(jbpf_io_queue_ctx_t* ioq_ctx);

void
jbpf_io_queue_release(void* iobuf, bool clean);

int
jbpf_io_queue_enqueue(jbpf_io_queue_ctx_t* ioq_ctx);

void*
jbpf_io_queue_dequeue(jbpf_io_queue_ctx_t* ioq_ctx);

int
jbpf_io_queue_get_elem_size(jbpf_io_queue_ctx_t* ioq_ctx);

int
jbpf_io_queue_get_num_elems(jbpf_io_queue_ctx_t* ioq_ctx);

#endif