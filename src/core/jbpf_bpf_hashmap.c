// Copyright (c) Microsoft Corporation. All rights reserved.

#include "jbpf_bpf_hashmap.h"
#include "jbpf_bpf_hashmap_int.h"
#include "jbpf_memory.h"
#include "jbpf_int.h"
#include "jbpf_helper_api_defs.h"

#include "jbpf_lookup3.h"

typedef struct jbpf_bpf_hashmap jbpf_hashmap_t;

static void*
ht_malloc(size_t r)
{
    return jbpf_alloc_mem(r);
}

static void
ht_free(void* p, size_t b, bool r)
{
    (void)b;
    (void)r;
    jbpf_free_mem(p);
}

static struct ck_malloc hmap_allocator = {
    .malloc = ht_malloc,
    .free = ht_free,
};

static void
ht_hash_wrapper(struct ck_ht_hash* h, const void* key, size_t length, uint64_t seed)
{

    h->value = (unsigned long)hashlittle(key, length, seed);
    return;
}

void*
jbpf_bpf_hashmap_create(const struct jbpf_load_map_def* map_def)
{

    struct jbpf_bpf_hashmap* hmap;

    if (!map_def)
        return NULL;

    hmap = jbpf_calloc_mem(1, sizeof(struct jbpf_bpf_hashmap));

    if (!hmap)
        return NULL;

    hmap->key_size = map_def->key_size;
    hmap->value_size = map_def->value_size;
    hmap->max_entries = map_def->max_entries;
    ck_spinlock_init(&hmap->lock);

    if (!ck_ht_init(
            &hmap->ht, CK_HT_MODE_BYTESTRING, ht_hash_wrapper, &hmap_allocator, map_def->max_entries, 6602834)) {
        jbpf_free_mem(hmap);
        return NULL;
    }

    hmap->mempool = jbpf_init_data_mempool(
        map_def->max_entries * 2, sizeof(ck_epoch_entry_t) + map_def->key_size + map_def->value_size);
    if (!hmap->mempool) {
        ck_ht_destroy(&hmap->ht);
        jbpf_free_mem(hmap);
        return NULL;
    }

    return hmap;
}

void
jbpf_bpf_hashmap_destroy(struct jbpf_map* map)
{

    jbpf_hashmap_t* hmap;

    if (!map)
        return;

    hmap = (jbpf_hashmap_t*)map->data;

    jbpf_call_barrier();

    jbpf_destroy_data_mempool(hmap->mempool);
    ck_ht_destroy(&hmap->ht);
    jbpf_free_mem(hmap);
}
