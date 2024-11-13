/*
 * The purpose of this test is to ensure that if a codelet floods an input channel, that the system has no crashes, and
 * that the data is transferred as expected.
 *
 * This test does the following:
 * 1. It creates the config for a codeletSet with a single codelet.
 * 2. This codelet uses object control-input-output.o.   In this codelet object, there are 2 input channels and 2 output
 * channels. The codelet reads data from input channel 1 and sends it output channel 1, and reads data from input
 * channel 2 and sends it to output channel 2.
 * 3. Both input channels are 100 elements long, and the output is 1000 elements long.  Although the input channel size
 * is 100, jbpf changes it to 255 in the code processing.
 * 4. The code attempts to send 300 messages to the input channels, and asserts that only the first 255 of these are
 * successful.
 * 5. It calls the codelet 300 times, and asserts that the output is as expected.
 */

#include <assert.h>
#include <time.h>
#include <semaphore.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "jbpf_utils.h"
#include "jbpf_io_thread_mgmt.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

#define NUM_IN_CHANNELS (2)
#define IN_CHANNEL_SIZE (100)
#define NUM_OUT_CHANNELS (2)

#define NUM_ITERATIONS (IN_CHANNEL_SIZE * 3)

#define round_up_pow_of_two(x) \
    ({                         \
        uint32_t v = x;        \
        v--;                   \
        v |= v >> 1;           \
        v |= v >> 2;           \
        v |= v >> 4;           \
        v |= v >> 8;           \
        v |= v >> 16;          \
        ++v;                   \
    })

#define ACTUAL_IN_CHANNEL_SIZE (round_up_pow_of_two((IN_CHANNEL_SIZE + JBPF_IO_MAX_NUM_THREADS)) - 1)

#define MIN_EXPECTED_COMMAND_VALUE (100)
#define MAX_EXPECTED_COMMAND_VALUE (MIN_EXPECTED_COMMAND_VALUE + ACTUAL_IN_CHANNEL_SIZE)
#define MIN_EXPECTED_PARAMETER1_VALUE (100)
#define MAX_EXPECTED_PARAMETER1_VALUE (MIN_EXPECTED_PARAMETER1_VALUE + ACTUAL_IN_CHANNEL_SIZE)
#define MIN_EXPECTED_PARAMETER2_VALUE (2100)
#define MAX_EXPECTED_PARAMETER2_VALUE (MIN_EXPECTED_PARAMETER2_VALUE + ACTUAL_IN_CHANNEL_SIZE)

jbpf_io_stream_id_t stream_in_ids[] = {
    {.id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}},
    {.id = {0x11, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}}};

jbpf_io_stream_id_t stream_out_ids[] = {
    {.id = {0x22, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}},
    {.id = {0x33, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}}};

#define OUTPUT_BIN_SIZE (1000) /* set size to be > ACTUAL_IN_CHANNEL_SIZE */
int output0_bins[OUTPUT_BIN_SIZE] = {0};
int output1_parameter1_bins[OUTPUT_BIN_SIZE] = {0};
int output1_parameter2_bins[OUTPUT_BIN_SIZE] = {0};

struct packet4 p = {0};

sem_t sem;

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

        if (memcmp(stream_id, &stream_out_ids[0], sizeof(jbpf_io_stream_id_t)) == 0) {
            custom_api* output = bufs[i];
            // printf("0: output->command = %d\n", output->command);
            assert(output->command >= MIN_EXPECTED_COMMAND_VALUE && output->command <= MAX_EXPECTED_COMMAND_VALUE);
            output0_bins[output->command - MIN_EXPECTED_COMMAND_VALUE]++;

        } else if (memcmp(stream_id, &stream_out_ids[1], sizeof(jbpf_io_stream_id_t)) == 0) {
            custom_api2* output = bufs[i];
            // printf("1: output->parameter1 = %d, output->parameter2 = %d\n", output->parameter1, output->parameter2);

            assert(
                output->parameter1 >= MIN_EXPECTED_PARAMETER1_VALUE &&
                output->parameter1 <= MAX_EXPECTED_PARAMETER1_VALUE);
            output1_parameter1_bins[output->parameter1 - MIN_EXPECTED_PARAMETER1_VALUE]++;

            assert(
                output->parameter2 >= MIN_EXPECTED_PARAMETER2_VALUE &&
                output->parameter2 <= MAX_EXPECTED_PARAMETER2_VALUE);
            output1_parameter2_bins[output->parameter2 - MIN_EXPECTED_PARAMETER2_VALUE]++;

        } else {
            // Unexpected output. Fail the test
            assert(false);
        }

        processed++;
    }

    // printf("processed = %d\n", processed);

    if (processed == (ACTUAL_IN_CHANNEL_SIZE * NUM_OUT_CHANNELS)) {
        for (int i = 0; i < OUTPUT_BIN_SIZE; i++) {
            int expected = (i < ACTUAL_IN_CHANNEL_SIZE) ? 1 : 0;

            JBPF_UNUSED(expected);
            
            assert(output0_bins[i] == expected);
            assert(output1_parameter1_bins[i] == expected);
            assert(output1_parameter2_bins[i] == expected);
        }
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

    JBPF_UNUSED(codeletset_unload_req_c1);

    jbpf_set_default_config_options(&config);

    sem_init(&sem, 0, 0);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    assert(jbpf_init(&config) == 0);

    assert(OUTPUT_BIN_SIZE >= ACTUAL_IN_CHANNEL_SIZE);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Register a callback to handle the buffers sent from the codelets
    jbpf_register_io_output_cb(io_channel_check_output);

    // Load the codeletset with codelet C1 in hook "test1"

    // The name of the codeletset
    strcpy(codeletset_req_c1.codeletset_id.name, "control_input_floodChannel_codeletset");

    // Create max possible codelets
    codeletset_req_c1.num_codelet_descriptors = 1;

    codeletset_req_c1.codelet_descriptor[0].num_in_io_channel = NUM_IN_CHANNELS;
    for (int inCh = 0; inCh < codeletset_req_c1.codelet_descriptor[0].num_in_io_channel; inCh++) {
        // The name of the input map that corresponds to the input channel of the codelet
        jbpf_io_channel_name_t ch_name = {0};
        snprintf(ch_name, JBPF_IO_CHANNEL_NAME_LEN, "input_map%d", inCh + 1);
        strcpy(codeletset_req_c1.codelet_descriptor[0].in_io_channel[inCh].name, ch_name);

        // Link the map to a stream id
        memcpy(
            &codeletset_req_c1.codelet_descriptor[0].in_io_channel[inCh].stream_id,
            &stream_in_ids[inCh],
            JBPF_STREAM_ID_LEN);
        // The input channel of the codelet does not have a serializer
        codeletset_req_c1.codelet_descriptor[0].in_io_channel[inCh].has_serde = false;
    }

    codeletset_req_c1.codelet_descriptor[0].num_out_io_channel = NUM_OUT_CHANNELS;
    // codeletset_req_c1.codelet_descriptor[0].num_out_io_channel = 0;
    for (int outCh = 0; outCh < codeletset_req_c1.codelet_descriptor[0].num_out_io_channel; outCh++) {
        // The name of the output map that corresponds to the output channel of the codelet
        jbpf_io_channel_name_t ch_name = {0};
        snprintf(ch_name, JBPF_IO_CHANNEL_NAME_LEN, "output_map%d", outCh + 1);
        strcpy(codeletset_req_c1.codelet_descriptor[0].out_io_channel[outCh].name, ch_name);

        // Link the map to a stream id
        memcpy(
            &codeletset_req_c1.codelet_descriptor[0].out_io_channel[outCh].stream_id,
            &stream_out_ids[outCh],
            JBPF_STREAM_ID_LEN);
        // The output channel of the codelet does not have a serializer
        codeletset_req_c1.codelet_descriptor[0].out_io_channel[outCh].has_serde = false;
    }

    codeletset_req_c1.codelet_descriptor[0].num_linked_maps = 0;

    // The path of the codelet
    assert(jbpf_path != NULL);
    snprintf(
        codeletset_req_c1.codelet_descriptor[0].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/codelet-control-input-output/codelet-control-input.o",
        jbpf_path);
    strcpy(codeletset_req_c1.codelet_descriptor[0].codelet_name, "codelet-control-input");

    snprintf(codeletset_req_c1.codelet_descriptor[0].hook_name, JBPF_HOOK_NAME_LEN, "test3");

    // Load the codeletset
    assert(jbpf_codeletset_load(&codeletset_req_c1, NULL) == JBPF_CODELET_LOAD_SUCCESS);

    // Flood the input channels with control inputs.
    // Call the hooks 5 times and check that we got the expected data
    for (int i = 0; i < NUM_ITERATIONS; i++) {

        // send to input channel 1
        custom_api control_input = {.command = i + MIN_EXPECTED_COMMAND_VALUE};
        int ret = jbpf_send_input_msg(&stream_in_ids[0], &control_input, sizeof(control_input));
        int expected_ret = (i < ACTUAL_IN_CHANNEL_SIZE) ? 0 : -1;
        assert(ret == expected_ret);

        JBPF_UNUSED(ret);
        JBPF_UNUSED(expected_ret);

        // send to input channel 2
        custom_api2 control_input2 = {
            .parameter1 = i + MIN_EXPECTED_PARAMETER1_VALUE, .parameter2 = i + MIN_EXPECTED_PARAMETER2_VALUE};
        ret = jbpf_send_input_msg(&stream_in_ids[1], &control_input2, sizeof(control_input2));
        expected_ret = (i < ACTUAL_IN_CHANNEL_SIZE) ? 0 : -1;
        assert(ret == expected_ret);
    }

    // Call the hooks 5 times and check that we got the expected data
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        hook_test3(&p, 1);
    }

    sem_wait(&sem);

    // Unload the codeletsets
    codeletset_unload_req_c1.codeletset_id = codeletset_req_c1.codeletset_id;
    assert(jbpf_codeletset_unload(&codeletset_unload_req_c1, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");
    return 0;
}
