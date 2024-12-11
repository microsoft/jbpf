// Copyright (c) Microsoft Corporation. All rights reserved.
// This test demonstrates how to replace an existing helper function in the codelets
// with a custom implementation. The test uses the helper function jbpf_time_get_ns
// as an example. The test creates a codelet that calls the helper function jbpf_time_get_ns
// and sends the result to an output channel. The test replaces the helper function with a
// custom implementation that returns a counter value. The test then calls the codelet
// multiple times and checks that the output from the codelet is the expected value.

#include <assert.h>
#include <semaphore.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "jbpf_utils.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

#define NUM_ITERATIONS 100

sem_t sem;

static uint64_t cnt = 0;

uint64_t
jbpf_perf_time_get_ns_replacement(void)
{
    return cnt++;
}

jbpf_io_stream_id_t stream_id_c1 = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

static uint64_t received_value = 0; // The value received from the codelet

static void
io_channel_check_output(jbpf_io_stream_id_t* stream_id, void** bufs, int num_bufs, void* ctx)
{
    int* c1_output;
    JBPF_UNUSED(c1_output);

    for (int i = 0; i < num_bufs; i++) {
        if (memcmp(stream_id, &stream_id_c1, sizeof(stream_id_c1)) == 0) {
            // Output from C1. Check that the counter has the expected value
            c1_output = bufs[i];
            assert(c1_output);
            assert(*c1_output == received_value);
            received_value++;
        } else {
            // Unexpected output. Fail the test
            assert(false);
        }
    }

    if (received_value == NUM_ITERATIONS) {
        sem_post(&sem);
    }
}

int
main(int argc, char** argv)
{
    struct jbpf_codeletset_load_req codeletset_req_c1 = {0};
    struct jbpf_codeletset_unload_req codeletset_unload_req_c1 = {0};
    const char* jbpf_path = getenv("JBPF_PATH");
    struct jbpf_config config = {0};

    jbpf_set_default_config_options(&config);
    sem_init(&sem, 0, 0);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    assert(jbpf_init(&config) == 0);

    // replacing the helper function jbpf_time_get_ns
    jbpf_helper_func_def_t helper_func1;
    strcpy(helper_func1.name, "jbpf_time_get_ns");
    helper_func1.reloc_id = JBPF_TIME_GET_NS;
    helper_func1.function_cb = (jbpf_helper_func_t)jbpf_perf_time_get_ns_replacement;

    assert(jbpf_register_helper_function(helper_func1) == 1);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Register a callback to handle the buffers sent from the codelets
    jbpf_register_io_output_cb(io_channel_check_output);

    // The name of the codeletset
    strcpy(codeletset_req_c1.codeletset_id.name, "time_events_codeletset");

    // We have only one codelet in this codeletset
    codeletset_req_c1.num_codelet_descriptors = 1;

    // The codelet has just one output channel and no shared maps
    codeletset_req_c1.codelet_descriptor[0].num_in_io_channel = 0;
    codeletset_req_c1.codelet_descriptor[0].num_out_io_channel = 1;

    // The name of the output map that corresponds to the output channel of the codelet
    strcpy(codeletset_req_c1.codelet_descriptor[0].out_io_channel[0].name, "output_map");
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
        "%s/jbpf_tests/test_files/codelets/time_events/time_events.o",
        jbpf_path);
    strcpy(codeletset_req_c1.codelet_descriptor[0].codelet_name, "time_events");
    strcpy(codeletset_req_c1.codelet_descriptor[0].hook_name, "test1");

    // Load the codeletset
    assert(jbpf_codeletset_load(&codeletset_req_c1, NULL) == JBPF_CODELET_LOAD_SUCCESS);

    // Call the hooks NUM_ITERATIONS times and check that we got the expected data
    struct packet p = {0, 0};

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        hook_test1(&p, 1);
    }

    // Wait for all the output checks to finish
    sem_wait(&sem);

    // Unload the codeletsets
    strcpy(codeletset_unload_req_c1.codeletset_id.name, "time_events_codeletset");
    assert(jbpf_codeletset_unload(&codeletset_unload_req_c1, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    // Stop
    jbpf_stop();

    sem_destroy(&sem);

    printf("Test completed successfully\n");
    return 0;
}
