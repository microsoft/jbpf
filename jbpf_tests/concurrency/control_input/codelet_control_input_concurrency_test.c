/*
 * This test does the following:
1. Load a codeletset with a codelet that has one input map and one output map.
2. Start 4 threads that send control inputs to the codelet.
3. Thread 0 sends control inputs with values 0 to 999. Thread 1 sends control inputs with values 1000 to 1999. Thread 2
sends control inputs with values 2000 to 2999. Thread 3 sends control inputs with values 3000 to 3999.
4. Call the codelet's hook function.
5. Check that the codelet sends the correct output by counting the number of outputs received. And check that each value
is received exactly once.
 */

#include <assert.h>
#include <semaphore.h>
#include <pthread.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

#define COUNT 1000
#define NUM_THREADS_CONTROL 4

sem_t sem;

jbpf_io_stream_id_t stream_id_c1 = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

int counter = 0;
int received_values[COUNT * NUM_THREADS_CONTROL] = {};

struct thread_params
{
    jbpf_io_stream_id_t* stream_id;
    int thread_id;
};

static void
io_channel_check_output(jbpf_io_stream_id_t* stream_id, void** bufs, int num_bufs, void* ctx)
{
    int* output_val;

    for (int i = 0; i < num_bufs; i++) {
        if (memcmp(stream_id, &stream_id_c1, sizeof(stream_id_c1)) == 0) {
            output_val = bufs[i];
            assert(output_val);
            counter++;
            // mark that we receive the value
            received_values[*output_val]++;
        } else {
            // Unexpected output. Fail the test
            assert(false);
        }
    }
    // Signal that the output check is done
    if (counter == COUNT * NUM_THREADS_CONTROL) {
        // Check that we received all the values from the control inputs e.g. from 0 to 3999 each value should be
        // received once
        int missed = 0;
        for (int i = 0; i < COUNT * NUM_THREADS_CONTROL; i++) {
            if (received_values[i] != 1) {
                missed++;
            }
        }
        // printf("missed: %d\n", missed);
        // there should be no missed values
        assert(missed == 0);
        sem_post(&sem);
    }
}

void*
control_thread_func(void* arg)
{
    struct thread_params* params = (struct thread_params*)arg;
    jbpf_io_stream_id_t* stream_id = params->stream_id;
    jbpf_register_thread();
    for (int i = 0; i < COUNT; i++) {
        custom_api control_input = {.command = params->thread_id * COUNT + i};
        jbpf_send_input_msg(stream_id, &control_input, sizeof(control_input));
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
    codeletset_req.codelet_descriptor[0].num_out_io_channel = 1;
    codeletset_req.codelet_descriptor[0].num_linked_maps = 0;
    codeletset_req.codelet_descriptor[0].num_in_io_channel = 1;
    // The name of the input map is "input_map"
    strcpy(codeletset_req.codelet_descriptor[0].in_io_channel[0].name, "input_map");
    // Link the map to a stream id
    memcpy(&codeletset_req.codelet_descriptor[0].in_io_channel[0].stream_id, &stream_id_c1, JBPF_STREAM_ID_LEN);
    // The input channel of the codelet does not have a serializer
    codeletset_req.codelet_descriptor[0].in_io_channel[0].has_serde = false;
    // The name of the output map is "output_map"
    strcpy(codeletset_req.codelet_descriptor[0].out_io_channel[0].name, "output_map");
    // Link the map to a stream id
    memcpy(&codeletset_req.codelet_descriptor[0].out_io_channel[0].stream_id, &stream_id_c1, JBPF_STREAM_ID_LEN);
    // The input channel of the codelet does not have a serializer
    codeletset_req.codelet_descriptor[0].out_io_channel[0].has_serde = false;

    assert(jbpf_path != NULL);
    snprintf(
        codeletset_req.codelet_descriptor[0].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/simple_input_output/simple_input_output.o",
        jbpf_path);
    strcpy(codeletset_req.codelet_descriptor[0].codelet_name, "simple_input_output");
    strcpy(codeletset_req.codelet_descriptor[0].hook_name, "test1");

    assert(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Register a callback to handle the buffers sent from the codelets
    jbpf_register_io_output_cb(io_channel_check_output);

    sem_init(&sem, 0, 0);

    // Load the codeletset
    assert(jbpf_codeletset_load(&codeletset_req, NULL) == JBPF_CODELET_LOAD_SUCCESS);

    pthread_t control_threads[NUM_THREADS_CONTROL];
    struct thread_params** params = malloc(NUM_THREADS_CONTROL * sizeof(struct thread_params*));
    for (int i = 0; i < NUM_THREADS_CONTROL; i++) {
        // testing jbpf_io_channel_send_msg as it is thread safe
        params[i] = malloc(sizeof(struct thread_params));
        params[i]->stream_id = &stream_id_c1;
        params[i]->thread_id = i;
        int ret = pthread_create(&control_threads[i], NULL, control_thread_func, params[i]);
        JBPF_UNUSED(ret);
        assert(ret == 0);
    }

    for (int i = 0; i < NUM_THREADS_CONTROL; i++) {
        int ret = pthread_join(control_threads[i], NULL);
        JBPF_UNUSED(ret);
        assert(ret == 0);
    }

    for (int i = 0; i < COUNT * NUM_THREADS_CONTROL; i++) {
        hook_test1(NULL, 1);
    }

    // Wait for all the output checks to finish
    sem_wait(&sem);

    // Unload the codeletset
    strcpy(codeletset_unload_req.codeletset_id.name, "simple_input_output_codeletset");
    assert(jbpf_codeletset_unload(&codeletset_unload_req, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    sem_destroy(&sem);

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
