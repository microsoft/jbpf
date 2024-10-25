// Copyright (c) Microsoft Corporation. All rights reserved.
#include <mntent.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/mman.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

#include "mimalloc.h"

#include "jbpf_mem_mgmt.h"
#include "jbpf_mem_mgmt_int.h"
#include "jbpf_mem_mgmt_utils.h"

#include "jbpf_logging.h"

#define NUM_PERS_ENTRIES (10)

static bool memory_initialized = false;

bool
_jbpf_transparent_hp_enabled(void)
{
    FILE* fp;
    char buf[MAX_NAME_SIZE];

    if ((fp = fopen(THP_ENABLED_PATH, "r")) == NULL) {
        jbpf_logger(JBPF_ERROR, "Error opening file %s\n", THP_ENABLED_PATH);
        return false;
    }

    if (fgets(buf, sizeof(buf), fp) == NULL) {
        jbpf_logger(JBPF_ERROR, "Error reading file %s\n", THP_ENABLED_PATH);
        fclose(fp);
        return false; // Error reading from the file
    }

    fclose(fp);

    // Check if the content contains "always" or "madvise" to verify THP is enabled
    return (strstr(buf, "[always]") != NULL || strstr(buf, "[madvise]") != NULL);
}

int
_jbpf_write_hp_alloc_info(
    struct jbpf_huge_page_info* huge_pages_info, char* meta_path_name, char* mem_name, size_t mem_name_len)
{
    struct stat st;
    char shm_meta_name[512];
    int res = 0;
    if (stat(meta_path_name, &st) == -1) {

        if (mkdir(meta_path_name, S_IRWXU | S_IRWXG) == -1) {
            jbpf_logger(JBPF_ERROR, "Error creating directory %s\n", meta_path_name);
            res = -1;
            goto out;
        }
        jbpf_logger(JBPF_INFO, "Directory '%s' created.\n", meta_path_name);
    } else {
        jbpf_logger(JBPF_INFO, "Directory '%s' already exists.\n", meta_path_name);
    }

    snprintf(shm_meta_name, 512, "%s/%s", meta_path_name, mem_name);

    FILE* file = fopen(shm_meta_name, "wb");
    if (file == NULL) {
        jbpf_logger(JBPF_ERROR, "Error opening file %s\n", shm_meta_name);
        res = -1;
        goto out;
    }

    if (huge_pages_info) {
        if (fwrite(huge_pages_info, sizeof(struct jbpf_huge_page_info), 1, file) != 1) {
            jbpf_logger(JBPF_ERROR, "Error writing to file %s\n", shm_meta_name);
            res = -1;
        }
    }

    fclose(file);

    if (chmod(shm_meta_name, S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
        res = -1;
    }
out:
    return res;
}

void
_jbpf_remove_hp_alloc_info(char* meta_path_name, char* mem_name, size_t mem_name_len)
{
    char shm_meta_name[512];

    snprintf(shm_meta_name, 512, "%s/%s", meta_path_name, mem_name);

    remove(shm_meta_name);
}

int
_jbpf_read_hp_alloc_info(
    struct jbpf_huge_page_info* huge_pages_info, char* meta_path_name, char* mem_name, size_t mem_name_len)
{
    char shm_meta_name[512];
    int res = 0;

    snprintf(shm_meta_name, 512, "%s/%s", meta_path_name, mem_name);

    jbpf_logger(JBPF_INFO, "Will try to read file %s\n", shm_meta_name);

    FILE* file = fopen(shm_meta_name, "rb");
    if (file == NULL) {
        jbpf_logger(JBPF_ERROR, "Error opening file %s\n", shm_meta_name);
        res = -1;
        goto out;
    }

    res = fread(huge_pages_info, sizeof(struct jbpf_huge_page_info), 1, file);

    fclose(file);
out:
    return res;
}

int
_jbpf_get_persistent_hp_info(struct jbpf_huge_page_info* huge_pages_info, int n_elems)
{

    FILE* mounts_file = setmntent(MOUNTS_PATH, "r");
    if (mounts_file == NULL) {
        jbpf_logger(JBPF_ERROR, "Error opening /proc/mounts\n");
        return -1;
    }

    int num_huge_pages = 0;

    const struct mntent* entry;
    while ((entry = getmntent(mounts_file)) != NULL && num_huge_pages < n_elems) {
        if (strcmp(entry->mnt_type, "hugetlbfs") == 0) {
            // Store the mount point and file system information
            strcpy(huge_pages_info[num_huge_pages].mount_point, entry->mnt_dir);
            strcpy(huge_pages_info[num_huge_pages].file_system, entry->mnt_fsname);
            huge_pages_info[num_huge_pages].is_anonymous = false;

            // Retrieve the huge page size from the mount options
            char* opt_ptr = entry->mnt_opts;
            int page_size;
            while (*opt_ptr) {
                if (strncmp(opt_ptr, "pagesize=", 9) == 0) {
                    sscanf(opt_ptr + 9, "%d", &page_size);
                    if (page_size == 1024) {
                        huge_pages_info[num_huge_pages].page_size = MAP_HUGE_1GB;
                    } else if (page_size == 2) {
                        huge_pages_info[num_huge_pages].page_size = MAP_HUGE_2MB;
                    } else {
                        return -1;
                    }
                    break;
                }
                while (*opt_ptr && (*opt_ptr != ',')) {
                    opt_ptr++; // Move to the next option
                }
                if (*opt_ptr == ',') {
                    opt_ptr++; // Skip the comma
                }
            }

            // Retrieve the reserved huge pages from /proc/meminfo
            FILE* meminfo_file = fopen("/proc/meminfo", "r");
            if (meminfo_file != NULL) {
                char line[256];
                while (fgets(line, sizeof(line), meminfo_file) != NULL) {
                    if (strncmp(line, "HugePages_Total:", 16) == 0) {
                        sscanf(line + 16, "%lu", &huge_pages_info[num_huge_pages].n_reserved_pages);
                        break;
                    }
                }
                fclose(meminfo_file);
            } else {
                jbpf_logger(JBPF_ERROR, "Error opening /proc/meminfo\n");
            }

            num_huge_pages++;
        }
    }

    endmntent(mounts_file);

    return num_huge_pages < n_elems ? num_huge_pages : n_elems;
}

int
_jbpf_get_file_pathname(int fd, char* out, int out_len)
{
    char buffer[1024];
    ssize_t path_length;

    // Construct the path to the symbolic link in /proc/self/fd
    // The fd path format is /proc/self/fd/<file_descriptor>
    snprintf(buffer, sizeof(buffer), "/proc/self/fd/%d", fd);

    // Read the symbolic link to get the actual file name and path
    path_length = readlink(buffer, out, out_len - 1);

    if (path_length == -1) {
        jbpf_logger(JBPF_ERROR, "Error reading symbolic link\n");
        return -1;
    }

    // Null-terminate the path string
    out[path_length] = '\0';
    return path_length;
}

void
_jbpf_remove_file(struct jbpf_mmap_info* mmap_info)
{
    char file_pathname[1024];

    if (mmap_info->uses_shm_open) {
        close(mmap_info->fd);
        shm_unlink(mmap_info->mem_name);
    } else {
        _jbpf_get_file_pathname(mmap_info->fd, file_pathname, 1024);
        jbpf_logger(JBPF_INFO, "Removing the file %s\n", file_pathname);
        close(mmap_info->fd);
        unlink(file_pathname);
    }
}

void*
_jbpf_mmap(
    size_t size,
    char* meta_path_name,
    char* mem_name,
    struct jbpf_mmap_info* mmap_info,
    bool is_shared_mem,
    struct jbpf_huge_page_info* hp_alloc_info,
    void* fixed_addr)
{

    void* addr;
    int mmap_prot = PROT_READ | PROT_WRITE;
    mmap_info->fd = -1;
    int mmap_flags;

    mmap_flags = 0;
    mmap_info->len = size;
    if (hp_alloc_info) {
        mmap_flags |= MAP_HUGETLB;
        mmap_flags |= hp_alloc_info->page_size;

        if (!hp_alloc_info->is_anonymous) {
            char mount_name[4 * MAX_NAME_SIZE];
            mmap_flags |= MAP_SHARED;
            snprintf(mount_name, sizeof(mount_name), "%s/%s", hp_alloc_info->mount_point, mem_name);
            strncpy(mmap_info->mem_name, mem_name, sizeof(mmap_info->mem_name) - 1);
            mmap_info->mem_name[sizeof(mmap_info->mem_name) - 1] = '\0';
            mmap_info->fd = open(mount_name, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
            mmap_info->uses_shm_open = false;
            mmap_info->has_mem_name = true;
            if (mmap_info->fd == -1)
                return NULL;
            if (ftruncate(mmap_info->fd, size) == -1) {
                close(mmap_info->fd);
                return NULL;
            }

        } else {
            mmap_flags |= (MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE);
            mmap_info->has_mem_name = false;
        }
    } else {
        jbpf_logger(JBPF_INFO, "No huge page allocation\n");
        if (is_shared_mem) {
            mmap_flags |= MAP_SHARED;
            strncpy(mmap_info->mem_name, mem_name, sizeof(mmap_info->mem_name) - 1);
            mmap_info->mem_name[sizeof(mmap_info->mem_name) - 1] = '\0';
            jbpf_logger(JBPF_INFO, "Openning shared memory with name %s\n", mem_name);
            mmap_info->fd = shm_open(mem_name, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
            mmap_info->uses_shm_open = true;
            mmap_info->has_mem_name = true;
            if (mmap_info->fd == -1) {
                jbpf_logger(JBPF_ERROR, "Error opening shared memory %s\n", mem_name);
                return NULL;
            }
            if (ftruncate(mmap_info->fd, size) == -1) {
                shm_unlink(mem_name);
                return NULL;
            }
        } else {
            mmap_flags |= (MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE);
            mmap_info->has_mem_name = false;
        }
    }

    if (fixed_addr) {
        mmap_flags |= MAP_FIXED_NOREPLACE;
    }

    addr = mmap(fixed_addr, size, mmap_prot, mmap_flags, mmap_info->fd, 0);
    if ((addr != MAP_FAILED) && (mmap_flags & MAP_SHARED)) {
        // Write the metadata about the allocation, so that other processes know how to attach
        if (_jbpf_write_hp_alloc_info(
                hp_alloc_info, meta_path_name, mmap_info->mem_name, sizeof(mmap_info->mem_name)) != 0) {
            munmap(addr, size);
            return NULL;
        }
    }
    return addr;
}

void*
jbpf_attach_memory(void* fixed_addr, size_t size, char* meta_path_name, char* mem_name, size_t mem_name_len)
{
    void* addr;
    struct jbpf_huge_page_info hp_info = {0};
    int res;
    int mmap_flags = 0;
    int fd;

    res = _jbpf_read_hp_alloc_info(&hp_info, meta_path_name, mem_name, mem_name_len);

    if (fixed_addr) {
        mmap_flags |= MAP_FIXED;
    }

    if (res == -1) {
        return NULL;
    } else if (res == 0) {
        // This is plain shared memory
        mmap_flags |= MAP_SHARED;
        fd = shm_open(mem_name, O_RDWR, S_IRUSR | S_IWUSR);
        if (fd == -1)
            return NULL;
    } else {
        // This is a huge pages configuration
        mmap_flags |= MAP_HUGETLB;
        mmap_flags |= hp_info.page_size;
        mmap_flags |= MAP_SHARED;
        char mount_name[4 * MAX_NAME_SIZE];

        snprintf(mount_name, sizeof(mount_name), "%s/%s", hp_info.mount_point, mem_name);

        fd = open(mount_name, O_RDWR, S_IRUSR | S_IWUSR);

        if (fd == -1) {
            return NULL;
        }
    }

    addr = mmap(fixed_addr, size, PROT_READ | PROT_WRITE, mmap_flags, fd, 0);

    if (addr == MAP_FAILED) {
        addr = NULL;
        if (res == 0) {
            shm_unlink(mem_name);
        }
    }

    close(fd);

    return addr;
}

// jbpf follows the following allocation logic:
// 1) If no flag is set, try to allocate transparent huge pages and
// if that fails, allocate regular memory
// 2) If JBPF_USE_PERS_HUGE_PAGES flag is set, then try to allocate
// persistent huge pages instead of transparent. If that fails, fall back
// to transparent and then to regular memory
// 3) If flag JBPF_USE_ONLY_HUGE_PAGES is set, then if we cannot allocate
// huge pages, fail the allocation
// In all huge page cases, jbpf will first try to allocate 1GB huge pages if size > 1GB
// and if that fails, it will fall back to 2MB huge pages
// JBPF_USE_SHARED_MEM is used to allocate shared memory instead of private memory
void*
jbpf_allocate_memory(
    size_t size, char* meta_path_name, char* mem_name, struct jbpf_mmap_info* mmap_info, uint32_t flags)
{

    struct jbpf_huge_page_info hp_info[NUM_PERS_ENTRIES];
    struct jbpf_huge_page_info* hp_pers_entry;
    int num_hp_mounts;
    void* addr;
    int alloc_attempt;
    size_t size_req = size;
    struct jbpf_huge_page_info info;

    num_hp_mounts = _jbpf_get_persistent_hp_info(hp_info, NUM_PERS_ENTRIES);

    if (size_req > (JBPF_HUGEPAGE_SIZE_2MB)) {
        alloc_attempt = JBPF_MEM_ALLOC_1GB_PERS;
    } else {
        alloc_attempt = JBPF_MEM_ALLOC_2MB_PERS;
    }

    while (alloc_attempt > 0) {
        hp_pers_entry = NULL;
        size_req = size;
        memset(&info, 0, sizeof(info));

        if (alloc_attempt == JBPF_MEM_ALLOC_1GB_PERS) {
            size_req = _jbpf_round_up_mem(size_req, JBPF_HUGEPAGE_SIZE_1GB);
            jbpf_logger(JBPF_INFO, "Trying 1GB persistent huge pages (%ld bytes)\n", size_req);
            for (int i = 0; i < num_hp_mounts; i++) {
                if (hp_info[i].page_size == MAP_HUGE_1GB) {
                    hp_pers_entry = &hp_info[i];
                }
            }
        } else if (alloc_attempt == JBPF_MEM_ALLOC_1GB) {
            if (flags & JBPF_USE_PERS_HUGE_PAGES) {
                alloc_attempt--;
                continue;
            }
            size_req = _jbpf_round_up_mem(size_req, JBPF_HUGEPAGE_SIZE_1GB);
            jbpf_logger(JBPF_INFO, "Trying 1GB transparent huge pages (%ld bytes)\n", size_req);
            info.is_anonymous = true;
            info.page_size = MAP_HUGE_1GB;
            hp_pers_entry = &info;
        } else if ((alloc_attempt == JBPF_MEM_ALLOC_2MB_PERS)) {
            size_req = _jbpf_round_up_mem(size_req, JBPF_HUGEPAGE_SIZE_2MB);
            jbpf_logger(JBPF_INFO, "Trying 2MB persistent huge pages (%ld bytes)\n", size_req);
            for (int i = 0; i < num_hp_mounts; i++) {
                if (hp_info[i].page_size == MAP_HUGE_2MB) {
                    hp_pers_entry = &hp_info[i];
                }
            }
        } else if (alloc_attempt == JBPF_MEM_ALLOC_2MB) {
            if (flags & JBPF_USE_PERS_HUGE_PAGES) {
                alloc_attempt--;
                continue;
            }
            size_req = _jbpf_round_up_mem(size_req, JBPF_HUGEPAGE_SIZE_1GB);
            jbpf_logger(JBPF_INFO, "Trying 2MB transparent huge pages (%ld bytes)\n", size_req);
            info.is_anonymous = true;
            info.page_size = MAP_HUGE_1GB;
            hp_pers_entry = &info;
        } else {
            size_req += JBPF_MEM_BLOCK_SIZE;
            jbpf_logger(JBPF_INFO, "Let's try to allocate regular memory\n");
        }

        addr =
            _jbpf_mmap(size_req, meta_path_name, mem_name, mmap_info, flags & JBPF_USE_SHARED_MEM, hp_pers_entry, NULL);
        if ((addr != MAP_FAILED) && (addr != NULL)) {
            jbpf_logger(JBPF_INFO, "Memory allocated successfully at address %p\n", addr);
            if (mlock(addr, size_req) == 0) {
                jbpf_logger(JBPF_INFO, "Allocated pages mlocked in memory\n");
            } else {
                jbpf_logger(JBPF_WARN, "Could not mlock allocated pages\n");
            }
            mmap_info->addr = addr;
            memset(addr, 0, mmap_info->len);
            return addr;
        } else {
            jbpf_logger(JBPF_WARN, "Could not allocate memory\n");
            if ((flags & JBPF_USE_ONLY_HUGE_PAGES) && (alloc_attempt <= JBPF_MEM_ALLOC_2MB))
                break;
        }

        alloc_attempt--;
    }
    return NULL;
}

int
jbpf_free_memory(struct jbpf_mmap_info* mmap_info, char* meta_path_name, bool release_shm)
{
    if (mmap_info->has_mem_name) {
        _jbpf_remove_hp_alloc_info(meta_path_name, mmap_info->mem_name, strlen(mmap_info->mem_name));
    }

    if (munmap(mmap_info->addr, mmap_info->len) == -1) {
        return -1;
    }

    if (release_shm && mmap_info->has_mem_name) {
        _jbpf_remove_file(mmap_info);
    }
    return 0;
}

struct jbpf_mem_ctx*
jbpf_create_mem_ctx(size_t size, char* meta_path_name, char* mem_name, uint32_t flags)
{
    struct jbpf_mmap_info mmap_info;

    const void* addr = jbpf_allocate_memory(size, meta_path_name, mem_name, &mmap_info, flags);

    if (!addr) {
        return NULL;
    }

    return jbpf_create_mem_ctx_from_mmap(&mmap_info, meta_path_name);
}

struct jbpf_mem_ctx*
jbpf_create_mem_ctx_from_mmap(struct jbpf_mmap_info* mmap_info, char* meta_path_name)
{
    struct jbpf_mem_ctx* mem_ctx;
    bool allocated;
    uintptr_t uptr_addr;
    void* align_addr;
    uintptr_t align_ptr = (uintptr_t)JBPF_MEM_BLOCK_SIZE;

    mem_ctx = calloc(1, sizeof(struct jbpf_mem_ctx));
    if (!mem_ctx) {
        goto err_free_memory;
    }

    mem_ctx->mem_ctx_tid = pthread_self();

    uptr_addr = (uintptr_t)mmap_info->addr;

    align_addr = (void*)((uptr_addr + align_ptr - 1) & -align_ptr);

    allocated = mi_manage_os_memory_ex(align_addr, mmap_info->len, true, true, true, -1, true, &mem_ctx->arena_id);

    if (allocated) {
        mem_ctx->mmap_info = *mmap_info;
        jbpf_logger(
            JBPF_DEBUG,
            "Successfully created memory arena of size %ld at address %p\n",
            mmap_info->len,
            mmap_info->addr);
    } else {
        jbpf_logger(
            JBPF_DEBUG, "Failed to create memory arena of size %ld at address %p\n", mmap_info->len, mmap_info->addr);
        goto err_free_ctx;
    }

    size_t allocated_size;
    mi_arena_area(mem_ctx->arena_id, &allocated_size);

    mem_ctx->heap = mi_heap_new_in_arena(mem_ctx->arena_id);

    if (!mem_ctx->heap) {
        jbpf_logger(JBPF_ERROR, "Failed to create heap. Tearing down allocation\n");
        goto err_free_ctx;
    }

    return mem_ctx;

err_free_ctx:
    free(mem_ctx);
err_free_memory:
    jbpf_free_memory(mmap_info, meta_path_name, true);
    return NULL;
}

void
jbpf_destroy_mem_ctx(jbpf_mem_ctx_t* mem_ctx, char* meta_path_name)
{
    mi_heap_destroy(mem_ctx->heap);
    jbpf_free_memory(&mem_ctx->mmap_info, meta_path_name, true);
    free(mem_ctx);
}

int
jbpf_init_memory(size_t size, bool use_1g_huge_pages)
{

    size_t num_1g_huge_pages;
    bool done = false;

    if (memory_initialized) {
        jbpf_logger(JBPF_INFO, "Memory is already initialized. Ignoring...\n");
        return 0;
    }

    mi_option_disable(mi_option_show_errors);

    if (use_1g_huge_pages > 0) {
        num_1g_huge_pages = 1 + (size / JBPF_HUGEPAGE_SIZE_1GB);
        jbpf_logger(JBPF_INFO, "Trying to reserve %ld 1G huge pages\n", num_1g_huge_pages);
        if (mi_reserve_huge_os_pages_interleave(num_1g_huge_pages, 0, 0) != 0) {
            jbpf_logger(JBPF_WARN, "Unable to reserve 1G huge pages\n");
        } else {
            done = true;
        }
    }

    if (!done) {
        jbpf_logger(JBPF_INFO, "Trying to reserve regular memory\n");
        if (mi_reserve_os_memory(size, true, true) != 0) {
            jbpf_logger(JBPF_ERROR, "Unable to reserve regular memory\n");
            return -1;
        }
    }

    jbpf_logger(JBPF_INFO, "Allocated memory for jbpf\n");

    mi_option_enable(mi_option_limit_os_alloc);
    mi_option_disable(mi_option_purge_decommits);

    memory_initialized = true;
    return 0;
}

int
jbpf_init_memory_default()
{
    return jbpf_init_memory(JBPF_HUGEPAGE_SIZE_1GB, true);
}

void
jbpf_destroy_memory()
{

    memory_initialized = false;
    mi_heap_delete(mi_heap_get_default());
    mi_option_disable(mi_option_limit_os_alloc);
}

void*
jbpf_calloc(size_t count, size_t size)
{
    return mi_calloc(count, size);
}

void*
jbpf_calloc_ctx(struct jbpf_mem_ctx* mem_ctx, size_t count, size_t size)
{

    if (!mem_ctx) {
        return jbpf_calloc(count, size);
    }

    if (!pthread_equal(mem_ctx->mem_ctx_tid, pthread_self())) {
        jbpf_logger(JBPF_ERROR, "Trying to allocate the memory from the wrong thread\n");
        return NULL;
    }

    return mi_heap_calloc(mem_ctx->heap, count, size);
}

void*
jbpf_malloc(size_t size)
{
    return mi_malloc(size);
}

void*
jbpf_malloc_ctx(struct jbpf_mem_ctx* mem_ctx, size_t size)
{
    if (!mem_ctx) {
        return jbpf_malloc(size);
    }

    if (!pthread_equal(mem_ctx->mem_ctx_tid, pthread_self())) {
        jbpf_logger(JBPF_ERROR, "Trying to allocate the memory from the wrong thread\n");
        return NULL;
    }

    return mi_heap_malloc(mem_ctx->heap, size);
}

void*
jbpf_realloc(void* ptr, size_t new_size)
{
    return mi_realloc(ptr, new_size);
}

void*
jbpf_realloc_ctx(struct jbpf_mem_ctx* mem_ctx, void* ptr, size_t new_size)
{
    if (!mem_ctx) {
        return jbpf_realloc(ptr, new_size);
    }

    if (!pthread_equal(mem_ctx->mem_ctx_tid, pthread_self())) {
        jbpf_logger(JBPF_ERROR, "Trying to allocate the memory from the wrong thread\n");
        return NULL;
    }

    return mi_heap_realloc(mem_ctx->heap, ptr, new_size);
}

void
jbpf_free(void* ptr)
{
    mi_free(ptr);
}
