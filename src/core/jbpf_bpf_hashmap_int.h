// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef JBPF_BPF_HASHMAP_INT_H
#define JBPF_BPF_HASHMAP_INT_H

#include "ck_ht.h"
#include "ck_epoch.h"
#include "ck_spinlock.h"

#include "jbpf_memory.h"

struct jbpf_bpf_hashmap
{
    unsigned int key_size;
    unsigned int value_size;
    unsigned int max_entries;
    ck_ht_t ht;
    jbpf_mempool_ctx_t* mempool;
    ck_spinlock_t lock;
};

#endif
