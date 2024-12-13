// Copyright (c) Microsoft Corporation. All rights reserved.
// This codelet sends a single output value to the output ring buffer.
// The value is the jbpf_time_get_ns() which we can emulate in the emulator.

#include <string.h>
#include "jbpf_defs.h"
#include "jbpf_test_def.h"
#include "jbpf_helper.h"

// Send a single int value out
jbpf_ringbuf_map(output_map, uint64_t, 1000);

SEC("jbpf_generic")
uint64_t
jbpf_main(void* state)
{

    uint64_t a = jbpf_time_get_ns();

    if (jbpf_ringbuf_output(&output_map, &a, sizeof(uint64_t)) < 0) {
        return 1;
    }

    return 0;
}
