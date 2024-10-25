// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef JBPF_BPF_HASHMAP_H
#define JBPF_BPF_HASHMAP_H

#include "jbpf_helper_api_defs.h"
#include "jbpf_bpf_hashmap_int.h"
#include "jbpf_memory.h"

#include "jbpf_lookup3.h"

#include "jbpf.h"
#include "jbpf_int.h"

typedef struct jbpf_bpf_hashmap jbpf_hashmap_t;

/**
 * @brief Call the barrier function
 * @ingroup core
 */
void
jbpf_call_barrier(void);

/**
 * @brief free a hnode
 * @param e The epoch entry
 * @ingroup core
 */
static inline void
free_hnode(ck_epoch_entry_t* e)
{
    jbpf_free_data_mem(e);
}

/**
 * @brief Create a new bpf hashmap
 * @param map_def The map definition
 * @return The hashmap
 * @ingroup core
 */
void*
jbpf_bpf_hashmap_create(const struct jbpf_load_map_def* map_def);

/**
 * @brief Destroy a bpf hashmap
 * @param map The map to destroy
 * @ingroup core
 */
void
jbpf_bpf_hashmap_destroy(struct jbpf_map* map);

/**
 * @brief Get the size of the hashmap
 * @param map The map
 * @return The size of the hashmap
 * @ingroup core
 */
static inline __attribute__((always_inline)) unsigned int
jbpf_bpf_hashmap_size(const struct jbpf_map* map)
{
    jbpf_hashmap_t* hmap;

    if (!map)
        return 0;

    hmap = (jbpf_hashmap_t*)map->data;

    if (ck_spinlock_trylock(&hmap->lock)) {
        unsigned int count = ck_ht_count(&hmap->ht);
        ck_spinlock_unlock(&hmap->lock);
        return count;
    }
    return JBPF_MAP_BUSY;
}

/**
 * @brief Clear the hashmap
 * @param map The map
 * @return JBPF_MAP_SUCCESS on success.
 * Possible return values:
 * - JBPF_MAP_SUCCESS: Success
 * - JBPF_MAP_BUSY: The map is busy
 * - JBPF_MAP_ERROR: Error
 * @ingroup core
 */
static inline __attribute__((always_inline)) int
jbpf_bpf_hashmap_clear(const struct jbpf_map* map)
{
    jbpf_hashmap_t* hmap;
    ck_ht_iterator_t iterator = CK_HT_ITERATOR_INITIALIZER;
    ck_ht_entry_t* cursor;
    uint8_t* value;
    void* key;
    ck_ht_hash_t h;
    int res = JBPF_MAP_SUCCESS;

    if (!map)
        return JBPF_MAP_ERROR;

    hmap = (jbpf_hashmap_t*)map->data;

    if (ck_spinlock_trylock(&hmap->lock)) {
        ck_ht_iterator_init(&iterator);

        while (ck_ht_next(&hmap->ht, &iterator, &cursor) == true) {
            key = ck_ht_entry_key(cursor);
            value = ck_ht_entry_value(cursor);
            ck_ht_hash(&h, &hmap->ht, key, hmap->key_size);
            ck_epoch_call_strict(e_record, (ck_epoch_entry_t*)value, free_hnode);
            ck_ht_remove_spmc(&hmap->ht, h, cursor);
        }

        ck_spinlock_unlock(&hmap->lock);
    } else {
        res = JBPF_MAP_BUSY;
    }

    return res;
}

/**
 * @brief Dump the bpf hashmap
 * @param map The map
 * @param data The data buffer
 * @param max_size The maximum size of the buffer
 * @param flags The flags (currently not used)
 * @return The number of elements in the hashmap
 * @ingroup core
 */
static inline __attribute__((always_inline)) unsigned int
jbpf_bpf_hashmap_dump(const struct jbpf_map* map, void* data, uint32_t max_size, uint64_t flags)
{

    jbpf_hashmap_t* hmap;
    ck_ht_iterator_t iterator = CK_HT_ITERATOR_INITIALIZER;
    ck_ht_entry_t* cursor;
    uint8_t* value;
    int count = 0;
    uint8_t* dataptr = data;

    if (!map)
        return 0;

    hmap = (jbpf_hashmap_t*)map->data;

    if (ck_spinlock_trylock(&hmap->lock)) {
        uint32_t required_size = ck_ht_count(&hmap->ht) * (hmap->key_size + hmap->value_size);
        if (required_size > max_size) {
            ck_spinlock_unlock(&hmap->lock);
            goto out;
        }

        ck_ht_iterator_init(&iterator);
        while (ck_ht_next(&hmap->ht, &iterator, &cursor) == true) {
            value = ck_ht_entry_value(cursor);
            memcpy(dataptr, value + sizeof(ck_epoch_entry_t), hmap->key_size);
            dataptr += hmap->key_size;
            memcpy(dataptr, value + sizeof(ck_epoch_entry_t) + hmap->key_size, hmap->value_size);
            dataptr += hmap->value_size;
            count++;
        }

        ck_spinlock_unlock(&hmap->lock);
    }

out:
    return count;
}

/**
 * @brief Lookup an element in the bpf ashmap
 * @param map The map
 * @param key The key
 * @note thread-safe: yes
 * @return The value of the element
 * Possible return values:
 * - NULL: Element not found or the map is NULL
 * - void*: The element
 * @ingroup core
 */
static inline __attribute__((always_inline)) void*
jbpf_bpf_hashmap_lookup_elem(const struct jbpf_map* map, const void* key)
{

    jbpf_hashmap_t* hmap;
    ck_ht_entry_t entry;
    ck_ht_hash_t h;

    if (!map || !key)
        return NULL;

    hmap = (jbpf_hashmap_t*)map->data;

    ck_ht_hash(&h, &hmap->ht, key, hmap->key_size);
    ck_ht_entry_key_set(&entry, key, hmap->key_size);

    if (ck_ht_get_spmc(&hmap->ht, h, &entry) == false) {
        return NULL;
    } else {
        uint8_t* v;
        v = ck_ht_entry_value(&entry);
        return v + sizeof(ck_epoch_entry_t) + hmap->key_size;
    }
}

/**
 * @brief Reset an element in the bpf hashmap
 * @param map The map
 * @param key The key
 * @return The value of the element
 * Possible return values:
 * - NULL: Element not found or the map is NULL
 * - void*: The element
 * @ingroup core
 */
static inline __attribute__((always_inline)) void*
jbpf_bpf_hashmap_reset_elem(const struct jbpf_map* map, const void* key)
{

    jbpf_hashmap_t* hmap;
    ck_ht_entry_t entry;
    ck_ht_hash_t h;

    if (!map || !key)
        return NULL;

    hmap = (jbpf_hashmap_t*)map->data;

    ck_ht_hash(&h, &hmap->ht, key, hmap->key_size);
    ck_ht_entry_key_set(&entry, key, hmap->key_size);

    if (ck_ht_get_spmc(&hmap->ht, h, &entry) == false) {
        return NULL;
    } else {
        uint8_t* v;
        v = ck_ht_entry_value(&entry);
        v += sizeof(ck_epoch_entry_t) + hmap->key_size;
        memset(v, 0, hmap->value_size);
        return v;
    }
}

/**
 * @brief Update an element in the bpf hashmap
 * @param map The map
 * @param key The key
 * @param value The value
 * @param flags The flags
 * @return The status of the operation
 * Possible return values:
 * - JBPF_MAP_SUCCESS: Success
 * - JBPF_MAP_BUSY: The map is busy
 * - JBPF_MAP_FULL: The map is full
 * - JBPF_MAP_ERROR: Error
 * @ingroup core
 */
#pragma message("WARNING: Hashmap update functionality using flags is not implemented yet")
static inline __attribute__((always_inline)) int
jbpf_bpf_hashmap_update_elem(struct jbpf_map* map, const void* key, void* value, uint64_t flags)
{

    jbpf_hashmap_t* hmap;
    ck_ht_entry_t entry;
    ck_ht_hash_t h;
    uint8_t* val;
    bool found = false;
    void *key_e, *val_e;
    void* ret_val;
    int res = 0;

    if (!map || !key || !value)
        return JBPF_MAP_ERROR;

    hmap = (jbpf_hashmap_t*)map->data;

    if (!ck_spinlock_trylock(&hmap->lock)) {
        return -JBPF_MAP_BUSY;
    }

    val = jbpf_alloc_data_mem(hmap->mempool);

    if (!val) {
        res = JBPF_MAP_ERROR;
        goto out;
    }

    ck_ht_hash(&h, &hmap->ht, key, hmap->key_size);
    ck_ht_entry_key_set(&entry, key, hmap->key_size);

    found = ck_ht_get_spmc(&hmap->ht, h, &entry);

    if (!found && ck_ht_count(&hmap->ht) == hmap->max_entries) {
        res = JBPF_MAP_FULL;
        goto out;
    }

    key_e = val + sizeof(ck_epoch_entry_t);
    val_e = val + sizeof(ck_epoch_entry_t) + hmap->key_size;

    memcpy(key_e, key, hmap->key_size);
    memcpy(val_e, value, hmap->value_size);

    ck_ht_hash(&h, &hmap->ht, key_e, hmap->key_size);
    ck_ht_entry_set(&entry, h, key_e, hmap->key_size, val);

    ck_ht_set_spmc(&hmap->ht, h, &entry);

    ret_val = ck_ht_entry_value(&entry);

    if (ret_val && ret_val != val) {
        ck_epoch_call_strict(e_record, (ck_epoch_entry_t*)ret_val, free_hnode);
    } else if (ret_val == val) {
        jbpf_free_data_mem(val);
    }

out:
    ck_spinlock_unlock(&hmap->lock);
    return res;
}

/**
 * @brief Delete an element in the bpf hashmap
 * @param map The map
 * @param key The key
 * @note thread-safe: yes
 * @return The status of the operation
 * Possible return values:
 * - JBPF_MAP_SUCCESS: Success
 * - JBPF_MAP_BUSY: The map is busy
 * - JBPF_MAP_ERROR: Error
 * @ingroup core
 */
static inline __attribute__((always_inline)) int
jbpf_bpf_hashmap_delete_elem(struct jbpf_map* map, const void* key)
{

    jbpf_hashmap_t* hmap;
    ck_ht_entry_t entry;
    ck_ht_hash_t h;
    void* ret_val;
    int res = JBPF_MAP_ERROR;

    if (!map || !key)
        return JBPF_MAP_ERROR;

    hmap = (jbpf_hashmap_t*)map->data;

    if (!ck_spinlock_trylock(&hmap->lock)) {
        return JBPF_MAP_BUSY;
    }

    ck_ht_hash(&h, &hmap->ht, key, hmap->key_size);
    ck_ht_entry_key_set(&entry, key, hmap->key_size);

    if (ck_ht_remove_spmc(&hmap->ht, h, &entry)) {

        ret_val = ck_ht_entry_value(&entry);
        ck_epoch_call_strict(e_record, (ck_epoch_entry_t*)ret_val, free_hnode);
        res = JBPF_MAP_SUCCESS;
    }

    ck_spinlock_unlock(&hmap->lock);
    return res;
}

#endif
