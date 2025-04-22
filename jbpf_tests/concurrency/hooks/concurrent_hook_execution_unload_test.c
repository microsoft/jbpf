// Copyright (c) Microsoft Corporation. All rights reserved.
/*
    This test creates 5 threads to load 5 codelets into 5 hooks.
    Then it creates another 5 threads to call the hooks.
    Then in the middle of the execution, it creates another 5 threads to unload the codelets.
    The test is successful if the codelets are loaded, hooks are called and codelets are unloaded successfully.
    The test repeats this process for 1000 times.
*/

#include <assert.h>
#include <pthread.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

#define HOOK_CALLED_TIME (100000)
#define NUMBER_OF_WORKERS (5)
#define ITERATIONS (1000)

jbpf_io_stream_id_t stream_id_c1 = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};
jbpf_io_stream_id_t stream_id_c2 = {
    .id = {0x01, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};
jbpf_io_stream_id_t stream_id_c3 = {
    .id = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};
jbpf_io_stream_id_t stream_id_c4 = {
    .id = {0x03, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};
jbpf_io_stream_id_t stream_id_c5 = {
    .id = {0x04, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

// parameters passing to the threads
struct thread_params
{
    struct packet data;
};

pthread_barrier_t barrier_call_hook;
pthread_barrier_t barrier_load;
pthread_barrier_t barrier_unload;

#define DEFINE_THREAD_WORKER_FUNC(NAME, ITER, HOOK_NAME, DATA, CTX)           \
    void* NAME(void* ctx)                                                     \
    {                                                                         \
        struct thread_params* p = (struct thread_params*)ctx;                 \
        jbpf_register_thread();                                               \
        pthread_barrier_wait(&barrier_call_hook);                             \
        for (int i = 0; i < ITER; ++i) {                                      \
            HOOK_NAME(&p->DATA, CTX);                                         \
        }                                                                     \
        printf("Now will cleanup and teardown thread %ld\n", pthread_self()); \
        jbpf_cleanup_thread();                                                \
        printf("Done with thread %ld\n", pthread_self());                     \
        pthread_exit(0);                                                      \
    }

DEFINE_THREAD_WORKER_FUNC(worker_func1, HOOK_CALLED_TIME, hook_test1, data, 1);
DEFINE_THREAD_WORKER_FUNC(worker_func2, HOOK_CALLED_TIME, hook_test2, data, 1);
DEFINE_THREAD_WORKER_FUNC(worker_func3, HOOK_CALLED_TIME, hook_test4, data, 1);
DEFINE_THREAD_WORKER_FUNC(worker_func4, HOOK_CALLED_TIME, hook_test5, data, 1);
DEFINE_THREAD_WORKER_FUNC(worker_func5, HOOK_CALLED_TIME, hook_test6, data, 1);

void
load_codeletset(jbpf_io_stream_id_t stream_id, const char* codelet_set_name, const char* hook_name)
{
    struct jbpf_codeletset_load_req codeletset_req_c1 = {0};

    // The name of the codeletset
    strcpy(codeletset_req_c1.codeletset_id.name, codelet_set_name);

    // Create max possible codelets
    codeletset_req_c1.num_codelet_descriptors = 1;

    // The codelet has just one output channel and no shared maps
    codeletset_req_c1.codelet_descriptor[0].num_in_io_channel = 0;
    codeletset_req_c1.codelet_descriptor[0].num_out_io_channel = 1;

    // The name of the output map that corresponds to the output channel of the codelet
    strcpy(codeletset_req_c1.codelet_descriptor[0].out_io_channel[0].name, "map1");

    // Link the map to a stream id
    memcpy(&codeletset_req_c1.codelet_descriptor[0].out_io_channel[0].stream_id, &stream_id, JBPF_STREAM_ID_LEN);

    // The output channel of the codelet does not have a serializer
    codeletset_req_c1.codelet_descriptor[0].out_io_channel[0].has_serde = false;
    codeletset_req_c1.codelet_descriptor[0].num_linked_maps = 0;

    // The path of the codelet
    const char* jbpf_path = getenv("JBPF_PATH");
    assert(jbpf_path != NULL);
    snprintf(
        codeletset_req_c1.codelet_descriptor[0].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/codelet-ringbuf-multithreading/codelet-ringbuf-multithreading.o",
        jbpf_path);
    strcpy(codeletset_req_c1.codelet_descriptor[0].codelet_name, "codelet-ringbuf");

    strcpy(codeletset_req_c1.codelet_descriptor[0].hook_name, hook_name);

    // Load the codeletset
    res = jbpf_codeletset_load(&codeletset_req_c1, NULL);
    assert(res == JBPF_CODELET_LOAD_SUCCESS);
}

#define DEFINE_THREAD_LOAD_FUNC(NAME, STREAM_ID, CODDELETSET_NAME, HOOK_NAME) \
    void* NAME()                                                              \
    {                                                                         \
        pthread_barrier_wait(&barrier_load);                                  \
        jbpf_register_thread();                                               \
        printf("Loading codelet %ld\n", pthread_self());                      \
        load_codeletset(STREAM_ID, CODDELETSET_NAME, HOOK_NAME);              \
        printf("Done loading with thread %ld\n", pthread_self());             \
        jbpf_cleanup_thread();                                                \
        pthread_exit(0);                                                      \
    }

DEFINE_THREAD_LOAD_FUNC(loader_func1, stream_id_c1, "codeletset1", "test1");
DEFINE_THREAD_LOAD_FUNC(loader_func2, stream_id_c2, "codeletset2", "test2");
DEFINE_THREAD_LOAD_FUNC(loader_func3, stream_id_c3, "codeletset3", "test4");
DEFINE_THREAD_LOAD_FUNC(loader_func4, stream_id_c4, "codeletset4", "test5");
DEFINE_THREAD_LOAD_FUNC(loader_func5, stream_id_c5, "codeletset5", "test6");

void
unload_func(jbpf_codeletset_id_t codeletset_id)
{
    struct jbpf_codeletset_unload_req codeletset_unload_req_c1 = {0};

    codeletset_unload_req_c1.codeletset_id = codeletset_id;
    assert(jbpf_codeletset_unload(&codeletset_unload_req_c1, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);
}

#define DEFINE_THREAD_UNLOAD_FUNC(NAME, CODELET_SET_ID)             \
    void* NAME()                                                    \
    {                                                               \
        pthread_barrier_wait(&barrier_unload);                      \
        printf("UnLoading codelet %ld\n", pthread_self());          \
        jbpf_register_thread();                                     \
        jbpf_codeletset_id_t codeletset_id = {0};                   \
        strcpy(codeletset_id.name, CODELET_SET_ID);                 \
        unload_func(codeletset_id);                                 \
        printf("Done unloading with thread %ld\n", pthread_self()); \
        jbpf_cleanup_thread();                                      \
        pthread_exit(0);                                            \
    }

DEFINE_THREAD_UNLOAD_FUNC(unloader_func1, "codeletset1");
DEFINE_THREAD_UNLOAD_FUNC(unloader_func2, "codeletset2");
DEFINE_THREAD_UNLOAD_FUNC(unloader_func3, "codeletset3");
DEFINE_THREAD_UNLOAD_FUNC(unloader_func4, "codeletset4");
DEFINE_THREAD_UNLOAD_FUNC(unloader_func5, "codeletset5");

int
main(int argc, char** argv)
{
    pthread_t threads[NUMBER_OF_WORKERS];
    struct thread_params* data[NUMBER_OF_WORKERS];
    pthread_t threads_unload[NUMBER_OF_WORKERS];
    pthread_t threads_load[NUMBER_OF_WORKERS];

    // initialize the jbpf
    struct jbpf_config config = {0};
    jbpf_set_default_config_options(&config);
    config.lcm_ipc_config.has_lcm_ipc_thread = false;
    int res = jbpf_init(&config);
    assert(res == 0);
#ifdef NDEBUG
    (void)res; // suppress unused-variable warning in release mode
#endif
    jbpf_register_thread();

    for (int iter = 0; iter < ITERATIONS; ++iter) {
        // loading 5 codelets
        pthread_barrier_init(&barrier_load, NULL, NUMBER_OF_WORKERS);
        pthread_create(&threads_load[0], NULL, loader_func1, NULL);
        pthread_create(&threads_load[1], NULL, loader_func2, NULL);
        pthread_create(&threads_load[2], NULL, loader_func3, NULL);
        pthread_create(&threads_load[3], NULL, loader_func4, NULL);
        pthread_create(&threads_load[4], NULL, loader_func5, NULL);

        // wait for all codelets to be loaded
        for (int i = 0; i < NUMBER_OF_WORKERS; ++i) {
            pthread_join(threads_load[i], NULL);
        }

        // then call the hooks
        pthread_barrier_init(&barrier_call_hook, NULL, NUMBER_OF_WORKERS);
        for (int i = 0; i < NUMBER_OF_WORKERS; ++i) {
            data[i] = (struct thread_params*)malloc(sizeof(struct thread_params));
            data[i]->data.counter_a = 0;
            data[i]->data.counter_b = 0;
        }
        pthread_create(&threads[0], NULL, worker_func1, data[0]);
        pthread_create(&threads[1], NULL, worker_func2, data[1]);
        pthread_create(&threads[2], NULL, worker_func3, data[2]);
        pthread_create(&threads[3], NULL, worker_func4, data[3]);
        pthread_create(&threads[4], NULL, worker_func5, data[4]);

        sleep(0.01);

        // during the exeuction, unload the codelets
        pthread_barrier_init(&barrier_unload, NULL, NUMBER_OF_WORKERS);
        pthread_create(&threads_unload[0], NULL, unloader_func1, NULL);
        pthread_create(&threads_unload[1], NULL, unloader_func2, NULL);
        pthread_create(&threads_unload[2], NULL, unloader_func3, NULL);
        pthread_create(&threads_unload[3], NULL, unloader_func4, NULL);
        pthread_create(&threads_unload[4], NULL, unloader_func5, NULL);

        for (int i = 0; i < NUMBER_OF_WORKERS; ++i) {
            pthread_join(threads_unload[i], NULL);
        }

        for (int i = 0; i < NUMBER_OF_WORKERS; ++i) {
            pthread_join(threads[i], NULL);
        }

        pthread_barrier_destroy(&barrier_load);
        pthread_barrier_destroy(&barrier_call_hook);
        pthread_barrier_destroy(&barrier_unload);

        for (int i = 0; i < NUMBER_OF_WORKERS; ++i) {
            printf("data[%d] counter_a = %d, counter_b = %d\n", i, data[i]->data.counter_a, data[i]->data.counter_b);
        }

        for (int i = 0; i < NUMBER_OF_WORKERS; ++i) {
            free(data[i]);
        }
    }

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");
    return 0;
}
