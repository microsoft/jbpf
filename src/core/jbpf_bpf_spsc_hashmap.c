#include "jbpf_bpf_spsc_hashmap.h"
#include "jbpf_utils.h"

void*
jbpf_bpf_spsc_hashmap_create(const struct jbpf_load_map_def* map_def)
{

    struct jbpf_bpf_spsc_hashmap* hmap;

    if (!map_def)
        return NULL;

    hmap = jbpf_calloc_mem(1, sizeof(struct jbpf_bpf_spsc_hashmap));

    if (!hmap)
        return NULL;

    hmap->key_size = map_def->key_size;
    hmap->value_size = map_def->value_size;
    hmap->max_entries = map_def->max_entries;
    hmap->count = 0;

    // Make hashtable to be power of 2 for faster lookups
    hmap->ht_size = round_up_pow_of_two(hmap->max_entries);

    // Each entry of the table has the following structure | used | key | value |
    // The "used" value designates whether an entry is empty (0) or used (1)
    hmap->ht = jbpf_calloc_mem(hmap->ht_size, hmap->key_size + hmap->value_size + 1);

    if (!hmap->ht) {
        jbpf_free_mem(hmap);
        return NULL;
    }

    return hmap;
}

void
jbpf_bpf_spsc_hashmap_destroy(struct jbpf_map* map)
{

    jbpf_spsc_hashmap_t* hmap;

    if (!map)
        return;

    hmap = (jbpf_spsc_hashmap_t*)map->data;

    jbpf_free_mem(hmap->ht);
    jbpf_free_mem(hmap);
}
