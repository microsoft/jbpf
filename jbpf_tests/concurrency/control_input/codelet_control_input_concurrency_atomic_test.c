/*
 * This test does the following:
 * - Load a codeletset with one codelet that has one input map and no output map
 * - Create 8 threads that send 1000 control inputs each
 * - Create 4 threads that call a hook 10000 times
 * - In the hook, the control input is read and the counter is incremented (atomic operation)
 * - The hook is called more times than the control input is sent
 * - The test checks if the counter is incremented by the number of control inputs
 */

#include <assert.h>
#include <semaphore.h>
#include <pthread.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

#define CONTROL_CALLED_COUNT 1000
#define HOOK_CALLED_COUNT 10000
#define NUM_THREADS_CONTROL 8
#define NUM_THREAD_HOOK 4

// barrier
pthread_barrier_t barrier_hook;
pthread_barrier_t barrier_control;

sem_t sem;

jbpf_io_stream_id_t stream_id_c1 = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

struct thread_params
{
    jbpf_io_stream_id_t* stream_id;
    int thread_id;
};

void*
control_thread_func(void* arg)
{
    pthread_barrier_wait(&barrier_control);
    struct thread_params* params = (struct thread_params*)arg;
    jbpf_io_stream_id_t* stream_id = params->stream_id;
    jbpf_register_thread();
    for (int i = 0; i < CONTROL_CALLED_COUNT; i++) {
        custom_api control_input = {.command = 1};
        jbpf_send_input_msg(stream_id, &control_input, sizeof(control_input));
    }
    jbpf_cleanup_thread();
    return NULL;
}

void*
hook_thread_func(void* arg)
{
    pthread_barrier_wait(&barrier_hook);
    jbpf_register_thread();
    for (int i = 0; i < HOOK_CALLED_COUNT; i++) {
        hook_test1(arg, 1);
    }
    jbpf_cleanup_thread();
    return NULL;
}

int
main(int argc, char** argv)
{
    struct jbpf_codeletset_load_req codeletset_req = {0};
    struct jbpf_codeletset_unload_req codeletset_unload_req = {0};
    const char* jbpf_path = getenv("JBPF_PATH");
    struct jbpf_config config = {0};

    jbpf_set_default_config_options(&config);
    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    strcpy(codeletset_req.codeletset_id.name, "simple_input_output_codeletset");
    codeletset_req.num_codelet_descriptors = 1;

    // Codelet 1 (C1) has one input map and no output map
    codeletset_req.codelet_descriptor[0].num_out_io_channel = 0;
    codeletset_req.codelet_descriptor[0].num_linked_maps = 0;
    codeletset_req.codelet_descriptor[0].num_in_io_channel = 1;
    // The name of the input map is "input_map"
    strcpy(codeletset_req.codelet_descriptor[0].in_io_channel[0].name, "input_map");
    // Link the map to a stream id
    memcpy(&codeletset_req.codelet_descriptor[0].in_io_channel[0].stream_id, &stream_id_c1, JBPF_STREAM_ID_LEN);
    // The input channel of the codelet does not have a serializer
    codeletset_req.codelet_descriptor[0].in_io_channel[0].has_serde = false;
    // The input channel of the codelet does not have a serializer
    codeletset_req.codelet_descriptor[0].out_io_channel[0].has_serde = false;

    assert(jbpf_path != NULL);
    snprintf(
        codeletset_req.codelet_descriptor[0].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/simple_input_output_atomic/simple_input_output_atomic.o",
        jbpf_path);
    strcpy(codeletset_req.codelet_descriptor[0].codelet_name, "simple_input_output_atomic");
    strcpy(codeletset_req.codelet_descriptor[0].hook_name, "test1");

    assert(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Load the codeletset
    assert(jbpf_codeletset_load(&codeletset_req, NULL) == JBPF_CODELET_LOAD_SUCCESS);

    pthread_barrier_init(&barrier_control, NULL, NUM_THREADS_CONTROL);
    pthread_t control_threads[NUM_THREADS_CONTROL];
    struct thread_params** params = malloc(NUM_THREADS_CONTROL * sizeof(struct thread_params*));
    for (int i = 0; i < NUM_THREADS_CONTROL; i++) {
        // testing jbpf_io_channel_send_msg as it is thread safe
        params[i] = malloc(sizeof(struct thread_params));
        params[i]->stream_id = &stream_id_c1;
        params[i]->thread_id = i;
        int ret = pthread_create(&control_threads[i], NULL, control_thread_func, params[i]);
        assert(ret == 0);
    }

    // join the control threads
    for (int i = 0; i < NUM_THREADS_CONTROL; i++) {
        int ret = pthread_join(control_threads[i], NULL);
        assert(ret == 0);
    }

    struct packet data = {0};

    pthread_barrier_init(&barrier_hook, NULL, NUM_THREAD_HOOK);
    pthread_t hook_threads[NUM_THREAD_HOOK];
    for (int i = 0; i < NUM_THREAD_HOOK; i++) {
        int ret = pthread_create(&hook_threads[i], NULL, hook_thread_func, &data);
        assert(ret == 0);
    }

    // join the hook threads
    for (int i = 0; i < NUM_THREAD_HOOK; i++) {
        int ret = pthread_join(hook_threads[i], NULL);
        assert(ret == 0);
    }

    // Check the data, the counter should be incremented by the number of control calls
    // the hook called (consumer) is more than the control calls less than the number of control calls (producer)
    assert(data.counter_a == CONTROL_CALLED_COUNT * NUM_THREADS_CONTROL);

    // the counter_b should be incremented by the number of hook calls minus the number of control calls
    // which is the number of times when the control input is not received i.e. jbpf_control_input_receive returns
    // failure
    assert(data.counter_b == HOOK_CALLED_COUNT * NUM_THREAD_HOOK - CONTROL_CALLED_COUNT * NUM_THREADS_CONTROL);

    // Unload the codeletset
    strcpy(codeletset_unload_req.codeletset_id.name, "simple_input_output_codeletset");
    assert(jbpf_codeletset_unload(&codeletset_unload_req, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    // free
    for (int i = 0; i < NUM_THREADS_CONTROL; i++) {
        free(params[i]);
    }
    free(params);

    // Stop
    jbpf_stop();
    printf("Test completed successfully\n");
    return 0;
}
