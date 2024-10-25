// Copyright (c) Microsoft Corporation. All rights reserved.

#include <string.h>

#include "jbpf_defs.h"
#include "jbpf_test_def.h"
#include "jbpf_helper.h"

jbpf_output_map(map1, req_resp, 1000);

struct jbpf_load_map_def SEC("maps") map1_tmp = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(req_resp),
    .max_entries = 1,
};

jbpf_output_map(map2, req_resp, 1000);

struct jbpf_load_map_def SEC("maps") map2_tmp = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(status),
    .max_entries = 1,
};

jbpf_output_map(map3, item, 1000);

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

jbpf_output_map(map4, req_resp, 1000);

struct jbpf_load_map_def SEC("maps") map4_tmp = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(req_resp),
    .max_entries = 1,
};

jbpf_output_map(map5, status, 1000);

struct jbpf_load_map_def SEC("maps") map5_tmp = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(status),
    .max_entries = 1,
};

jbpf_output_map(map6, item, 1000);

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

jbpf_output_map(map7, req_resp, 1000);

struct jbpf_load_map_def SEC("maps") map7_tmp = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(req_resp),
    .max_entries = 1,
};

jbpf_output_map(map8, status, 1000);

struct jbpf_load_map_def SEC("maps") map8_tmp = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(status),
    .max_entries = 1,
};

jbpf_output_map(map9, item, 1000);

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

jbpf_output_map(map10, req_resp, 1000);

struct jbpf_load_map_def SEC("maps") map10_tmp = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(req_resp),
    .max_entries = 1,
};

jbpf_output_map(map11, status, 1000);

struct jbpf_load_map_def SEC("maps") map11_tmp = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(status),
    .max_entries = 1,
};

jbpf_output_map(map12, item, 1000);

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
    response* out1a;
    out1a = jbpf_get_output_buf(&map1);
    if (!out1a) {
        return 1;
    }
    out1a->id = out->id;
    memcpy(out1a->msg, out->msg, sizeof(out->msg));
    if (jbpf_send_output(&map1) < 0) {
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
    status* out2a;
    out2a = jbpf_get_output_buf(&map2);
    if (!out2a) {
        return 1;
    }
    out2a->id = out2->id;
    memcpy(out2a->status, out2->status, sizeof(out2->status));
    out2a->a_struct.a_num = out2->a_struct.a_num;
    // out2a->a_struct.has_another_num = out2->a_struct.has_another_num;
    // out2a->a_struct.another_num = out2->a_struct.another_num;
    if (jbpf_send_output(&map2) < 0) {
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
    item* out3a;
    out3a = jbpf_get_output_buf(&map3);
    if (!out3a) {
        return 1;
    }
    out3a->has_val = out3->has_val;
    out3a->val = out3->val;
    memcpy(out3a->name, out3->name, sizeof(out3->name));
    if (jbpf_send_output(&map3) < 0) {
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
    response* out4a;
    out4a = jbpf_get_output_buf(&map4);
    if (!out4a) {
        return 1;
    }
    out4a->id = out4->id;
    memcpy(out4a->msg, out4->msg, sizeof(out4->msg));
    if (jbpf_send_output(&map4) < 0) {
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
    status* out5a;
    out5a = jbpf_get_output_buf(&map5);
    if (!out5a) {
        return 1;
    }
    out5a->id = out5->id;
    memcpy(out5a->status, out5->status, sizeof(out5->status));
    out5a->a_struct.a_num = out5->a_struct.a_num;
    out5a->a_struct.has_another_num = out5->a_struct.has_another_num;
    out5a->a_struct.another_num = out5->a_struct.another_num;
    if (jbpf_send_output(&map5) < 0) {
        return 1;
    }

    /* build a dummy example2 item */
    item* out6;
    c = jbpf_map_lookup_elem(&map6_tmp, &index);
    if (!c)
        return 1;
    out6 = (item*)c;
    strncpy(out6->name, dummy_str, 3);
    out6->has_val = true;
    out6->val = 222;

    // send example2 item
    item* out6a;
    out6a = jbpf_get_output_buf(&map6);
    if (!out6a) {
        return 1;
    }
    out6a->has_val = out6->has_val;
    out6a->val = out6->val;
    memcpy(out6a->name, out6->name, sizeof(out6->name));
    if (jbpf_send_output(&map6) < 0) {
        return 1;
    }

    /* inc counter in p2_counter map */
    c = jbpf_map_lookup_elem(&p2_counter, &index);
    if (!c)
        return 1;
    counter = *(uint64_t*)c;
    counter = (counter + 1) % 1024;

    response* out7;
    c = jbpf_map_lookup_elem(&map7_tmp, &index);
    if (!c)
        return 1;
    out7 = (response*)c;
    out7->id = 1;
    strncpy(out7->msg, dummy_str, 5);

    // send example req_resp
    response* out7a;
    out7a = jbpf_get_output_buf(&map7);
    if (!out7a) {
        return 1;
    }
    out7a->id = out7->id;
    memcpy(out7a->msg, out7->msg, sizeof(out7->msg));
    if (jbpf_send_output(&map7) < 0) {
        return 1;
    }

    /* build a dummy example status */
    status* out8;
    c = jbpf_map_lookup_elem(&map8_tmp, &index);
    if (!c)
        return 1;
    out8 = (status*)c;
    out8->id = 33;
    strncpy(out8->status, dummy_str, 3);
    out8->a_struct.a_num = 44;
    out8->a_struct.has_another_num = true;
    out8->a_struct.another_num = 45;

    // send example status
    status* out8a;
    out8a = jbpf_get_output_buf(&map8);
    if (!out8a) {
        return 1;
    }
    out8a->id = out8->id;
    memcpy(out8a->status, out8->status, sizeof(out8->status));
    out8a->a_struct.a_num = out8->a_struct.a_num;
    out8a->a_struct.has_another_num = out8->a_struct.has_another_num;
    out8a->a_struct.another_num = out8->a_struct.another_num;
    if (jbpf_send_output(&map8) < 0) {
        return 1;
    }

    /* build a dummy example2 item */
    item* out9;
    c = jbpf_map_lookup_elem(&map9_tmp, &index);
    if (!c)
        return 1;
    out9 = (item*)c;
    strncpy(out9->name, dummy_str, 3);
    out9->has_val = true;
    out9->val = 222;

    // send example2 item
    item* out9a;
    out9a = jbpf_get_output_buf(&map9);
    if (!out9a) {
        return 1;
    }
    out9a->has_val = out9->has_val;
    out9a->val = out9->val;
    memcpy(out9a->name, out9->name, sizeof(out9->name));
    if (jbpf_send_output(&map9) < 0) {
        return 1;
    }

    /* inc counter in p2_counter map */
    c = jbpf_map_lookup_elem(&p3_counter, &index);
    if (!c)
        return 1;
    counter = *(uint64_t*)c;
    counter = (counter + 1) % 1024;

    response* out10;
    c = jbpf_map_lookup_elem(&map10_tmp, &index);
    if (!c)
        return 1;
    out10 = (response*)c;
    out10->id = 1;
    strncpy(out10->msg, dummy_str, 5);

    // send example req_resp
    response* out10a;
    out10a = jbpf_get_output_buf(&map10);
    if (!out10a) {
        return 1;
    }
    out10a->id = out10->id;
    memcpy(out10a->msg, out10->msg, sizeof(out10->msg));
    if (jbpf_send_output(&map10) < 0) {
        return 1;
    }

    /* build a dummy example status */
    status* out11;
    c = jbpf_map_lookup_elem(&map11_tmp, &index);
    if (!c)
        return 1;
    out11 = (status*)c;
    out11->id = 33;
    strncpy(out11->status, dummy_str, 3);
    out11->a_struct.a_num = 44;
    out11->a_struct.has_another_num = true;
    out11->a_struct.another_num = 45;

    // send example status
    status* out11a;
    out11a = jbpf_get_output_buf(&map11);
    if (!out11a) {
        return 1;
    }
    out11a->id = out11->id;
    memcpy(out11a->status, out11->status, sizeof(out11->status));
    out11a->a_struct.a_num = out11->a_struct.a_num;
    out11a->a_struct.has_another_num = out11->a_struct.has_another_num;
    out11a->a_struct.another_num = out11->a_struct.another_num;
    if (jbpf_send_output(&map11) < 0) {
        return 1;
    }

    /* build a dummy example2 item */
    item* out12;
    c = jbpf_map_lookup_elem(&map12_tmp, &index);
    if (!c)
        return 1;
    out12 = (item*)c;
    strncpy(out12->name, dummy_str, 3);
    out12->has_val = true;
    out12->val = 222;

    // send example2 item
    item* out12a;
    out12a = jbpf_get_output_buf(&map12);
    if (!out12a) {
        return 1;
    }
    out12a->has_val = out12->has_val;
    out12a->val = out12->val;
    memcpy(out12a->name, out12->name, sizeof(out12->name));
    if (jbpf_send_output(&map12) < 0) {
        return 1;
    }

    /* inc counter in p2_counter map */
    c = jbpf_map_lookup_elem(&p4_counter, &index);
    if (!c)
        return 1;
    counter = *(uint64_t*)c;
    counter = (counter + 1) % 1024;

    return 0;
}
