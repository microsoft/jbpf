#ifndef JBPF_MEM_MGMT_UTILS_H
#define JBPF_MEM_MGMT_UTILS_H

#include <stddef.h>

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

static inline size_t
_jbpf_round_up_mem(size_t orig_size, size_t multiple_size)
{
    if (multiple_size == 0)
        return orig_size;

    size_t remainder = orig_size % multiple_size;
    if (remainder == 0)
        return orig_size;
    else
        return orig_size + multiple_size - remainder;
}

#endif