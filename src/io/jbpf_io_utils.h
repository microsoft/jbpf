#ifndef JBPF_IO_UTILS_H
#define JBPF_IO_UTILS_H

#include <sys/utsname.h>
#include <stddef.h>
#include <fcntl.h>

#define JBPF_IO_UNUSED(x) ((void)(x))

int
_jbpf_io_tohex_str(char* in, size_t insz, char* out, size_t outsz);

int
_jbpf_io_kernel_below_3_17(void);

int
_jbpf_io_open_mem_fd(const char* name);

int
_jbpf_io_close_mem_fd(int shm_fd, const char* name);

void*
_jbpf_io_load_lib(int shm_fd, const char* name);

int
_jbpf_io_unload_lib(void* handle);

int
_jbpf_io_write_lib(void* data, size_t size, int fd);

#define round_up_pow_of_two(x) \
    ({                         \
        uint32_t v = x;        \
        v--;                   \
        v |= v >> 1;           \
        v |= v >> 2;           \
        v |= v >> 4;           \
        v |= v >> 8;           \
        v |= v >> 16;          \
        ++v;                   \
    })

#endif

ssize_t
recv_all(int sock_fd, void* buf, size_t len, int flags);

ssize_t
send_all(int sock_fd, const void* buf, size_t len, int flags);
