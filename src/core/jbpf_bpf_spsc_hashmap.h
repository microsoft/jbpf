// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef JBPF_BPF_SPSC_HASHMAP_H
#define JBPF_BPF_SPSC_HASHMAP_H

#include "jbpf_bpf_spsc_hashmap_int.h"
#include "jbpf_helper_api_defs.h"
#include "jbpf_defs.h"
#include "jbpf_memory.h"
#include "jbpf_int.h"

#include "jbpf_lookup3.h"

typedef struct jbpf_bpf_spsc_hashmap jbpf_spsc_hashmap_t;

static inline __attribute__((always_inline)) uint32_t
jbpf_bpf_spsc_hashmap_hash(const void* key, size_t key_size)
{
    return hashlittle(key, key_size, 6602834);
}

void*
jbpf_bpf_spsc_hashmap_create(const struct jbpf_load_map_def* map_def);
void
jbpf_bpf_spsc_hashmap_destroy(struct jbpf_map* map);

static inline __attribute__((always_inline)) unsigned int
jbpf_bpf_spsc_hashmap_size(const struct jbpf_map* map)
{
    jbpf_spsc_hashmap_t* hmap;

    if (!map)
        return 0;

    hmap = (jbpf_spsc_hashmap_t*)map->data;
    return hmap->count;
}

static inline __attribute__((always_inline)) int
jbpf_bpf_spsc_hashmap_clear(const struct jbpf_map* map)
{
    jbpf_spsc_hashmap_t* hmap;

    if (!map)
        return JBPF_MAP_ERROR;

    hmap = (jbpf_spsc_hashmap_t*)map->data;

    hmap->count = 0;
    memset(hmap->ht, 0, (hmap->key_size + hmap->value_size + 1) * hmap->ht_size);

    return JBPF_MAP_SUCCESS;
}

static inline __attribute__((always_inline)) unsigned int
jbpf_bpf_spsc_hashmap_dump(const struct jbpf_map* map, void* data, uint32_t max_size, uint64_t flags)
{

    jbpf_spsc_hashmap_t* hmap;
    uint32_t hash_idx;
    uint8_t* entry_ptr;
    uint32_t count = 0;
    uint8_t* data_ptr;

    if (!map || !data)
        return JBPF_MAP_ERROR;

    hmap = (jbpf_spsc_hashmap_t*)map->data;

    hash_idx = 0;
    entry_ptr = (uint8_t*)hmap->ht + (hash_idx * (hmap->key_size + hmap->value_size + 1));
    data_ptr = data;

    while ((count < hmap->count) && hash_idx < hmap->ht_size) {
        if (*(uint8_t*)entry_ptr == 1) {
            memcpy(data_ptr, entry_ptr + 1, hmap->key_size);
            memcpy(data_ptr + hmap->key_size, entry_ptr + 1 + hmap->key_size, hmap->value_size);
            data_ptr += (hmap->key_size + hmap->value_size);
            count++;
        }

        hash_idx++;
    }

    return hmap->count;
}

static inline __attribute__((always_inline)) void*
jbpf_bpf_spsc_hashmap_lookup_elem(const struct jbpf_map* map, const void* key)
{
    jbpf_spsc_hashmap_t* hmap;
    uint32_t hash_idx, orig_idx;
    uint8_t* entry_ptr;

    if (!map || !key)
        return NULL;

    hmap = (jbpf_spsc_hashmap_t*)map->data;

    hash_idx = jbpf_bpf_spsc_hashmap_hash(key, hmap->key_size) & (hmap->ht_size - 1);
    orig_idx = hash_idx;

    entry_ptr = (uint8_t*)hmap->ht + (hash_idx * (hmap->key_size + hmap->value_size + 1));

    do {
        // Check if entry is used or not
        if (*(uint8_t*)entry_ptr == 0) {
            return NULL;
        }

        // Check the key
        if (memcmp(entry_ptr + 1, key, hmap->key_size) == 0) {
            return entry_ptr + hmap->key_size + 1;
        }

        // Go to the next entry
        hash_idx = (hash_idx + 1) & (hmap->ht_size - 1);
        entry_ptr = (uint8_t*)hmap->ht + (hash_idx * (hmap->key_size + hmap->value_size + 1));

    } while (orig_idx != hash_idx);

    return NULL;
}

static inline __attribute__((always_inline)) void*
jbpf_bpf_spsc_hashmap_reset_elem(const struct jbpf_map* map, const void* key)
{
    jbpf_spsc_hashmap_t* hmap;
    void* elem;

    if (!map || !key)
        return NULL;

    hmap = (jbpf_spsc_hashmap_t*)map->data;

    elem = jbpf_bpf_spsc_hashmap_lookup_elem(map, key);

    if (elem) {
        memset(elem, 0, hmap->value_size);
    }

    return elem;
}

#pragma message("WARNING: Hashmap update functionality using flags is not implemented yet")
static inline __attribute__((always_inline)) int
jbpf_bpf_spsc_hashmap_update_elem(struct jbpf_map* map, const void* key, void* value, uint64_t flags)
{
    jbpf_spsc_hashmap_t* hmap;
    uint32_t hash_idx, orig_idx;
    uint8_t* entry_ptr;

    if (!map || !key || !value)
        return JBPF_MAP_ERROR;

    hmap = (jbpf_spsc_hashmap_t*)map->data;

    hash_idx = jbpf_bpf_spsc_hashmap_hash(key, hmap->key_size) & (hmap->ht_size - 1);
    orig_idx = hash_idx;
    entry_ptr = (uint8_t*)hmap->ht + (hash_idx * (hmap->key_size + hmap->value_size + 1));

    do {
        // Check if the entry is free and we have space
        if (*(uint8_t*)entry_ptr == 0) {

            if (hmap->count == hmap->max_entries) {
                return JBPF_MAP_FULL;
            }

            *(uint8_t*)entry_ptr = 1;
            memcpy(entry_ptr + 1, key, hmap->key_size);
            memcpy(entry_ptr + hmap->key_size + 1, value, hmap->value_size);
            hmap->count++;
            return JBPF_MAP_SUCCESS;
        } else if (memcmp(entry_ptr + 1, key, hmap->key_size) == 0) {
            memcpy(entry_ptr + 1, key, hmap->key_size);
            memcpy(entry_ptr + hmap->key_size + 1, value, hmap->value_size);
            return JBPF_MAP_SUCCESS;
        }

        // Go to the next entry
        hash_idx = (hash_idx + 1) & (hmap->ht_size - 1);
        entry_ptr = (uint8_t*)hmap->ht + (hash_idx * (hmap->key_size + hmap->value_size + 1));

    } while (orig_idx != hash_idx);

    return JBPF_MAP_FULL;
}

static inline __attribute__((always_inline)) long int
jbpf_bpf_spsc_hashmap_restructure(jbpf_spsc_hashmap_t* hmap, uint32_t root_idx)
{
    uint8_t* entry_ptr;
    uint8_t* root_entry_ptr;
    uint32_t hash_idx, new_idx;

    hash_idx = root_idx;
    hash_idx = (hash_idx + 1) & (hmap->ht_size - 1);
    entry_ptr = (uint8_t*)hmap->ht + (hash_idx * (hmap->key_size + hmap->value_size + 1));
    root_entry_ptr = (uint8_t*)hmap->ht + (root_idx * (hmap->key_size + hmap->value_size + 1));

    while (hash_idx != root_idx) {

        if (*(uint8_t*)entry_ptr == 0) {
            return -1;
        }

        new_idx = jbpf_bpf_spsc_hashmap_hash(entry_ptr + 1, hmap->key_size);

        if (new_idx <= root_idx) {
            memcpy(root_entry_ptr, entry_ptr, hmap->key_size + hmap->value_size + 1);
            memset(entry_ptr, 0, hmap->key_size + hmap->value_size + 1);
            return new_idx;
        }

        // Go to the next entry
        hash_idx = (hash_idx + 1) & (hmap->ht_size - 1);
        entry_ptr = (uint8_t*)hmap->ht + (hash_idx * (hmap->key_size + hmap->value_size + 1));
    }
    return -1;
}

static inline __attribute__((always_inline)) int
jbpf_bpf_spsc_hashmap_delete_elem(struct jbpf_map* map, const void* key)
{
    jbpf_spsc_hashmap_t* hmap;
    uint32_t hash_idx, orig_idx, new_idx;
    uint8_t* entry_ptr;
    bool entry_deleted = false;

    if (!map || !key)
        return JBPF_MAP_ERROR;

    hmap = (jbpf_spsc_hashmap_t*)map->data;

    hash_idx = jbpf_bpf_spsc_hashmap_hash(key, hmap->key_size) & (hmap->ht_size - 1);
    orig_idx = hash_idx;
    entry_ptr = (uint8_t*)hmap->ht + (hash_idx * (hmap->key_size + hmap->value_size + 1));

    // Entry was empty
    if (*(uint8_t*)entry_ptr == 0) {
        return JBPF_MAP_ERROR;
    }

    do {
        // Check if the entry is free or contains the key
        if (memcmp(entry_ptr + 1, key, hmap->key_size) == 0) {
            entry_deleted = true;
            hmap->count--;
            memset(entry_ptr, 0, map->key_size + map->value_size + 1);
            break;
        }

        // Go to the next entry
        hash_idx = (hash_idx + 1) & (hmap->ht_size - 1);
        entry_ptr = (uint8_t*)hmap->ht + (hash_idx * (hmap->key_size + hmap->value_size + 1));

    } while (orig_idx != hash_idx);

    if (!entry_deleted) {
        return JBPF_MAP_ERROR;
    }

    do {
        new_idx = jbpf_bpf_spsc_hashmap_restructure(hmap, hash_idx);
    } while (new_idx != -1);

    return JBPF_MAP_SUCCESS;
}

#endif