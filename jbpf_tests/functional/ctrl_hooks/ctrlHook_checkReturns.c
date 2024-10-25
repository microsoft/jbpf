/*
 * The purpose of this test a control hook.
 *
 * This test does the following:
 * 1. It creates a codeletset with 1 codelet.
 * 2. The codelet has 1 input and 1 output channel.  When the codelet runs it does the following:
 *    - A "counters_val" type is passed as the argument to the codelet.
 *    - The codelet reads from the input channeel. If it fails to read, it returns counters_val->u32_or_flag.
 *    - If its reads successfully, it checks the value of counters_val->u32_counter.
 *    -     If the value is even, it returns that value, and nothing is written to the output channel.
 *    -     If the value is odd, it attempts to write that value to the output channel.
 *    -        If it is able to write to the output channel, it returns counters_val->u32_xor_flag.
 *    -        If it is NOT able to write to the output channel, it returns counters_val->u32_and_flag.
 * 3. 95 messages sent to the input channel, and counters_val->u32_counter is incremented in each write.
 * 4. The hook is called 100 times.  All expected returns codes are asserted.
 * 5. The output data is checked and asserted for the expected values.
 * 6. The codelet is unloaded and the test completes.
 */

#include <assert.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include <semaphore.h>

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

jbpf_io_stream_id_t out_stream_id = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};
jbpf_io_stream_id_t in_stream_id = {
    .id = {0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00}};

#define NUM_ITERATIONS (100)
#define NUM_IN_MESSAGES (NUM_ITERATIONS - 5)

#define NUM_OUT_MESSAGES (NUM_IN_MESSAGES / 2)
#define MIN_EXPECTED_COMMAND_VALUE (1)
#define MAX_EXPECTED_COMMAND_VALUE (93)

#define CODELET_SUCCESS 10000
#define CODELET_INPUT_FAILURE 10001
#define CODELET_OUTPUT_FAILURE 10002

sem_t sem;

struct counter_vals cv = {
    .u32_counter = 0,
    .u32_and_flag = CODELET_OUTPUT_FAILURE,
    .u32_or_flag = CODELET_INPUT_FAILURE,
    .u32_xor_flag = CODELET_SUCCESS};

int processed = 0;

static void
io_channel_check_output(jbpf_io_stream_id_t* stream_id, void** bufs, int num_bufs, void* ctx)
{
    // int* output;

    for (int i = 0; i < num_bufs; i++) {

        // printf("stream = ");
        // for (int j = 0; j < JBPF_STREAM_ID_LEN; j++) {
        //     printf("%02x", stream_id->id[j]);
        // }
        // printf("\n");

        assert(memcmp(stream_id, &out_stream_id, sizeof(jbpf_io_stream_id_t)) == 0);

        custom_api* output = bufs[i];
        // printf("%d %d: output->command = %d\n", i, processed, output->command);
        assert(output->command >= ((processed * 2) + 1));

        processed++;
    }

    // printf("processed = %d\n", processed);

    if (processed == NUM_OUT_MESSAGES) {
        sem_post(&sem);
    }
}

int
main(int argc, char** argv)
{
    uint16_t codset = 0;
    // struct jbpf_codeletset_unload_req codeletset_unload_req_c1 = {0};
    const char* jbpf_path = getenv("JBPF_PATH");
    struct jbpf_config config = {0};
    // jbpf_codeletset_load_error_s err_msg = {0};

    jbpf_set_default_config_options(&config);

    sem_init(&sem, 0, 0);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    assert(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Register a callback to handle the buffers sent from the codelets
    jbpf_register_io_output_cb(io_channel_check_output);

    jbpf_codeletset_load_req_s codeletset_req = {0};

    jbpf_codeletset_load_req_s* codset_req = &codeletset_req;

    // Load the codeletset with codelet C1 in hook "test1"

    // The name of the codeletset
    snprintf(
        codset_req->codeletset_id.name, JBPF_CODELETSET_NAME_LEN, "control-input-withFailures_codeletset-%d", codset);

    // Create max possible codelets
    codset_req->num_codelet_descriptors = 1;

    jbpf_codelet_descriptor_s* cod_desc = &codset_req->codelet_descriptor[0];

    // The codelet has just one input channel and no shared maps
    cod_desc->num_in_io_channel = 1;
    // The name of the input map that corresponds to the input channel of the codelet
    strcpy(cod_desc->in_io_channel[0].name, "input_map");
    // Link the map to a stream id
    memcpy(&cod_desc->in_io_channel[0].stream_id, &in_stream_id, JBPF_STREAM_ID_LEN);
    // The input channel of the codelet does not have a serializer
    cod_desc->in_io_channel[0].has_serde = false;
    cod_desc->num_linked_maps = 0;

    cod_desc->num_out_io_channel = 1;
    // The name of the output map that corresponds to the output channel of the codelet
    strcpy(cod_desc->out_io_channel[0].name, "output_map");
    // Link the map to a stream id
    memcpy(&cod_desc->out_io_channel[0].stream_id, &out_stream_id, JBPF_STREAM_ID_LEN);
    // The output channel of the codelet does not have a serializer
    cod_desc->out_io_channel[0].has_serde = false;
    cod_desc->num_linked_maps = 0;

    // The path of the codelet
    assert(jbpf_path != NULL);
    snprintf(
        cod_desc->codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/codelet-control-input-withFailures/codelet-control-input-withFailures.o",
        jbpf_path);

    snprintf(cod_desc->codelet_name, JBPF_CODELET_NAME_LEN, "control-input-withFailures");
    strcpy(cod_desc->hook_name, "ctrl_test0");

    // Load the codeletset
    assert(jbpf_codeletset_load(codset_req, NULL) == JBPF_CODELET_LOAD_SUCCESS);

    // Send NUM_IN_MESSAGES messages to the input channel
    for (int i = 0; i < NUM_IN_MESSAGES; i++) {
        // send to input channel 1
        custom_api control_input = {0};
        assert(jbpf_send_input_msg(&in_stream_id, &control_input, sizeof(control_input)) == 0);
    }

    // Call the hooks NUM_ITERATIONS times and check that we got the expected data
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        uint32_t ret = (uint32_t)ctrl_hook_ctrl_test0(&cv, 1);
        if (i < NUM_IN_MESSAGES) {
            if (i % 2 == 0) {
                // if u32_counter is even, the codelet sends this value in the output
                assert(ret == cv.u32_counter);
            } else {
                // if u32_counter is odd, the codelet sends ...
                //  in error cases, the value in u32_and_flag
                //  in success cases, the value in u32_xor_flag
                assert(ret == CODELET_SUCCESS);
            }
        } else {
            // if the codelet does not receive any input, it sends the value in u32_or_flag
            assert(ret == CODELET_INPUT_FAILURE);
        }
        cv.u32_counter++;
    }

    sem_wait(&sem);

    // unload the codeletSet
    jbpf_codeletset_unload_req_s codeletset_unload_req;
    jbpf_codeletset_unload_req_s* codset_unload_req = &codeletset_unload_req;
    codset_unload_req->codeletset_id = codset_req->codeletset_id;
    assert(jbpf_codeletset_unload(codset_unload_req, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");
    return 0;
}
