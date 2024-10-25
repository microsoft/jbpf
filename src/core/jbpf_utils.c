// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <ctype.h>
#include <sys/utsname.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <errno.h>
#include <sched.h>
#include <linux/version.h>

#include "jbpf_utils.h"
#include "jbpf_logging.h"

#if defined CONFIG_LINUX && !defined CONFIG_MEMFD || LINUX_VERSION_CODE <= KERNEL_VERSION(3, 17, 0)
#include <sys/syscall.h>
static inline int
memfd_create(const char* name, unsigned int flags)
{
#ifdef __NR_memfd_create
    return syscall(__NR_memfd_create, name, flags);
#else
    errno = ENOSYS;
    return -1;
#endif
}
#endif

int
check_unsafe_string(char* unsafe)
{

    int result = 0, strlen;
    int fd[2];
    char safe[JBPF_MAX_PRINTF_STR_LEN];
    int max_length = JBPF_MAX_PRINTF_STR_LEN;
    errno = 0;

    if (pipe(fd) >= 0) {
        strlen = 0;
        for (int i = 0; i < max_length; i++) {
            result = write(fd[1], &unsafe[i], 1);
            if (result < 0 || (size_t)result != 1 || errno == EFAULT) {
                goto final;
            } else {
                strlen++;
                if (unsafe[i] == '\0') {
                    result = read(fd[0], safe, strlen);
                    if ((result < 0 || (size_t)result != strlen || errno == EFAULT)) {
                        result = -1;
                        goto final;
                    }
                    if (result > 0)
                        goto final;
                }
            }
        }
        result = 0;
    final:
        close(fd[0]);
        close(fd[1]);
    }
    return result;
}

int
_jbpf_set_thread_affinity(pthread_t thread, uint64_t cpuset_mask)
{
    int i, s;
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    for (i = 0; i < 64; i++) {
        if (((uint64_t)1 << i) & cpuset_mask)
            CPU_SET(i, &cpuset);
    }

    // printf("Setting affinity of thread %ld to 0x%.8lX\n", thread, cpuset_mask);
    s = pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
    if (s != 0)
        goto affinity_error;

    s = pthread_getaffinity_np(thread, sizeof(cpuset), &cpuset);
    if (s != 0)
        goto affinity_error;

    return 0;

affinity_error:
    printf("WARNING: Affinity of thread with id %ld could not be set\n", thread);
    return -1;
}
