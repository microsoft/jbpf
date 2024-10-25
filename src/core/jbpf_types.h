// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef JBPF_TYPES_H_
#define JBPF_TYPES_H_

#ifdef __CHECKER__
#define JBPF_BITWISE __attribute__((bitwise))
#define JBPF_FORCE __attribute__((force))
#else
#define JBPF_BITWISE
#define JBPF_FORCE
#endif

/* The jbe<N> types indicate that an object is in big-endian, not
 * native-endian, byte order.  They are otherwise equivalent to uint<N>_t. */
typedef uint16_t JBPF_BITWISE jbe16;
typedef uint32_t JBPF_BITWISE jbe32;
typedef uint64_t JBPF_BITWISE jbe64;

typedef union
{
    uint32_t u32[4];
    struct
    {
#ifdef WORDS_BIGENDIAN
        uint64_t hi, lo;
#else
        uint64_t lo, hi;
#endif
    } u64;
} j_u128;

#endif
