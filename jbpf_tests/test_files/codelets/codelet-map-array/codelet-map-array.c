// Copyright (c) Microsoft Corporation. All rights reserved.
/*
This codelet tests the jbpf_map_lookup_elem and jbpf_map_try_update_elem
calling the hook first time - the counter_a and counter_b not changed
and subsequent calls will increment counter_a by one and counter_b by two
Then, use ring buffer to output counter_a
*/

#include <string.h>
#include <stdlib.h>
#include "jbpf_defs.h"
#include "jbpf_helper.h"
#include "jbpf_test_def.h"

jbpf_ringbuf_map(map2, req_resp, 100)

    struct jbpf_load_map_def SEC("maps") map1 = {
        .type = JBPF_MAP_TYPE_ARRAY,
        .key_size = sizeof(int),
        .value_size = sizeof(struct packet),
        .max_entries = 1,
};

SEC("jbpf_generic")
uint64_t
jbpf_main(void* state)
{
    struct jbpf_generic_ctx* ctx = (struct jbpf_generic_ctx*)state;
    volatile struct packet* data = (struct packet*)ctx->data;
    volatile struct packet* data_end = (struct packet*)ctx->data_end;
    if (data + 1 > data_end) {
        return 1;
    }
    // jbpf_printf_debug("Start is %p and end is %p\n", data, data_end);
    // jbpf_printf_debug("Size of packet is %d and of context is %d\n", sizeof(struct packet), sizeof(struct
    // jbpf_generic_ctx));
    struct packet* tmp;
    struct packet tmp2 = {0};
    int index = 0;
    tmp = jbpf_map_lookup_elem(&map1, &index);
    if (!tmp) {
        return 1;
    }
    // jbpf_printf_debug("Tmp_a = %d, Tmp_b = %d\n", tmp->counter_a, tmp->counter_b);
    if (tmp->counter_a == 0 && tmp->counter_b == 0) {
        // the first time we store it in the map
        tmp->counter_a = 1;
        tmp->counter_b = 2;
        tmp2 = *tmp;
        if (jbpf_map_try_update_elem(&map1, &index, &tmp2, 0) != JBPF_MAP_SUCCESS) {
            return 1;
        }
    } else {
        // then subsequent call should increment the counter as we expect.
        // jbpf_printf_debug("Tmp_a = %d, Tmp_b = %d\n", tmp->counter_a, tmp->counter_b)
        // jbpf_printf_debug("Start is %p and end is %p\n", data, data_end);
        data->counter_a += tmp->counter_a;
        data->counter_b += tmp->counter_b;
        // jbpf_printf_debug("Did a counter update\n");
    }
    // jbpf_printf_debug("Counter_a = %d, Counter_b = %d\n", data->counter_a, data->counter_b);
    //  send to ring buffer - output thread
    req_resp out_data = {0};
    out_data.which_req_or_resp = 2;
    out_data.req_or_resp.resp.id = data->counter_a;
    out_data.req_or_resp.resp.id = 10;
    if (jbpf_ringbuf_output(&map2, &out_data, sizeof(req_resp)) < 0) {
        return 1;
    }
    return 0;
}
