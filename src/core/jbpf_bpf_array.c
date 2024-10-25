// Copyright (c) Microsoft Corporation. All rights reserved.
#include <stdio.h>
#include <string.h>

#include "jbpf_bpf_array.h"
#include "jbpf_memory.h"

void*
jbpf_bpf_array_create(const struct jbpf_load_map_def* map_def)
{
    return jbpf_calloc_mem(map_def->max_entries, map_def->value_size);
}

void
jbpf_bpf_array_destroy(struct jbpf_map* map)
{
    jbpf_free_mem(map->data);
}
