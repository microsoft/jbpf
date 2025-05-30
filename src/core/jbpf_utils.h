// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef JBPF_UTILS_H_
#define JBPF_UTILS_H_

#include <sys/time.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdbool.h>

/* Use "%"PRIuSIZE to format size_t with printf(). */
#ifdef _WIN32
#define PRIdSIZE "Id"
#define PRIiSIZE "Ii"
#define PRIoSIZE "Io"
#define PRIuSIZE "Iu"
#define PRIxSIZE "Ix"
#define PRIXSIZE "IX"
#else
#define PRIdSIZE "zd"
#define PRIiSIZE "zi"
#define PRIoSIZE "zo"
#define PRIuSIZE "zu"
#define PRIxSIZE "zx"
#define PRIXSIZE "zX"
#endif

#define JBPF_SOURCE_LOCATOR __FILE__ ":" JBPF_STRINGIZE(__LINE__)
#define JBPF_STRINGIZE(ARG) JBPF_STRINGIZE2(ARG)
#define JBPF_STRINGIZE2(ARG) #ARG

#define is_power_of_2(x) ((x) != 0 && (((x) & ((x)-1)) == 0))

#define IS_ALIGNED(x, a) (((x) & ((typeof(x))(a)-1)) == 0)

#ifndef MIN
#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#endif
#ifndef MAX
#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))
#endif

#define JBPF_LIKELY(CONDITION) __builtin_expect(!!(CONDITION), 1)
#define JBPF_UNLIKELY(CONDITION) __builtin_expect(!!(CONDITION), 0)

#define JBPF_UNUSED(x) ((void)(x))

#define JBPF_MAX_PRINTF_STR_LEN 64

/*
 * Compute the next highest power of 2 of 32-bit x.
 */
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

#define __round_mask(x, y) ((__typeof__(x))((y)-1))
#define round_up(x, y) ((((x)-1) | __round_mask(x, y)) + 1)

int
_jbpf_set_thread_affinity(pthread_t thread, uint64_t cpuset_mask);

uint32_t
random_uint32(void);

int
check_unsafe_string(char* unsafe);

static __inline uint64_t __attribute__((__gnu_inline__, __always_inline__, __artificial__)) jbpf_start_time(void)
{
    uint64_t x;
#if defined(__x86_64__)
    asm volatile(".intel_syntax noprefix  \n\t"
                 "mfence                  \n\t"
                 "lfence                  \n\t"
                 "rdtsc                   \n\t"
                 "shl     rdx, 0x20       \n\t"
                 "or      rax, rdx        \n\t"
                 ".att_syntax prefix      \n\t"

                 : "=a"(x)

                 :
                 : "rdx");
#elif defined(__aarch64__)
    asm volatile("isb; mrs %0, cntvct_el0; isb; " : "=r"(x)::"memory");
#else
#pragma message "Non x86_64 or aarch64 architecture. Will measure time using clock_gettime()"
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    x = (uint64_t)ts.tv_sec * (uint64_t)1000000000 + (uint64_t)ts.tv_nsec;
#endif
    return x;
}

static __inline uint64_t __attribute__((__gnu_inline__, __always_inline__, __artificial__)) jbpf_end_time(void)
{
    uint64_t x;
#if defined(__x86_64__)
    asm volatile(".intel_syntax noprefix  \n\t"
                 "rdtscp                  \n\t"
                 "lfence                  \n\t"
                 "shl     rdx, 0x20       \n\t"
                 "or      rax, rdx        \n\t"
                 ".att_syntax prefix      \n\t"

                 : "=a"(x)
                 :
                 : "rdx", "rcx");
#elif defined(__aarch64__)
    asm volatile("isb; mrs %0, cntvct_el0; isb; " : "=r"(x)::"memory");
#else
#pragma message "Non x86_64 or aarch64 architecture. Will measure time using clock_gettime()"
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    x = (uint64_t)ts.tv_sec * (uint64_t)1000000000 + (uint64_t)ts.tv_nsec;
#endif
    return x;
}

static inline int64_t
_jbpf_difftimespec_ns(const struct timespec* end_time, const struct timespec* start_time)
{
    return ((int64_t)end_time->tv_sec - (int64_t)start_time->tv_sec) * (int64_t)1000000000 +
           ((int64_t)end_time->tv_nsec - (int64_t)start_time->tv_nsec);
}

#endif /* JBPF_UTILS_H_ */
