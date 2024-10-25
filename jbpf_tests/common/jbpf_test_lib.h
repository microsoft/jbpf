#ifndef JBPF_TEST_LIB_H
#define JBPF_TEST_LIB_H

/* Function prototype for test functions. */
typedef void (*jbpf_test_function_t)(void** state);

/* Function prototype for setup functions. */
typedef int (*jbpf_setup_function_t)(void** state);

/* Function prototype for teardown functions. */
typedef int (*jbpf_teardown_function_t)(void** state);

typedef struct jbpf_test_t
{
    char name[256];
    jbpf_test_function_t test_func;
    jbpf_setup_function_t setup_func;
    jbpf_teardown_function_t teardown_func;
    void* initial_state;
} jbpf_test;

jbpf_test
__jbpf_create_test(
    jbpf_test_function_t test_func,
    jbpf_setup_function_t setup_func,
    jbpf_teardown_function_t teardown_func,
    void* initial_state,
    char* name);

int
jbpf_run_test(
    const jbpf_test* tests,
    int num_tests,
    jbpf_setup_function_t group_setup_func,
    jbpf_teardown_function_t group_teardown_func);

#define JBPF_CREATE_TEST(test_func, setup_func, teardown_func, initial_state) \
    __jbpf_create_test(test_func, setup_func, teardown_func, initial_state, #test_func)

#endif // JBPF_TEST_LIB_H