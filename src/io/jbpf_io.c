// Copyright (c) Microsoft Corporation. All rights reserved.
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>

#include "jbpf_io.h"
#include "jbpf_io_ipc.h"
#include "jbpf_io_int.h"
#include "jbpf_io_local.h"
#include "jbpf_io_channel.h"
#include "jbpf_io_channel_int.h"
#include "jbpf_io_thread_mgmt.h"
#include "jbpf_logging.h"
#include "jbpf_common.h"

struct jbpf_io_ctx dio_ctx;

static int
_jbpf_io_create_run_dir(struct jbpf_io_ctx* io_ctx)
{
    struct stat st;
    if (stat(io_ctx->jbpf_io_path, &st) == -1) {

        if (mkdir(io_ctx->jbpf_io_path, S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
            jbpf_logger(JBPF_ERROR, "Error creating directory %s\n", io_ctx->jbpf_io_path);
            return -1;
        }
        jbpf_logger(JBPF_INFO, "Directory '%s' created.\n", io_ctx->jbpf_io_path);
    } else {
        jbpf_logger(JBPF_INFO, "Directory '%s' already exists.\n", io_ctx->jbpf_io_path);
    }

    return 0;
}

jbpf_io_ctx_t*
jbpf_io_init(struct jbpf_io_config* io_config)
{
    int res;

    if (!io_config)
        return NULL;

    if (io_config->has_jbpf_path_namespace) {
        snprintf(
            dio_ctx.jbpf_io_path, JBPF_PATH_NAMESPACE_LEN, "%s/%s", io_config->jbpf_path, io_config->jbpf_namespace);

        strncpy(dio_ctx.jbpf_mem_base_name, io_config->jbpf_namespace, JBPF_IO_IPC_MAX_NAMELEN - 1);
        snprintf(
            dio_ctx.jbpf_mem_base_name, sizeof(dio_ctx.jbpf_mem_base_name) - 1, "%s_io", io_config->jbpf_namespace);
        dio_ctx.jbpf_mem_base_name[sizeof(dio_ctx.jbpf_mem_base_name) - 1] = '\0';
    } else {
        snprintf(dio_ctx.jbpf_io_path, JBPF_PATH_NAMESPACE_LEN, "%s/%s", JBPF_DEFAULT_RUN_PATH, JBPF_DEFAULT_NAMESPACE);

        strncpy(dio_ctx.jbpf_mem_base_name, JBPF_IO_IPC_BASE_NAME, JBPF_IO_IPC_MAX_NAMELEN - 1);
        dio_ctx.jbpf_mem_base_name[JBPF_IO_IPC_MAX_NAMELEN - 1] = '\0';
    }

    if (_jbpf_io_create_run_dir(&dio_ctx) != 0)
        return NULL;

    jbpf_io_thread_ctx_init();

    if (io_config->type == JBPF_IO_IPC_PRIMARY) {
        jbpf_io_channel_list_init(&dio_ctx);
        res = jbpf_io_ipc_init(&io_config->ipc_config, &dio_ctx);

        if (res != 0) {
            jbpf_logger(JBPF_ERROR, "Error initializing IPC in jbpf_io_ipc_init\n");
            return NULL;
        }

    } else if (io_config->type == JBPF_IO_IPC_SECONDARY) {
        res = jbpf_io_ipc_register(&io_config->ipc_config, &dio_ctx);
        if (res != 0) {
            jbpf_logger(JBPF_ERROR, "Error registering IPC in jbpf_io_ipc_register\n");
            return NULL;
        }
    } else if (io_config->type == JBPF_IO_LOCAL_PRIMARY) {
        jbpf_io_channel_list_init(&dio_ctx);
        res = jbpf_io_local_init(&io_config->local_config, &dio_ctx);

        if (res != 0) {
            jbpf_logger(JBPF_ERROR, "Error initializing local IO in jbpf_io_local_init\n");
            return NULL;
        }

    } else {
        jbpf_logger(JBPF_ERROR, "Error. Invalid IO type\n");
        return NULL;
    }

    // TODO
    return &dio_ctx;
}

void
jbpf_io_stop()
{
    if (dio_ctx.io_type == JBPF_IO_IPC_PRIMARY) {
        jbpf_io_ipc_stop(&dio_ctx);
    } else if (dio_ctx.io_type == JBPF_IO_IPC_SECONDARY) {
        jbpf_io_ipc_deregister(&dio_ctx);
    } else if (dio_ctx.io_type == JBPF_IO_LOCAL_PRIMARY) {
        jbpf_io_local_destroy(&dio_ctx);
    }
}

struct jbpf_io_ctx*
jbpf_io_get_ctx()
{
    return &dio_ctx;
}

struct jbpf_io_channel*
jbpf_io_create_channel(
    jbpf_io_ctx_t* io_ctx,
    jbpf_io_channel_direction channel_direction,
    jbpf_io_channel_type channel_type,
    int num_elems,
    int elem_size,
    struct jbpf_io_stream_id stream_id,
    char* descriptor,
    size_t descriptor_size)
{

    if (!io_ctx) {
        return NULL;
    }

    if (io_ctx->io_type == JBPF_IO_IPC_PRIMARY) {
        return jbpf_io_ipc_local_req_create_channel(
            io_ctx, channel_direction, channel_type, num_elems, elem_size, stream_id, descriptor, descriptor_size);

    } else if (io_ctx->io_type == JBPF_IO_IPC_SECONDARY) {
        return jbpf_io_ipc_req_create_channel(
            io_ctx, channel_direction, channel_type, num_elems, elem_size, stream_id, descriptor, descriptor_size);

    } else if (io_ctx->io_type == JBPF_IO_LOCAL_PRIMARY) {

        struct jbpf_io_channel_request chan_req;
        if (descriptor_size > JBPF_IO_MAX_DESCRIPTOR_SIZE) {
            jbpf_logger(JBPF_ERROR, "Descriptor size was greater than expected\n");
            return NULL;
        }
        memcpy(chan_req.descriptor, descriptor, descriptor_size);
        chan_req.descriptor_size = descriptor_size;
        chan_req.direction = channel_direction;
        chan_req.type = channel_type;
        chan_req.num_elems = num_elems;
        chan_req.elem_size = elem_size;
        chan_req.stream_id = stream_id;

        return _jbpf_io_create_channel(
            &io_ctx->primary_ctx.io_channels, &chan_req, io_ctx->primary_ctx.local_ctx.mem_ctx);

        return NULL;
    } else {
        return NULL;
    }

    return NULL;
}

void
jbpf_io_destroy_channel(jbpf_io_ctx_t* io_ctx, struct jbpf_io_channel* io_channel)
{

    if (!io_ctx || !io_channel) {
        return;
    }

    jbpf_io_channel_release_all_buf(io_channel);

    if (io_ctx->io_type == JBPF_IO_IPC_PRIMARY) {
        jbpf_io_ipc_local_req_destroy_channel(io_ctx, io_channel);
    } else if (io_ctx->io_type == JBPF_IO_IPC_SECONDARY) {
        jbpf_io_ipc_req_destroy_channel(io_ctx, io_channel);
    } else if (io_ctx->io_type == JBPF_IO_LOCAL_PRIMARY) {
        if (io_channel->direction == JBPF_IO_CHANNEL_OUTPUT) {
            jbpf_io_destroy_out_channel(&io_ctx->primary_ctx.io_channels, io_channel);
        } else if (io_channel->direction == JBPF_IO_CHANNEL_INPUT) {
            jbpf_io_destroy_in_channel(&io_ctx->primary_ctx.io_channels, io_channel);
        } else {
            jbpf_logger(JBPF_ERROR, "Error. Invalid channel direction\n");
        }
    }
}

struct jbpf_io_channel*
jbpf_io_find_channel(jbpf_io_ctx_t* io_ctx, struct jbpf_io_stream_id stream_id, bool is_output)
{

    if (!io_ctx) {
        return NULL;
    }

    if (io_ctx->io_type == JBPF_IO_IPC_PRIMARY || io_ctx->io_type == JBPF_IO_LOCAL_PRIMARY) {
        return jbpf_io_channel_exists(&io_ctx->primary_ctx.io_channels, &stream_id, is_output);
    } else if (io_ctx->io_type == JBPF_IO_IPC_SECONDARY) {
        return jbpf_io_ipc_req_find_channel(io_ctx, &stream_id, is_output);
    } else {
        return NULL;
    }
}
