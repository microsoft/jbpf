// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef JBPF_MEMORY_H
#define JBPF_MEMORY_H

#include <stdint.h>

#include "jbpf_utils.h"
#include "jbpf_config.h"

/* Number of BPF programs that are allowed to be loaded.
 * Currently only 254 are allowed */
enum jbpf_prog
{
    /* Max number of programs supported */
    JBPF_PROG_MAX = 0xfe,
    JBPF_PROG_TOTAL = 0xff
};

#define MEMPOOL_NAME_LEN 32U
#define MEMPOOL_CACHE_SIZE 32

typedef struct
{
    void* (*jbpf_malloc_mem_cb)(size_t size);
    void* (*jbpf_calloc_mem_cb)(size_t num, size_t size);
    void* (*jbpf_realloc_mem_cb)(void* ptr, size_t size);
    void (*jbpf_free_mem_cb)(void* ptr);
} jbpf_alloc_cbs;

typedef struct jbpf_mempool_ctx jbpf_mempool_ctx_t;

jbpf_mempool_ctx_t*
jbpf_init_data_mempool(uint32_t num_elems, size_t elem_size);

void
jbpf_destroy_data_mempool(jbpf_mempool_ctx_t* mempool);

int
jbpf_get_data_mempool_size(jbpf_mempool_ctx_t* mempool_ctx);

/* Malloc type of memory allocation */
void*
jbpf_alloc_mem(size_t size);

/* Calloc type of memory allocation */
void*
jbpf_calloc_mem(size_t num, size_t size);

/* Realloc type of memory allocation */
void*
jbpf_realloc_mem(void* ptr, size_t size);

/* Malloc type of memory allocation for fast path */
void*
jbpf_alloc_data_mem(jbpf_mempool_ctx_t* mempool_ctx);

/* Free for slow path */
void
jbpf_free_mem(void* ptr);

/* Free for fast path */
void
jbpf_free_data_mem(void* ptr);

int
jbpf_memory_setup(struct jbpf_agent_mem_config* mem_config);

void
jbpf_memory_teardown(void);

#endif
