// Copyright (c) Microsoft Corporation. All rights reserved.
/*
    This is an example codelet to demonstrate the usage of atomic operations in jbpf.
    This test creates 8 threads, each thread calls the hook test0 10000 times.
    The hook will call the following atomic operations:
    1. __sync_fetch_and_add
    2. __sync_fetch_and_or
    3. __sync_fetch_and_and
    4. __sync_fetch_and_xor
    5. __sync_lock_test_and_set
    6. __sync_val_compare_and_swap
    The test checks that the values in the map are as expected after the threads finish.
    All threads are calling the atomic operations on the same memory locations (counters).
*/

#include <assert.h>
#include <pthread.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "jbpf_utils.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

#define HOOK_CALLED_TIME (10000)
#define NUMBER_OF_WORKERS (8)

jbpf_io_stream_id_t stream_id_c1 = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

// parameters passing to the threads
struct thread_params
{
    struct counter_vals data;
};

pthread_barrier_t barrier;

#define DEFINE_THREAD_WORKER_FUNC(NAME, ITER, HOOK_NAME, DATA, CTX)           \
    void* NAME(void* ctx)                                                     \
    {                                                                         \
        struct thread_params* p = (struct thread_params*)ctx;                 \
        jbpf_register_thread();                                               \
        pthread_barrier_wait(&barrier);                                       \
        for (int i = 0; i < ITER; ++i) {                                      \
            HOOK_NAME(&p->DATA, CTX);                                         \
        }                                                                     \
        printf("Now will cleanup and teardown thread %ld\n", pthread_self()); \
        jbpf_cleanup_thread();                                                \
        printf("Done with thread %ld\n", pthread_self());                     \
        pthread_exit(0);                                                      \
    }

DEFINE_THREAD_WORKER_FUNC(worker_func, HOOK_CALLED_TIME, hook_test0, data, 1);

int
main(int argc, char** argv)
{
    pthread_t threads[NUMBER_OF_WORKERS];
    struct jbpf_codeletset_load_req codeletset_req_c1 = {0};
    struct jbpf_codeletset_unload_req codeletset_unload_req_c1 = {0};
    const char* jbpf_path = getenv("JBPF_PATH");
    struct jbpf_config config = {0};

    JBPF_UNUSED(codeletset_unload_req_c1);

    jbpf_set_default_config_options(&config);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    assert(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // The name of the codeletset
    strcpy(codeletset_req_c1.codeletset_id.name, "atomic_concurrency_test");

    // Create max possible codelets
    codeletset_req_c1.num_codelet_descriptors = 1;

    // The codelet has just one output channel and no shared maps
    codeletset_req_c1.codelet_descriptor[0].num_in_io_channel = 0;

    // Link the map to a stream id
    memcpy(&codeletset_req_c1.codelet_descriptor[0].out_io_channel[0].stream_id, &stream_id_c1, JBPF_STREAM_ID_LEN);
    // The output channel of the codelet does not have a serializer
    codeletset_req_c1.codelet_descriptor[0].out_io_channel[0].has_serde = false;
    codeletset_req_c1.codelet_descriptor[0].num_linked_maps = 0;

    // The path of the codelet
    assert(jbpf_path != NULL);
    snprintf(
        codeletset_req_c1.codelet_descriptor[0].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/atomics-example/atomics-example.o",
        jbpf_path);
    strcpy(codeletset_req_c1.codelet_descriptor[0].codelet_name, "codelet-atomics");

    snprintf(codeletset_req_c1.codelet_descriptor[0].hook_name, JBPF_HOOK_NAME_LEN, "test0");

    // Load the codeletset
    assert(jbpf_codeletset_load(&codeletset_req_c1, NULL) == JBPF_CODELET_LOAD_SUCCESS);

    struct thread_params data;
    memset(&data, 0, sizeof(data));

    pthread_barrier_init(&barrier, NULL, NUMBER_OF_WORKERS);

    // spawn the threads, each thread will call the hook and then increment the map entry by 1
    for (int i = 0; i < NUMBER_OF_WORKERS; ++i) {
        pthread_create(&threads[i], NULL, worker_func, &data.data);
    }

    // wait for the threads to finish
    for (int i = 0; i < NUMBER_OF_WORKERS; ++i) {
        pthread_join(threads[i], NULL);
    }

    pthread_barrier_destroy(&barrier);

    // now assert that the value in the map is equal to the number of times the hook was called
    for (int i = 0; i < NUMBER_OF_WORKERS; ++i) {
        // 8 threads, each thread calls __sync_fetch_and_add(&cvals->u64_counter, 1) 1000x2 times
        // hook is called 10000 times
        // therefore the value should be 8*1000*2*10000 = 16000
        assert(data.data.u64_counter == 160000000);
        assert(data.data.u64_or_flag == -1);
        assert(data.data.u64_and_flag == 0);
        assert(data.data.u64_xor_flag == 0);
        assert(data.data.u64_swap_val == 160000000);
        assert(data.data.u64_cmp_swap_val == 0);
        assert(data.data.u64_changed_cmp_swap_val == 160000000);
    }

    // Unload the codeletsets
    codeletset_unload_req_c1.codeletset_id = codeletset_req_c1.codeletset_id;
    assert(jbpf_codeletset_unload(&codeletset_unload_req_c1, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");
    return 0;
}
