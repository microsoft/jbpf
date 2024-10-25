/*
 * This test does the following:
 * 1. It loads a codeletset with a single codelet (C1) that sends a counter as an output.
 * 2. It loads a second codeletset with two codelets (C2, C3) sharing a map.
 * 3. C1 sends out a counter value, which is incremented by one on each hook call.
 * 3. A control input is sent to codelet C2 and is written to the shared map.
 * 4. C3 picks the data from the shared map and sends them out through an output map.
 */

#include <assert.h>
#include <semaphore.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "jbpf_utils.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

#define NUM_ITERATIONS 5

sem_t sem;

jbpf_io_stream_id_t stream_id_c1 = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};
jbpf_io_stream_id_t stream_id_c2 = {
    .id = {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00}};
jbpf_io_stream_id_t stream_id_c3 = {
    .id = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11}};

int expected_c1_value = 0;
int expected_c2_c3_value = 100;

static void
io_channel_check_output(jbpf_io_stream_id_t* stream_id, void** bufs, int num_bufs, void* ctx)
{
    int* c1_output;
    int* c2_c3_output;

    JBPF_UNUSED(c1_output);
    JBPF_UNUSED(c2_c3_output);

    for (int i = 0; i < num_bufs; i++) {

        if (memcmp(stream_id, &stream_id_c1, sizeof(stream_id_c1)) == 0) {
            // Output from C1. Check that the counter has the expected value
            c1_output = bufs[i];
            assert(c1_output);
            assert(*c1_output == expected_c1_value);
            expected_c1_value++;
        } else if (memcmp(stream_id, &stream_id_c3, sizeof(stream_id_c3)) == 0) {
            // Output from C3. Check that the output is the same as the input we sent to C2
            c2_c3_output = bufs[i];
            assert(c2_c3_output);
            assert(*c2_c3_output == expected_c2_c3_value);
            expected_c2_c3_value++;
        } else {
            // Unexpected output. Fail the test
            assert(false);
        }
    }

    if (expected_c1_value == NUM_ITERATIONS && expected_c2_c3_value == 105) {
        sem_post(&sem);
    }
}

int
main(int argc, char** argv)
{
    struct jbpf_codeletset_load_req codeletset_req_c1 = {0}, codeletset_req_c2_c3 = {0};
    struct jbpf_codeletset_unload_req codeletset_unload_req_c1 = {0}, codeletset_unload_req_c2_c3 = {0};
    const char* jbpf_path = getenv("JBPF_PATH");
    struct jbpf_config config = {0};

    jbpf_set_default_config_options(&config);
    sem_init(&sem, 0, 0);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    assert(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Register a callback to handle the buffers sent from the codelets
    jbpf_register_io_output_cb(io_channel_check_output);

    // Load the codeletset with codelet C1 in hook "test1"

    // The name of the codeletset
    strcpy(codeletset_req_c1.codeletset_id.name, "simple_output_codeletset");

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
        "%s/jbpf_tests/test_files/codelets/simple_output/simple_output.o",
        jbpf_path);
    strcpy(codeletset_req_c1.codelet_descriptor[0].codelet_name, "simple_output");
    strcpy(codeletset_req_c1.codelet_descriptor[0].hook_name, "test1");

    // Load the codeletset
    assert(jbpf_codeletset_load(&codeletset_req_c1, NULL) == JBPF_CODELET_LOAD_SUCCESS);

    // Next, we load the codeletset with codelets C2 and C3 in hooks "test1" and "test2"
    strcpy(codeletset_req_c2_c3.codeletset_id.name, "shared_map_input_output_codeletset");
    // This codeletset has 2 codelets
    codeletset_req_c2_c3.num_codelet_descriptors = 2;

    // Codelet 1 (C2) has one input map and no output map
    codeletset_req_c2_c3.codelet_descriptor[0].num_out_io_channel = 0;
    codeletset_req_c2_c3.codelet_descriptor[0].num_linked_maps = 0;
    codeletset_req_c2_c3.codelet_descriptor[0].num_in_io_channel = 1;
    // The name of the input map is "input_map"
    strcpy(codeletset_req_c2_c3.codelet_descriptor[0].in_io_channel[0].name, "input_map");
    // Link the map to a stream id
    memcpy(&codeletset_req_c2_c3.codelet_descriptor[0].in_io_channel[0].stream_id, &stream_id_c2, JBPF_STREAM_ID_LEN);
    // The input channel of the codelet does not have a serializer
    codeletset_req_c2_c3.codelet_descriptor[0].in_io_channel[0].has_serde = false;

    assert(jbpf_path != NULL);
    snprintf(
        codeletset_req_c2_c3.codelet_descriptor[0].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/simple_input_shared/simple_input_shared.o",
        jbpf_path);
    strcpy(codeletset_req_c2_c3.codelet_descriptor[0].codelet_name, "simple_input_shared");
    strcpy(codeletset_req_c2_c3.codelet_descriptor[0].hook_name, "test1");

    // Codelet 2 (C3) has one output map and one shared map linked to C2
    codeletset_req_c2_c3.codelet_descriptor[1].num_in_io_channel = 0;
    codeletset_req_c2_c3.codelet_descriptor[1].num_out_io_channel = 1;
    strcpy(codeletset_req_c2_c3.codelet_descriptor[1].out_io_channel[0].name, "output_map");
    memcpy(&codeletset_req_c2_c3.codelet_descriptor[1].out_io_channel[0].stream_id, &stream_id_c3, JBPF_STREAM_ID_LEN);
    codeletset_req_c2_c3.codelet_descriptor[1].out_io_channel[0].has_serde = false;

    codeletset_req_c2_c3.codelet_descriptor[1].num_linked_maps = 1;
    // The name of the linked map as it appears in C3
    strcpy(codeletset_req_c2_c3.codelet_descriptor[1].linked_maps[0].map_name, "shared_map_output");
    // The codelet that contains the map that this map will be linked to
    strcpy(codeletset_req_c2_c3.codelet_descriptor[1].linked_maps[0].linked_codelet_name, "simple_input_shared");
    // The name of the linked map as it appears in C2
    strcpy(codeletset_req_c2_c3.codelet_descriptor[1].linked_maps[0].linked_map_name, "shared_map");

    assert(jbpf_path != NULL);
    snprintf(
        codeletset_req_c2_c3.codelet_descriptor[1].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/simple_output_shared/simple_output_shared.o",
        jbpf_path);

    strcpy(codeletset_req_c2_c3.codelet_descriptor[1].codelet_name, "simple_output_shared");
    strcpy(codeletset_req_c2_c3.codelet_descriptor[1].hook_name, "test2");

    // Load the codeletset
    assert(jbpf_codeletset_load(&codeletset_req_c2_c3, NULL) == JBPF_CODELET_LOAD_SUCCESS);

    // Send 5 control inputs
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        custom_api control_input = {.command = i + 100};
        jbpf_send_input_msg(&stream_id_c2, &control_input, sizeof(control_input));
    }

    // Call the hooks 5 times and check that we got the expected data
    struct packet p = {0, 0};

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        hook_test1(&p, 1);
        hook_test2(&p, 2);

        p.counter_a++;
    }

    // Wait for all the output checks to finish
    sem_wait(&sem);

    // Unload the codeletsets
    strcpy(codeletset_unload_req_c1.codeletset_id.name, "simple_output_codeletset");
    assert(jbpf_codeletset_unload(&codeletset_unload_req_c1, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    strcpy(codeletset_unload_req_c2_c3.codeletset_id.name, "shared_map_input_output_codeletset");
    assert(jbpf_codeletset_unload(&codeletset_unload_req_c2_c3, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    // Stop
    jbpf_stop();

    sem_destroy(&sem);

    printf("Test completed successfully\n");
    return 0;
}
