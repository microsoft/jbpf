#ifndef JBPF_MEM_MGMT_INT_H
#define JBPF_MEM_MGMT_INT_H

#include "jbpf_mem_mgmt.h"
#include <pthread.h>
#include "mimalloc.h"

struct jbpf_huge_page_info
{
    char mount_point[MAX_NAME_SIZE];
    char file_system[MAX_NAME_SIZE];
    uint64_t page_size;
    uint64_t n_reserved_pages;
    bool is_anonymous;
};

struct jbpf_mem_ctx
{
    mi_arena_id_t arena_id;
    mi_heap_t* heap;
    pthread_t mem_ctx_tid;
    struct jbpf_mmap_info mmap_info;
};

#endif