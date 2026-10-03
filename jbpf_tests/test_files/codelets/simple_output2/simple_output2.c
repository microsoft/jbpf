// Copyright (c) Microsoft Corporation. All rights reserved.

#include <string.h>
#include "jbpf_defs.h"
#include "jbpf_test_def.h"
#include "jbpf_helper.h"

// Send a single int value out
jbpf_ringbuf_map(output_map, int, 10);

SEC("jbpf_generic")
uint64_t
jbpf_main(void* state)
{
    int data = 123;
    if (jbpf_ringbuf_output(&output_map, &data, sizeof(data)) < 0) {
        return 1;
    }

    return 0;
}
