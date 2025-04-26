/*
 * The purpose of this test is to ensure that if a codelet floods an output channel, that the system has no crashes, and
 * that the data received from the output channel has valid streams and expected data values.
 *
 * This test does the following:
 * 1. It creates the config for a codeletset with a single codelet.
 * 2. This codelet uses object simple_output_floodChannel.o.   In this codelet object, the outChannel has a size of 10
 * elements. However, each time the codelet is called it attempts 100 writes to the outChannel.
 * 3. The code assert that the stream and data has valid values and no garbage.
 */

#include <assert.h>
#include <time.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "jbpf_utils.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"
#include "jbpf_test_lib.h"

#define NUM_ITERATIONS (5)
#define TOTAL_HOOK_CALLS (NUM_ITERATIONS)
#define MIN_EXPECTED_VALUE (1)
#define MAX_EXPECTED_VALUE (NUM_ITERATIONS)

jbpf_io_stream_id_t stream_id_c1 = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

static uint32_t data_bitmap = 0;

struct packet p = {0, 0};

static void
io_channel_check_output(jbpf_io_stream_id_t* stream_id, void** bufs, int num_bufs, void* ctx)
{
    for (int i = 0; i < num_bufs; i++) {

        // printf("stream = ");
        // for (int j = 0; j < JBPF_STREAM_ID_LEN; j++) {
        //     printf("%02x", stream_id->id[j]);
        // }
        // printf("\n");

        assert(memcmp(stream_id, &stream_id_c1, sizeof(jbpf_io_stream_id_t)) == 0);

        const int* output = bufs[i];
        // printf("output=%d\n", *output);

        assert(*output >= MIN_EXPECTED_VALUE && *output <= MAX_EXPECTED_VALUE);

        // set bitmap showing value of *output has been received
        data_bitmap |= (1 << (*output - MIN_EXPECTED_VALUE));
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

    assert(NUM_ITERATIONS <= 32);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    __assert__(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Register a callback to handle the buffers sent from the codelets
    jbpf_register_io_output_cb(io_channel_check_output);

    // Load the codeletset with codelet C1 in hook "test1"

    // The name of the codeletset
    strcpy(codeletset_req_c1.codeletset_id.name, "simple_output_floodChannel_codeletset");

    // Create max possible codelets
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
    __assert__(jbpf_path != NULL);
    snprintf(
        codeletset_req_c1.codelet_descriptor[0].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/simple_output_floodChannel/simple_output_floodChannel.o",
        jbpf_path);
    strcpy(codeletset_req_c1.codelet_descriptor[0].codelet_name, "simple_output_floodChannel");

    snprintf(codeletset_req_c1.codelet_descriptor[0].hook_name, JBPF_HOOK_NAME_LEN, "test1");

    // Load the codeletset
    __assert__(jbpf_codeletset_load(&codeletset_req_c1, NULL) == JBPF_CODELET_LOAD_SUCCESS);

    // Call the hooks 5 times and check that we got the expected data

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        hook_test1(&p, 0);
        usleep(10000);
    }

    assert(p.counter_a + p.counter_b == NUM_ITERATIONS * 100);

    // Check that we got all the expected data i.e. 0..(NUM_ITERATIONS)
    uint32_t expected_data_bitmap = (1 << MAX_EXPECTED_VALUE) - 1;
    printf("expected_data_bitmap = 0x%08x, data_bitmap = 0x%08x\n", expected_data_bitmap, data_bitmap);
    // assert(data_bitmap == expected_data_bitmap);

    // Unload the codeletsets
    codeletset_unload_req_c1.codeletset_id = codeletset_req_c1.codeletset_id;
    __assert__(jbpf_codeletset_unload(&codeletset_unload_req_c1, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");
    return 0;
}
