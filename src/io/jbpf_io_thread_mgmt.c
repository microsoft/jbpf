// Copyright (c) Microsoft Corporation. All rights reserved.
#include <stdio.h>

#include "jbpf_io_thread_mgmt.h"
#include "jbpf_io_int.h"

#include "ck_bitmap.h"
#include "ck_epoch.h"

#include "jbpf_logging.h"

static _Thread_local int16_t jbpf_io_thread_id = -1;

static ck_bitmap_t* thread_mask;

void
jbpf_io_thread_ctx_init()
{

    unsigned int bytes;
    bytes = ck_bitmap_size(JBPF_IO_MAX_NUM_THREADS);

    thread_mask = jbpf_calloc(1, bytes);

    ck_bitmap_init(thread_mask, JBPF_IO_MAX_NUM_THREADS, false);
}

void
jbpf_io_thread_ctx_destroy()
{
    free(thread_mask);
}

bool
jbpf_io_register_thread()
{

    bool thread_allocated;
    unsigned int i, num_threads;

    if (jbpf_io_thread_id >= 0) {
        return true;
    }

    thread_allocated = false;
    num_threads = ck_bitmap_bits(thread_mask);
    for (i = 0; i < num_threads; i++) {
        if (!ck_bitmap_bts(thread_mask, i)) {
            jbpf_logger(JBPF_INFO, "Registered thread id %d\n", i);
            thread_allocated = true;
            jbpf_io_thread_id = i;
            break;
        }
    }

    if (!thread_allocated) {
        jbpf_io_thread_id = -1;
        jbpf_logger(JBPF_ERROR, "Error: Was unable to register new thread\n");
        return false;
    }

    local_list_epoch_record = &list_epoch_records[jbpf_io_thread_id];
    local_in_channel_list_epoch_record = &in_channel_list_epoch_records[jbpf_io_thread_id];
    local_out_channel_list_epoch_record = &out_channel_list_epoch_records[jbpf_io_thread_id];

    return true;
}

void
jbpf_io_remove_thread()
{

    if (jbpf_io_thread_id < 0)
        return;

    ck_bitmap_reset(thread_mask, jbpf_io_thread_id);

    local_list_epoch_record = NULL;
    local_in_channel_list_epoch_record = NULL;
    local_out_channel_list_epoch_record = NULL;

    jbpf_io_thread_id = -1;
}

int16_t
jbpf_io_get_thread_id()
{
    if (jbpf_io_thread_id < 0)
        jbpf_io_register_thread();

    return jbpf_io_thread_id;
}
