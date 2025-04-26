// Copyright (c) Microsoft Corporation. All rights reserved.
/*
    This test is used to test the hash map concurrent read.
    codelet is called once to increment the map entry by 1
    then we spawn multiple threads to call the codelet in parallel
    each thread will call the codelet HOOK_CALLED_TIME times
    so the value will be equal to the HOOK_CALLED_TIME for each thread
*/

#include <assert.h>
#include <pthread.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "jbpf_utils.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"
#include "jbpf_test_lib.h"

#define HOOK_CALLED_TIME (100)
#define NUMBER_OF_WORKERS (20)

jbpf_io_stream_id_t stream_id_c1 = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

// parameters passing to the threads
struct thread_params
{
    struct packet data;
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

DEFINE_THREAD_WORKER_FUNC(worker_func, HOOK_CALLED_TIME, hook_test1, data, 1);

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

    __assert__(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // The name of the codeletset
    strcpy(codeletset_req_c1.codeletset_id.name, "hashmap_codelet_concurrency_test");

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
    __assert__(jbpf_path != NULL);
    snprintf(
        codeletset_req_c1.codelet_descriptor[0].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/codelet-hashmap/codelet-hashmap.o",
        jbpf_path);
    strcpy(codeletset_req_c1.codelet_descriptor[0].codelet_name, "codelet-hashmap");

    snprintf(codeletset_req_c1.codelet_descriptor[0].hook_name, JBPF_HOOK_NAME_LEN, "test1");

    // Load the codeletset
    __assert__(jbpf_codeletset_load(&codeletset_req_c1, NULL) == JBPF_CODELET_LOAD_SUCCESS);

    struct thread_params* data[NUMBER_OF_WORKERS];
    for (int i = 0; i < NUMBER_OF_WORKERS; ++i) {
        data[i] = (struct thread_params*)malloc(sizeof(struct thread_params));
        assert(data[i] != NULL);
        data[i]->data.counter_a = 1;
        data[i]->data.counter_b = 0;
    }

    // The hashmap allows multiple readers and a single writer to concurrently use the map
    // therefore, we call the hook manually once to increment the map entry by 1
    struct thread_params* data0 = (struct thread_params*)malloc(sizeof(struct thread_params));
    assert(data0 != NULL);
    data0->data.counter_a = -1;
    hook_test1(&data0->data, 1);

    printf("we spawn the threads to access the hashmap in parallel..\n");

    pthread_barrier_init(&barrier, NULL, NUMBER_OF_WORKERS);

    // spawn the threads, each thread will call the hook and then increment the map entry by 1
    for (int i = 0; i < NUMBER_OF_WORKERS; ++i) {
        pthread_create(&threads[i], NULL, worker_func, data[i]);
    }

    // wait for the threads to finish
    for (int i = 0; i < NUMBER_OF_WORKERS; ++i) {
        pthread_join(threads[i], NULL);
    }

    pthread_barrier_destroy(&barrier);

    // now assert that the value in the map is equal to the number of times the hook was called
    for (int i = 0; i < NUMBER_OF_WORKERS; ++i) {
        assert(data[i]->data.counter_b == HOOK_CALLED_TIME);
    }

    // Unload the codeletsets
    codeletset_unload_req_c1.codeletset_id = codeletset_req_c1.codeletset_id;
    __assert__(jbpf_codeletset_unload(&codeletset_unload_req_c1, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");
    for (int i = 0; i < NUMBER_OF_WORKERS; ++i) {
        free(data[i]);
    }
    free(data0);
    return 0;
}
