// Copyright (c) Microsoft Corporation. All rights reserved.
#ifndef JBPF_MEM_MGMT_H
#define JBPF_MEM_MGMT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MOUNTS_PATH "/proc/mounts"
#define THP_ENABLED_PATH "/sys/kernel/mm/transparent_hugepage/enabled"

#define JBPF_HUGEPAGE_SIZE_1GB (1L * 1024L * 1024L * 1024L)
#define JBPF_HUGEPAGE_SIZE_2MB (2L * 1024L * 1024L)

#define JBPF_MEM_BLOCK_SIZE (1 << 25) // 32MB
#define MAX_NAME_SIZE (256)

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct jbpf_mem_ctx jbpf_mem_ctx_t;

    /**
     * @brief Enum to indicate the options for memory allocation.
     * @param JBPF_MEM_NONE No memory allocation options.
     * @param JBPF_USE_ONLY_HUGE_PAGES Use only huge pages for memory allocation.
     * @param JBPF_USE_PERS_HUGE_PAGES Use persistent huge pages for memory allocation.
     * @param JBPF_USE_SHARED_MEM Use shared memory for memory allocation.
     * @ingroup mem_mgmt
     */
    enum jbpf_mem_alloc_options
    {
        JBPF_MEM_NONE = 0,
        JBPF_USE_ONLY_HUGE_PAGES = 1 << 0,
        JBPF_USE_PERS_HUGE_PAGES = 1 << 1,
        JBPF_USE_SHARED_MEM = 1 << 2,
    };

    /**
     * @brief Enum to indicate the type of memory allocation attempt.
     * @param JBPF_NO_MEM_ALLOC_ATTEMPT No memory allocation attempt.
     * @param JBPF_MEM_ALLOC_REG Regular memory allocation.
     * @param JBPF_MEM_ALLOC_2MB 2MB huge page memory allocation.
     * @param JBPF_MEM_ALLOC_1GB 1GB huge page memory allocation.
     * @ingroup mem_mgmt
     */
    enum jbpf_mem_alloc_attempt
    {
        JBPF_NO_MEM_ALLOC_ATTEMPT = 0,
        JBPF_MEM_ALLOC_REG,
        JBPF_MEM_ALLOC_2MB,
        JBPF_MEM_ALLOC_2MB_PERS,
        JBPF_MEM_ALLOC_1GB,
        JBPF_MEM_ALLOC_1GB_PERS
    };

    /**
     * @brief Structure to hold the information of the memory allocated.
     * @param fd File descriptor of the memory allocated.
     * @param addr Address of the memory allocated.
     * @param len Length of the memory allocated.
     * @param uses_shm_open Flag to indicate if the memory was allocated using shm_open.
     * @param has_mem_name Flag to indicate if the memory has a name.
     * @param mem_name Name of the memory allocated.
     * @ingroup mem_mgmt
     */
    struct jbpf_mmap_info
    {
        int fd;
        void* addr;
        size_t len;
        bool uses_shm_open;
        bool has_mem_name;
        char mem_name[MAX_NAME_SIZE];
    };

    /**
     * @brief Initializes the heap memory that will be used for the jbpf-io-lib operations on the calling process.
     * By default, attempts to allocate 2MB huge pages and falls back to allocating regular memory on failure.
     * @param size The (minimum) amount of memory allocated for jbpf-io-lib in bytes.
     * @param use_1g_huge_pages If set to true, will try to pre-allocate 1G huge pages for the memory of the
     * jbpf-io-lib. Will allocate N huge pages, so that size <= N * 1G. If allocation of 1G pages fails, this falls
     * back to the default allocation behavior.
     * @return 0 on success, or a negative value on failure.
     * @ingroup mem_mgmt
     */
    int
    jbpf_init_memory(size_t size, bool use_1g_huge_pages);

    /**
     * @brief Initializes the memory of the jbpf-io-lib with 2GB of memory and with 1G huge pages.
     * @return 0 on success, or a negative value on failure.
     * @ingroup mem_mgmt
     */
    int
    jbpf_init_memory_default(void);

    /**
     * @brief Releases the memory pre-allocated to the jbpf-io-lib.
     * @return void
     * @ingroup mem_mgmt
     */
    void
    jbpf_destroy_memory(void);

    /**
     * @brief calloc replacement using the jbpf-io-lib heap allocated memory instead of relying to the OS allocator.
     * @param count Number of elements to allocate.
     * @param size Size of each element in bytes.
     * @return A pointer to the allocated memory on success or NULL on failure.
     * @ingroup mem_mgmt
     */
    void*
    jbpf_calloc(size_t count, size_t size);

    /**
     * @brief malloc replacement using the jbpf-io-lib heap allocated memory instead of relying to the OS allocator.
     * @param size Size of memory to allocate in bytes.
     * @return A pointer to the allocated memory on success or NULL on failure.
     * @ingroup mem_mgmt
     */
    void*
    jbpf_malloc(size_t size);

    /**
     * @brief realloc replacement using the jbpf-io-lib heap allocated memory instead of relying to the OS allocator.
     * @param ptr Pointer to the allocated memory
     * @param new_size New size for reallocation in bytes
     * @return A pointer to the reallocated memory on success or NULL on failure.
     * @ingroup mem_mgmt
     */
    void*
    jbpf_realloc(void* ptr, size_t new_size);

    /**
     * @brief free replacement using the jbpf-io-lib heap allocated memory instead of relying to the OS allocator.
     * @param ptr Pointer to the allocated memory
     * @return void
     * @ingroup mem_mgmt
     */
    void
    jbpf_free(void* ptr);

    /**
     * @brief Initializes a heap after mmapping private or shared memory from the OS. The heap is thread local, i.e.,
     * only the thread that called jbpf_create_mem_ctx() can use it. Any attempts to allocate memory using this heap
     * from another thread will fail.
     * @param size The size in bytes of the memory to allocate and assign to the heap.
     * @param meta_path_name The name of the directory where memory related meta info is stored.
     * @param mem_name A reference name for the heap. This parameter is used for mmapping named memory (e.g. shared
     * memory) and thus, the name should be unique."
     * @param flags If no flag is set, the library attempts to allocate transparent huge pages.
     * If that fails, it falls back to allocating regular memory.
     * If JBPF_USE_PERS_HUGE_PAGES flag is set, then the library attempts to allocate
     * persistent huge pages instead of transparent. If that fails, it fall back to allocating regular memory.
     * If the flag jbpf_USE_ONLY_HUGE_PAGES is set, then if huge pages cannot be allocated, fail the allocation.
     * In all cases with a huge page configuration, the library will first try to allocate 1GB huge pages if size > 1GB
     * and if that fails, it will fall back to 2MB huge pages. Finally, JBPF_USE_SHARED_MEM is used to allocate shared
     * memory instead of private memory
     * @return An opaque pointer to a memory context is return upon success or NULL on failure.
     * @ingroup mem_mgmt
     */
    jbpf_mem_ctx_t*
    jbpf_create_mem_ctx(size_t size, char* meta_path_name, char* mem_name, uint32_t flags);

    /**
     * @brief malloc replacement using the a custom heap initialized using jbpf_create_mem_ctx() or
     * jbpf_create_mem_ctx_from_mmap(). Can only be used by the thread that created the heap.
     * @param mem_ctx The memory context pointer assigned to the heap by jbpf_create_mem_ctx() or
     * jbpf_create_mem_ctx_from_mmap().
     * @param size Size of memory to allocate in bytes.
     * @return Pointer to the allocated memory.
     * @ingroup mem_mgmt
     */
    void*
    jbpf_malloc_ctx(jbpf_mem_ctx_t* mem_ctx, size_t size);

    /**
     * @brief calloc replacement using the a custom heap initialized using jbpf_create_mem_ctx() or
     * jbpf_create_mem_ctx_from_mmap().
     * @param mem_ctx The memory context pointer assigned to the heap by jbpf_create_mem_ctx() or
     * jbpf_create_mem_ctx_from_mmap().
     * @param count Number of elements to allocate.
     * @param size Size of each element in bytes.
     * @return Pointer to the allocated memory if successful or NULL if out of memory or the call was made from a thread
     * other than the one that created the heap.
     * @ingroup mem_mgmt
     */
    void*
    jbpf_calloc_ctx(jbpf_mem_ctx_t* mem_ctx, size_t count, size_t size);

    /**
     * @brief realloc replacement using the a custom heap initialized using jbpf_create_mem_ctx() or
     * jbpf_create_mem_ctx_from_mmap().
     * @param mem_ctx The memory context pointer assigned to the heap by jbpf_create_mem_ctx() or
     * jbpf_create_mem_ctx_from_mmap().
     * @param ptr Pointer to the allocated memory
     * @param new_size New size for reallocation in bytes
     * @return Pointer to the reallocated memory if successful or NULL if out of memory or the call was made from a
     * thread other than the one that created the heap.
     * @ingroup mem_mgmt
     */
    void*
    jbpf_realloc_ctx(jbpf_mem_ctx_t* mem_ctx, void* ptr, size_t new_size);

    /**
     * @brief Initializes a heap after mmapping private or shared memory from the OS using the call
     * jbpf_allocate_memory(). The behavior of the heap is the same as in the case of jbpf_create_mem_ctx().
     * @param mmap_info The context information of the mmap, as generated by the call to jbpf_allocate_memory().
     * @return An opaque pointer to a memory context is return upon success or NULL on failure.
     * @ingroup mem_mgmt
     */
    jbpf_mem_ctx_t*
    jbpf_create_mem_ctx_from_mmap(struct jbpf_mmap_info* mmap_info, char* meta_path_name);

    /**
     * @brief Destroys the heap that was created using jbpf_create_mem_ctx() or jbpf_create_mem_ctx_from_mmap()
     * @param mem_ctx The pointer to the memory context of the heap
     * @param meta_path_name The name of the directory where memory allcation metadata is stored.
     * @ingroup mem_mgmt
     * @return void
     */
    void
    jbpf_destroy_mem_ctx(jbpf_mem_ctx_t* mem_ctx, char* meta_path_name);

    /**
     * @brief Mmaps private or shared memory that can be given to jbpf_create_mem_ctx_from_mmap() to create a thread
     * local heap
     * @param size Size of mmapped memory in bytes
     * @param meta_path_name The name of the directory where memory related meta info is stored.
     * @param mem_name The name to be used for the mmap when the flag jbpf_USE_SHARED_MEM is set
     * @param mmap_info Descriptor of the mmapped memory that can be passed to jbpf_create_mem_ctx_from_mmap() to
     * create a thread local heap.
     * @param flags The flags are the same as in the call jbpf_create_mem_ctx()
     * @return The address to the mmapped memory if successful, or NULL otherwise.
     * @ingroup mem_mgmt
     */
    void*
    jbpf_allocate_memory(
        size_t size, char* meta_path_name, char* mem_name, struct jbpf_mmap_info* mmap_info, uint32_t flags);

    /**
     * @brief Attempts to attach to a shared memory region allocated with jbpf_allocate_memory.
     * @param fixed_addr If not NULL, try to mmap the memory region on the virtual address space of the process,
     * starting with address fixed_addr. If NULL, any virtual address range is allowed.
     * @param size The size of the mmapped memory in bytes.
     * @param meta_path_name The name of the directory where memory related meta info is stored.
     * @param mem_name The name of the allocated shared memory. It must be the same that was used when calling
     * jbpf_allocate_memory().
     * @param mem_name_len Length of mem_name.
     * @return The address assigned after a successful attachment to the shared memory or NULL otherwise.
     * @ingroup mem_mgmt
     */
    void*
    jbpf_attach_memory(void* fixed_addr, size_t size, char* meta_path_name, char* mem_name, size_t mem_name_len);

    /**
     * @brief Frees the memory allocated with jbpf_allocate_memory().
     * @param mmap_info Descriptor obtained through jbpf_allocate_memory.
     * @param meta_path_name The name of the directory where memory related meta info is stored.
     * @param release_shm If set to true, frees the shared memory. If false, only detaches.
     * @return 0 on success and a negative value on failure
     * @ingroup mem_mgmt
     */
    int
    jbpf_free_memory(struct jbpf_mmap_info* mmap_info, char* meta_path_name, bool release_shm);

#ifdef __cplusplus
}
#endif

#endif
