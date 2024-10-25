#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct test_struct
{
    uint32_t counter_a;
    uint32_t counter_b;
};

// Function to find a key in the JSON string and return its integer value
int
get_json_value(const char* json, const char* key)
{
    const char* pos = strstr(json, key);
    if (pos != NULL) {
        pos = strchr(pos, ':');
        if (pos != NULL) {
            return atoi(pos + 1);
        }
    }
    return -1;
}

// Function to parse JSON and populate the struct
int
parse_json_to_struct(const char* json_string, struct test_struct* output_struct)
{
    output_struct->counter_a = get_json_value(json_string, "counter_a");
    output_struct->counter_b = get_json_value(json_string, "counter_b");

    return (output_struct->counter_a != -1 && output_struct->counter_b != -1) ? 0 : -1;
}

int
jbpf_io_serialize(
    void* input_msg_buf, size_t input_msg_buf_size, char* serialized_data_buf, size_t serialized_data_buf_size)
{

    int res;
    char* str = "{ \"counter_a\": %u, \"counter_b\": %u }";

    struct test_struct* s = input_msg_buf;
    res = snprintf(serialized_data_buf, serialized_data_buf_size, str, s->counter_a, s->counter_b);

    return res;
}

int
jbpf_io_deserialize(
    char* serialized_data_buf, size_t serialized_data_buf_size, void* output_msg_buf, size_t output_msg_buf_size)
{

    struct test_struct* output_struct = output_msg_buf;

    if (parse_json_to_struct(serialized_data_buf, output_struct) == 0) {
        return 1;
    } else {
        return 0;
    }
}
