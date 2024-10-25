#ifndef JBPF_BITMAP_H_
#define JBPF_BITMAP_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "jbpf_memory.h"

#define BITMAP_BITS_PER_WORD (sizeof(unsigned int) * 8)

struct jbpf_bitmap
{
    unsigned int* data;
    int num_blocks;
    size_t size;
};

static inline void
jbpf_init_bitmap(struct jbpf_bitmap* bitmap, size_t size)
{
    size_t num_elems = (size + BITMAP_BITS_PER_WORD - 1) / BITMAP_BITS_PER_WORD;
    bitmap->data = (unsigned int*)jbpf_calloc_mem(num_elems, sizeof(unsigned int));
    bitmap->size = size;
    bitmap->num_blocks = num_elems;
}

static inline int
jbpf_allocate_bit(struct jbpf_bitmap* bitmap)
{
    for (size_t i = 0; i < bitmap->size; i++) {
        unsigned int* word = &bitmap->data[i / BITMAP_BITS_PER_WORD];
        unsigned int old_value = *word;
        unsigned int bit_mask = (1u << (i % BITMAP_BITS_PER_WORD));
        if ((old_value & bit_mask) == 0) {
            unsigned int new_value = old_value | bit_mask;
            if (__sync_bool_compare_and_swap(word, old_value, new_value)) {
                return i;
            }
        }
    }
    return -1; // No free bits available
}

static inline void
jbpf_free_bit(struct jbpf_bitmap* bitmap, int bit_index)
{
    size_t index = bit_index / BITMAP_BITS_PER_WORD;
    if (index >= bitmap->num_blocks)
        return;
    unsigned int* word = &bitmap->data[index];
    unsigned int bit_mask = (1u << (bit_index % BITMAP_BITS_PER_WORD));
    __sync_fetch_and_and(word, ~bit_mask);
}

static inline void
jbpf_destroy_bitmap(struct jbpf_bitmap* bitmap)
{
    jbpf_free_mem(bitmap->data);
}

#endif /* JBPF_BITMAP_H_ */
