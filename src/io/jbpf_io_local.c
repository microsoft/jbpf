// Copyright (c) Microsoft Corporation. All rights reserved.
#include <stdio.h>

#include "jbpf_io_local.h"
#include "jbpf_mem_mgmt.h"
#include "jbpf_io_int.h"
#include "jbpf_io_queue.h"
#include "jbpf_io_channel.h"
#include "jbpf_logging.h"

int
jbpf_io_local_init(struct jbpf_io_local_cfg* local_cfg, struct jbpf_io_ctx* io_ctx)
{

    int res = 0;

    if (!local_cfg || !io_ctx) {
        jbpf_logger(JBPF_ERROR, "Invalid parameters for local IO initialization\n");
        return -1;
    }

    jbpf_logger(JBPF_INFO, "Allocating memory for IO\n");
    // For now we use mi-malloc for the allocation in this case. Might revisit in the future
    res = jbpf_init_memory(local_cfg->mem_cfg.memory_size, true);
    io_ctx->primary_ctx.local_ctx.mem_ctx = NULL;

    return res;
}

void
jbpf_io_local_destroy(struct jbpf_io_ctx* io_ctx)
{

    if (!io_ctx) {
        jbpf_logger(JBPF_ERROR, "Invalid io_ctx for local IO destruction\n");
        return;
    }

    jbpf_logger(JBPF_INFO, "Destroying local IO interface\n");
    jbpf_destroy_memory();
}
