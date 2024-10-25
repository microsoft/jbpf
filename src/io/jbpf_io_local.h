#ifndef JBPF_IO_LOCAL_H
#define JBPF_IO_LOCAL_H

#include "jbpf_io_defs.h"

int
jbpf_io_local_init(struct jbpf_io_local_cfg* local_cfg, struct jbpf_io_ctx* io_ctx);

void
jbpf_io_local_destroy(struct jbpf_io_ctx* io_ctx);

#endif