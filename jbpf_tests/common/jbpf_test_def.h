#ifndef JBPF_TEST_DEF
#define JBPF_TEST_DEF

#include <stdint.h>
#include <stdbool.h>

/* Test */
struct packet
{
    int counter_a;
    int counter_b;
};

struct test_packet
{
    int test_passed;
    float test_passed_32;
    double test_passed_64;
};

struct packet4
{
    int counter_a;
    int counter_b;
    int counter_c;
    int counter_d;
};

typedef struct _custom_api
{
    uint32_t command;
} custom_api;

typedef struct _custom_api2
{
    uint32_t parameter1;
    uint32_t parameter2;
} custom_api2;

typedef struct _item
{
    char name[30];
    bool has_val;
    uint32_t val;
} item;

/* Enum definitions */
typedef enum _my_state
{
    my_state_GOOD = 0,
    my_state_BAD = 1
} my_state;

/* Struct definitions */
typedef struct _my_struct
{
    uint32_t a_num;
    bool has_another_num;
    uint32_t another_num;
} my_struct;

typedef struct _request
{
    uint32_t id;
    char name[32];
    bool has_state;
    my_state state;
} request;

typedef struct _response
{
    uint32_t id;
    char msg[100];
} response;

typedef struct _req_resp
{
    int which_req_or_resp;
    union
    {
        request req;
        response resp;
    } req_or_resp;
} req_resp;

typedef struct _status
{
    uint32_t id;
    char status[100];
    my_struct a_struct;
} status;

struct counter_vals
{

    uint32_t u32_counter;
    uint64_t u64_counter;

    uint64_t u64_non_atomic_counter;

    uint32_t u32_or_flag;
    uint64_t u64_or_flag;

    uint32_t u32_and_flag;
    uint64_t u64_and_flag;

    uint32_t u32_xor_flag;
    uint64_t u64_xor_flag;

    uint32_t u32_swap_val;
    uint64_t u64_swap_val;

    uint32_t u32_cmp_swap_val;
    uint64_t u64_cmp_swap_val;

    uint32_t u32_changed_cmp_swap_val;
    uint64_t u64_changed_cmp_swap_val;

    uint64_t result;

    bool benchmark_init;
};

#endif
