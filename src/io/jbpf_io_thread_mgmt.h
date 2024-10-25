#ifndef JBPF_IO_THREAD_MGMT_H
#define JBPF_IO_THREAD_MGMT_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#ifndef JBPF_IO_MAX_NUM_THREADS
#define JBPF_IO_MAX_NUM_THREADS 32
#endif

void
jbpf_io_thread_ctx_init(void);

int16_t
jbpf_io_get_thread_id(void);

#endif
