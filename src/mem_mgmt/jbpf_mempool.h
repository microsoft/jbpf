// Copyright (c) Microsoft Corporation. All rights reserved.
#ifndef JBPF_MEMPOOL_H
#define JBPF_MEMPOOL_H

#include <stdbool.h>

#include "jbpf_mem_mgmt.h"

typedef struct jbpf_mempool jbpf_mempool_t;

/**
 * @brief Enumeration of the mempool types.
 * @ingroup mempool
 * @param JBPF_MEMPOOL_MP Multi-thread allocation and free of memory
 * @param JBPF_MEMPOOL_SP Single-thread allocation and free of memory
 */
enum mempool_type
{
    JBPF_MEMPOOL_MP = 0,
    JBPF_MEMPOOL_SP,
};

/**
 * @brief The jbpf memory buffer
 * @param ref_cnt Reference count
 * @param mempool Memory pool
 * @param data Data
 * @ingroup mempool
 */
typedef struct jbpf_mbuf
{
    size_t ref_cnt;
    jbpf_mempool_t* mempool;
    uint8_t data[];
} jbpf_mbuf_t;

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Create a mempool using memory that was allocated from the default heap of jbpf-io-lib, using
     * the jbpf_init_memory() call.
     * @param n_elems Number of jbpf_mbuf_t elements in the pool.
     * @param elem_size Size of each element in bytes.
     * @param mempool_type // JBPF_MEMPOOL_MP, if several threads will allocate and free the buffers,
     * JBPF_MEMPOOL_SP, if only a single thread allocates and frees the buffers.
     * @return An opaque pointer to the context of the created mempool upon success or NULL on failure.
     * @ingroup mempool
     */
    jbpf_mempool_t*
    jbpf_init_mempool(uint32_t n_elems, size_t elem_size, int mempool_type);

    /**
     * @brief Create a mempool using memory that was allocated from a thread-local heap of jbpf-io-lib, using
     * the jbpf_create_mem_ctx() call.
     * @param mem_ctx Pointer to the memory context to be used for the allocation.
     * @param n_elems Number of jbpf_mbuf_t elements in the pool.
     * @param elem_size Size of each element in bytes.
     * @param mempool_type // JBPF_MEMPOOL_MP, if several threads will allocate and free the buffers,
     * JBPF_MEMPOOL_SP, if only a single thread allocates and frees the buffers.
     * @return An opaque pointer to the context of the created mempool upon success or NULL on failure.
     * @ingroup mempool
     */
    jbpf_mempool_t*
    jbpf_init_mempool_ctx(jbpf_mem_ctx_t* mem_ctx, uint32_t n_elems, size_t elem_size, int mempool_type);

    /**
     * @brief Destroy a mempool.
     * @param mempool Pointer to the mempool to be destroyed.
     * @ingroup mempool
     */
    void
    jbpf_destroy_mempool(jbpf_mempool_t* mempool);

    /**
     * @brief Allocates a jbpf_mbuf_t from a mempool.
     * @param mempool Pointer to the mempool context to be used for the reservation.
     * @return A pointer to the mbuf upon success or NULL if a buffer could not be allocated.
     * @ingroup mempool
     *
     */
    jbpf_mbuf_t*
    jbpf_mbuf_alloc(jbpf_mempool_t* mempool);

    /**
     * @brief Free a jbpf_mbuf_t to its mempool.
     * @param mbuf Pointer to the jbpf_mbuf to be freed.
     * @param reset If true, resets the data section of the mbuf to 0.
     * @ingroup mempool
     */
    void
    jbpf_mbuf_free(jbpf_mbuf_t* mbuf, bool reset);

    /**
     * @brief Free a jbpf_mbuf_t to its mempool.
     * @param data_ptr Pointer to the data field of the allocated jbpf_mbuf.
     * @param reset If true, resets the data section of the mbuf to 0.
     * @ingroup mempool
     */
    void
    jbpf_mbuf_free_from_data_ptr(void* data_ptr, bool reset);

    /**
     * @brief Call that is used to increment manual reference counter of jbpf_mbufs.
     * For the jbpf_mbuf to be actually freed, jbpf_mbuf_free() must be called as many times as
     * jbpf_mbuf_share().
     * @param mbuf Pointer to the jbpf_mbuf.
     * @return Pointer to the same jbpf_mbuf, with reference counter incremented by one.
     * @ingroup mempool
     *
     */
    jbpf_mbuf_t*
    jbpf_mbuf_share(jbpf_mbuf_t* mbuf);

    /**
     * @brief Call that is used to increment manual reference counter of jbpf_mbufs.
     * For the jbpf_mbuf to be actually freed, jbpf_mbuf_free() must be called as many times as
     * jbpf_mbuf_share().
     * @param data_ptr Pointer to the data field of the allocated jbpf_mbuf.
     * @return Pointer to the jbpf_mbuf, with reference counter incremented by one.
     * @ingroup mempool
     */
    void*
    jbpf_mbuf_share_data_ptr(void* data_ptr);

    /**
     * @brief Returns the number of free elements in the pool.
     * @param mempool Pointer to the mempool to get the size of.
     * @return Size of the jbpf mempool.
     * @ingroup mempool
     */
    int
    jbpf_get_mempool_size(jbpf_mempool_t* mempool);

    /**
     * @brief Returns the capacity of the mempool.
     * @param mempool Pointer to the mempool to get the capacity of.
     * @return Capacity of the jbpf mempool.
     * @ingroup mempool
     */
    int
    jbpf_get_mempool_capacity(jbpf_mempool_t* mempool);

#ifdef __cplusplus
}
#endif

#endif