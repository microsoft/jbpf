/*
 * The purpose of this test is to ensure if you specify more codelets than is populated, it is captured during
 * validation checks.
 *
 * This test does the following:
 * 1. It creates the config for a codeletset.
 * 2. It sets codeletset_req_c1.num_codelet_descriptors to 5, but only populates 4.
 * 3. It asserts that the return code from the jbpf_codeletset_load call is JBPF_CODELET_PARAM_INVALID).
 */

#include <assert.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "jbpf_utils.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

jbpf_io_stream_id_t stream_id_c1 = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

int
main(int argc, char** argv)
{
    struct jbpf_codeletset_load_req codeletset_req_c1 = {0};
    const char* jbpf_path = getenv("JBPF_PATH");
    struct jbpf_config config = {0};
    jbpf_codeletset_load_error_s err_msg = {0};

    JBPF_UNUSED(err_msg);

    jbpf_set_default_config_options(&config);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    assert(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Load the codeletset with codelet C1 in hook "test1"

    // The name of the codeletset
    strcpy(codeletset_req_c1.codeletset_id.name, "simple_output_codeletset");

    // Set num_codelet_descriptors to 5, but only populate 4
    codeletset_req_c1.num_codelet_descriptors = 5;

    uint16_t i = 0;
    for (i = 0; i < (codeletset_req_c1.num_codelet_descriptors - 1); i++) {

        // The codelet has just one output channel and no shared maps
        codeletset_req_c1.codelet_descriptor[i].num_in_io_channel = 0;
        codeletset_req_c1.codelet_descriptor[i].num_out_io_channel = 1;

        // The name of the output map that corresponds to the output channel of the codelet
        strcpy(codeletset_req_c1.codelet_descriptor[i].out_io_channel[0].name, "output_map");
        // Link the map to a stream id
        stream_id_c1.id[(JBPF_STREAM_ID_LEN - 1)] = (uint8_t)i;
        memcpy(&codeletset_req_c1.codelet_descriptor[i].out_io_channel[0].stream_id, &stream_id_c1, JBPF_STREAM_ID_LEN);
        // The output channel of the codelet does not have a serializer
        codeletset_req_c1.codelet_descriptor[i].out_io_channel[0].has_serde = false;
        codeletset_req_c1.codelet_descriptor[i].num_linked_maps = 0;

        // The path of the codelet
        assert(jbpf_path != NULL);
        snprintf(
            codeletset_req_c1.codelet_descriptor[i].codelet_path,
            JBPF_PATH_LEN,
            "%s/jbpf_tests/test_files/codelets/simple_output/simple_output.o",
            jbpf_path);

        strcpy(codeletset_req_c1.codelet_descriptor[i].codelet_name, "simple_output");
        strcpy(codeletset_req_c1.codelet_descriptor[i].hook_name, "test1");
    }

    // Load the codeletset
    assert(jbpf_codeletset_load(&codeletset_req_c1, &err_msg) == JBPF_CODELET_PARAM_INVALID);
    assert(strlen(err_msg.err_msg) > 0);

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");
    return 0;
}
