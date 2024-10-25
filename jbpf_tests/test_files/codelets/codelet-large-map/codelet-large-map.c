// Copyright (c) Microsoft Corporation. All rights reserved.

#include <string.h>

#include "jbpf_defs.h"
#include "jbpf_test_def.h"
#include "jbpf_helper.h"

jbpf_ringbuf_map(map1, req_resp, 100);

jbpf_ringbuf_map(map2, status, 100);

jbpf_ringbuf_map(map3, item, 100);

jbpf_ringbuf_map(map4, req_resp, 100);

jbpf_ringbuf_map(map5, status, 100);

struct jbpf_load_map_def SEC("maps") map1_tmp = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(req_resp),
    .max_entries = 1,
};

struct jbpf_load_map_def SEC("maps") map2_tmp = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(status),
    .max_entries = 1,
};

struct jbpf_load_map_def SEC("maps") map3_tmp = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(item),
    .max_entries = 1,
};

struct jbpf_load_map_def SEC("maps") p1_counter = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(uint32_t),
    .value_size = sizeof(uint64_t),
    .max_entries = 20,
};

struct jbpf_load_map_def SEC("maps") map4_tmp = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(req_resp),
    .max_entries = 1,
};

struct jbpf_load_map_def SEC("maps") map5_tmp = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(status),
    .max_entries = 1,
};

struct jbpf_load_map_def SEC("maps") map6_tmp = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(item),
    .max_entries = 1,
};

struct jbpf_load_map_def SEC("maps") p2_counter = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(uint32_t),
    .value_size = sizeof(uint64_t),
    .max_entries = 20,
};

struct jbpf_load_map_def SEC("maps") map7_tmp = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(req_resp),
    .max_entries = 1,
};

struct jbpf_load_map_def SEC("maps") map8_tmp = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(status),
    .max_entries = 1,
};

struct jbpf_load_map_def SEC("maps") map9_tmp = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(item),
    .max_entries = 1,
};

struct jbpf_load_map_def SEC("maps") p3_counter = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(uint32_t),
    .value_size = sizeof(uint64_t),
    .max_entries = 20,
};

struct jbpf_load_map_def SEC("maps") map10_tmp = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(req_resp),
    .max_entries = 1,
};

struct jbpf_load_map_def SEC("maps") map11_tmp = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(status),
    .max_entries = 1,
};

struct jbpf_load_map_def SEC("maps") map12_tmp = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(item),
    .max_entries = 1,
};

struct jbpf_load_map_def SEC("maps") p4_counter = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(uint32_t),
    .value_size = sizeof(uint64_t),
    .max_entries = 20,
};

SEC("jbpf_generic")
uint64_t
jbpf_main(void* state)
{

    void* c;
    uint64_t counter;
    uint64_t index = 0;
    char* dummy_str = "1234567890";

    /* build a dummy example req_resp */
    response* out;
    c = jbpf_map_lookup_elem(&map1_tmp, &index);
    if (!c)
        return 1;
    out = (response*)c;
    out->id = 1;
    strncpy(out->msg, dummy_str, 5);

    // send example req_resp
    if (jbpf_ringbuf_output(&map1, out, sizeof(req_resp)) < 0) {
        return 1;
    }

    /* build a dummy example status */
    status* out2;
    c = jbpf_map_lookup_elem(&map2_tmp, &index);
    if (!c)
        return 1;
    out2 = (status*)c;
    out2->id = 33;
    strncpy(out2->status, dummy_str, 3);
    out2->a_struct.a_num = 44;
    out2->a_struct.has_another_num = true;
    out2->a_struct.another_num = 45;

    // send example status
    if (jbpf_ringbuf_output(&map2, out2, sizeof(status)) < 0) {
        return 1;
    }

    /* build a dummy example2 item */
    item* out3;
    c = jbpf_map_lookup_elem(&map3_tmp, &index);
    if (!c)
        return 1;
    out3 = (item*)c;
    strncpy(out3->name, dummy_str, 3);
    out3->has_val = true;
    out3->val = 222;

    // send example2 item
    if (jbpf_ringbuf_output(&map3, out3, sizeof(item)) < 0) {
        return 1;
    }

    /* inc counter in p1_counter map */
    c = jbpf_map_lookup_elem(&p1_counter, &index);
    if (!c)
        return 1;
    counter = *(uint64_t*)c;
    counter = (counter + 1) % 1024;

    /* build a dummy example req_resp */
    response* out4;
    c = jbpf_map_lookup_elem(&map4_tmp, &index);
    if (!c)
        return 1;
    out4 = (response*)c;
    out4->id = 1;
    strncpy(out4->msg, dummy_str, 5);

    // send example req_resp
    if (jbpf_ringbuf_output(&map4, out4, sizeof(req_resp)) < 0) {
        return 1;
    }

    /* build a dummy example status */
    status* out5;
    c = jbpf_map_lookup_elem(&map5_tmp, &index);
    if (!c)
        return 1;
    out5 = (status*)c;
    out5->id = 33;
    strncpy(out5->status, dummy_str, 3);
    out5->a_struct.a_num = 44;
    out5->a_struct.has_another_num = true;
    out5->a_struct.another_num = 45;

    // send example status
    if (jbpf_ringbuf_output(&map5, out5, sizeof(status)) < 0) {
        return 1;
    }

    /* inc counter in p2_counter map */
    c = jbpf_map_lookup_elem(&p2_counter, &index);
    if (!c)
        return 1;
    counter = *(uint64_t*)c;
    counter = (counter + 1) % 1024;

    /* inc counter in p2_counter map */
    c = jbpf_map_lookup_elem(&p3_counter, &index);
    if (!c)
        return 1;
    counter = *(uint64_t*)c;
    counter = (counter + 1) % 1024;

    /* inc counter in p2_counter map */
    c = jbpf_map_lookup_elem(&p4_counter, &index);
    if (!c)
        return 1;
    counter = *(uint64_t*)c;
    counter = (counter + 1) % 1024;

    return 0;
}
