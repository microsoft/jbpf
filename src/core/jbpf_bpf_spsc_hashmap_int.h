#ifndef JBPF_BPF_SPSC_HASHMAP_INT_H
#define JBPF_BPF_SPSC_HASHMAP_INT_H

struct jbpf_bpf_spsc_hashmap
{
    unsigned int key_size;
    unsigned int value_size;
    unsigned int max_entries;
    unsigned int ht_size;
    void* ht;
    int count;
};

#endif