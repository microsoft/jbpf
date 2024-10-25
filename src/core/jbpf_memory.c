// Copyright (c) Microsoft Corporation. All rights reserved.

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "jbpf_memory.h"
#include "jbpf_device_defs.h"
#include "jbpf_logging.h"
#include "jbpf_mem_mgmt.h"
#include "jbpf_mempool.h"

static uint64_t mempool_id = 0;

struct jbpf_mempool_ctx
{
    uint32_t num_elems;
    size_t elem_size;
    char name[MEMPOOL_NAME_LEN];
    // This will contain a struct of jbpf mempool
    jbpf_mempool_t* mempool;
};

jbpf_alloc_cbs alloc_cbs;

#ifndef JBPF_DEBUG_ENABLED
int
jbpf_memory_setup(struct jbpf_agent_mem_config* mem_config)
{

    if (jbpf_init_memory(mem_config->mem_size, true) != 0)
        return -1;

    alloc_cbs.jbpf_malloc_mem_cb = jbpf_malloc;
    alloc_cbs.jbpf_calloc_mem_cb = jbpf_calloc;
    alloc_cbs.jbpf_realloc_mem_cb = jbpf_realloc;
    alloc_cbs.jbpf_free_mem_cb = jbpf_free;

    return 0;
}
#else
int
jbpf_memory_setup(struct jbpf_agent_mem_config* mem_config)
{
    alloc_cbs.jbpf_malloc_mem_cb = malloc;
    alloc_cbs.jbpf_calloc_mem_cb = calloc;
    alloc_cbs.jbpf_realloc_mem_cb = realloc;
    alloc_cbs.jbpf_free_mem_cb = free;

    return 0;
}
#endif

void
jbpf_memory_teardown()
{
    jbpf_destroy_memory();
}

jbpf_mempool_ctx_t*
jbpf_init_data_mempool(uint32_t num_elems, size_t elem_size)
{

    if (!num_elems || !elem_size)
        return NULL;

    jbpf_mempool_ctx_t* mempool_ctx = jbpf_calloc_mem(1, sizeof(jbpf_mempool_ctx_t));
    if (!mempool_ctx)
        goto out;

    uint64_t mp_id = __sync_fetch_and_add(&mempool_id, 1);
    snprintf(mempool_ctx->name, MEMPOOL_NAME_LEN, "jbpf_mempool_%lu", mp_id);

    mempool_ctx->elem_size = elem_size;

    mempool_ctx->mempool = jbpf_init_mempool(num_elems, elem_size, JBPF_MEMPOOL_MP);
    if (!mempool_ctx->mempool)
        goto error;
    mempool_ctx->num_elems = jbpf_get_mempool_size(mempool_ctx->mempool);

out:
    return mempool_ctx;
error:
    jbpf_free_mem(mempool_ctx);
    return NULL;
}

int
jbpf_get_data_mempool_size(jbpf_mempool_ctx_t* mempool_ctx)
{
    if (!mempool_ctx)
        return -1;

    return mempool_ctx->num_elems;
}

void
jbpf_destroy_data_mempool(jbpf_mempool_ctx_t* mempool_ctx)
{

    jbpf_destroy_mempool(mempool_ctx->mempool);
    jbpf_free_mem(mempool_ctx);
}

/* This is used to allocate large memory chunks.
 * Should not be used for fast data path allocations */
void*
jbpf_alloc_mem(size_t size)
{
    void* p;

    p = alloc_cbs.jbpf_malloc_mem_cb(size);

    return p;
}

/* This is used to allocate large memory chunks.
 * Should not be used for fast data path allocations */
void*
jbpf_calloc_mem(size_t num, size_t size)
{
    void* p;

    p = alloc_cbs.jbpf_calloc_mem_cb(num, size);
    return p;
}

void*
jbpf_realloc_mem(void* ptr, size_t size)
{
    void* p;

    p = alloc_cbs.jbpf_realloc_mem_cb(ptr, size);
    return p;
}

/* This is used to allocate elements in data structures.
 * Allocations must be less than MBUF_DATA_SIZE bytes long */
void*
jbpf_alloc_data_mem(jbpf_mempool_ctx_t* mempool_ctx)
{

    void* p;

    struct jbpf_mbuf* jmbuf;

    jmbuf = jbpf_mbuf_alloc(mempool_ctx->mempool);

    if (!jmbuf) {
        p = NULL;
    } else {
        p = jmbuf->data;
    }

    return p;
}

/* This is used to free memory allocated with jbpf_alloc_mem() */
void
jbpf_free_mem(void* ptr)
{
    alloc_cbs.jbpf_free_mem_cb(ptr);
}

/* This is used to free memory allocated with jbpf_alloc_data_mem() */
void
jbpf_free_data_mem(void* data)
{
    jbpf_mbuf_free_from_data_ptr(data, false);
}
