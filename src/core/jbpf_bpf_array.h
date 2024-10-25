// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef JBPF_BPF_ARRAY_H
#define JBPF_BPF_ARRAY_H

#include "jbpf_defs.h"
#include "jbpf_helper_api_defs.h"
#include "jbpf_utils.h"

#include "jbpf_int.h"

/**
 * @brief Create a new array map
 * @param map_def The map definition
 * @ingroup core
 */
void*
jbpf_bpf_array_create(const struct jbpf_load_map_def* map_def);

/**
 * @brief Destroy an array map
 * @param map The map to destroy
 * @ingroup core
 */
void
jbpf_bpf_array_destroy(struct jbpf_map* map);

/**
 * @brief Insert an element into the array map
 * @param map The map
 * @param key The key
 * @param value The value
 * @param flags The flags
 * @return JBPF_MAP_SUCCESS on success or JBPF_MAP_ERROR on failure
 * @note thread-safe: yes
 * @ingroup core
 */
static inline __attribute__((always_inline)) void*
jbpf_bpf_array_lookup_elem(const struct jbpf_map* map, const void* key)
{

    uint32_t index = *(uint32_t*)key;

    if (JBPF_UNLIKELY(index >= map->max_entries)) {
        return NULL;
    }
    return (void*)((uint64_t)map->data + index * map->value_size);
}

/**
 * @brief Clear the array map
 * @param map The map
 * @return JBPF_MAP_SUCCESS on success.
 * @ingroup core
 */
static inline __attribute__((always_inline)) int
jbpf_bpf_array_clear(const struct jbpf_map* map)
{
    memset(map->data, 0, map->max_entries * map->value_size);
    return JBPF_MAP_SUCCESS;
}

/**
 * @brief Set an element in the array map to zero
 * @param map The map
 * @param key The key
 * @return The element on success, NULL on failure
 * @ingroup core
 */
static inline __attribute__((always_inline)) void*
jbpf_bpf_array_reset_elem(const struct jbpf_map* map, const void* key)
{
    uint32_t index = *(uint32_t*)key;

    if (JBPF_UNLIKELY(index >= map->max_entries)) {
        return NULL;
    }
    void* elem = (void*)((uint64_t)map->data + index * map->value_size);
    memset(elem, 0, map->value_size);
    return elem;
}

/**
 * @brief Update an element from the array map
 * @param map The map
 * @param key The key
 * @param value The value
 * @ingroup core
 * @return JBPF_MAP_SUCCESS on success or JBPF_MAP_ERROR on failure
 */
#pragma message("WARNING: Array update functionality using flags is not implemented yet")
static inline __attribute__((always_inline)) int
jbpf_bpf_array_update_elem(struct jbpf_map* map, const void* key, void* value, uint64_t flags)
{
    uint32_t index = *(uint32_t*)key;

    if (JBPF_UNLIKELY(index >= map->max_entries)) {
        return JBPF_MAP_ERROR;
    }
    void* addr = (void*)((uint64_t)map->data + map->value_size * index);
    memcpy(addr, value, map->value_size);
    return JBPF_MAP_SUCCESS;
}

#endif
