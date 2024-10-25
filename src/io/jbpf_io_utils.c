// Copyright (c) Microsoft Corporation. All rights reserved.
#define _GNU_SOURCE
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <ctype.h>
#include <unistd.h>

#include "jbpf_io_utils.h"
#include "jbpf_logging.h"

int
_jbpf_io_tohex_str(char* in, size_t insz, char* out, size_t outsz)
{
    char* pin = in;
    const char* hex = "0123456789abcdef";
    char* pout = out;

    if (!in || !out || outsz < ((insz * 2) + 1))
        return -1;

    for (; pin < in + insz; pout += 2, pin++) {
        pout[0] = hex[(*pin >> 4) & 0xF];
        pout[1] = hex[*pin & 0xF];
        if (pout + 2 - out > outsz) {
            break;
        }
    }
    pout[0] = 0;
    return 0;
}

int
_jbpf_io_kernel_below_3_17()
{
    static int already_checked;
    static int kernel_below;

    struct utsname buffer;
    char* p;
    long ver[16];
    int i = 0;

    if (!already_checked) {
        already_checked = 1;
        /* We could not determine the version, so let's play it safe */
        if (uname(&buffer) != 0) {
            kernel_below = 1;
        } else {
            p = buffer.release;

            while (*p) {
                if (isdigit(*p)) {
                    ver[i] = strtol(p, &p, 10);
                    i++;
                } else {
                    p++;
                }
            }

            if ((ver[0] < 3) || ((ver[0] == 3) && (ver[1] < 17))) {
                kernel_below = 1;
            } else {
                kernel_below = 0;
            }
        }
    }

    return kernel_below;
}

int
_jbpf_io_open_mem_fd(const char* name)
{
    int shm_fd = -1;
    if (_jbpf_io_kernel_below_3_17())
        shm_fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, S_IRWXU);
    else
        shm_fd = memfd_create(name, 0);

    if (shm_fd < 0) {
        jbpf_logger(JBPF_ERROR, "Could not create in-memory file descriptor %d\n", errno);
        if (errno == EEXIST)
            return -2;
        else
            return -1;
    }

    return shm_fd;
}

int
_jbpf_io_close_mem_fd(int shm_fd, const char* name)
{
    int res = 0;

    res = close(shm_fd);

    if (res < 0) {
        jbpf_logger(JBPF_ERROR, "Error closing shf_fd %d for %s\n", shm_fd, name);
        return res;
    }

    if (_jbpf_io_kernel_below_3_17())
        res = shm_unlink(name);

    return res;
}

void*
_jbpf_io_load_lib(int shm_fd, const char* name)
{
    char path[1024];
    void* handle;

    if (!_jbpf_io_kernel_below_3_17()) {
        snprintf(path, 1024, "/proc/%d/fd/%d", getpid(), shm_fd);
    } else {
        close(shm_fd);
        snprintf(path, 1024, "/dev/shm/%s", name);
    }
    jbpf_logger(JBPF_INFO, "Opening %s\n", path);
#ifndef ASAN
    handle = dlopen(path, RTLD_NOW | RTLD_DEEPBIND);
#else
    /* Address sanitizer cannot use RTLD_DEEPBIND.
     That should be fine, because we only use the sanitizer
     for local testing, where shared library versioning is not an issue */
    handle = dlopen(path, RTLD_NOW);
#endif
    if (!handle) {
        fprintf(stderr, "dlopen(%s) failed with error: %s\n", path, dlerror());
        return NULL;
    }

    return handle;
}

int
_jbpf_io_unload_lib(void* handle)
{
    return dlclose(handle);
}

int
_jbpf_io_write_lib(void* data, size_t size, int fd)
{
    return write(fd, data, size);
}