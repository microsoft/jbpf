/*
 * The purpose of this test is to ensure we can configure an "out" channel but dont set Serde, it fails validation
 *
 * This test does the following:
 * 1. It creates the config for a codeletset with a single codelet with a single output channel.
 * 2. It populates the Serde of the out_io_channel with a string which is zero length.
 * 3. It asserts that the return code from the jbpf_codeletset_load call is JBPF_CODELET_PARAM_INVALID.
 */

#include <assert.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

jbpf_io_stream_id_t stream_id_c1 = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

int
main(int argc, char** argv)
{

#ifdef JBPF_EXPERIMENTAL_FEATURES

    struct jbpf_codeletset_load_req codeletset_req_c1 = {0};
    const char* jbpf_path = getenv("JBPF_PATH");
    struct jbpf_config config = {0};
    jbpf_codeletset_load_error_s err_msg = {0};
    jbpf_set_default_config_options(&config);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    assert(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Load the codeletset with codelet C1 in hook "test1"

    // The name of the codeletset
    strcpy(codeletset_req_c1.codeletset_id.name, "simple_output_shared_codeletset");

    // We have only one codelet in this codeletset
    codeletset_req_c1.num_codelet_descriptors = 1;

    codeletset_req_c1.codelet_descriptor[0].num_out_io_channel = 1;

    strcpy(codeletset_req_c1.codelet_descriptor[0].out_io_channel[0].name, "output_map");

    // The output channel of the codelet does not have a serializer
    codeletset_req_c1.codelet_descriptor[0].out_io_channel[0].has_serde = true;
    codeletset_req_c1.codelet_descriptor[0].out_io_channel[0].serde.file_path[0] = '\0';

    // Link the map to a stream id
    memcpy(&codeletset_req_c1.codelet_descriptor[0].out_io_channel[0].stream_id, &stream_id_c1, JBPF_STREAM_ID_LEN);

    codeletset_req_c1.codelet_descriptor[0].num_in_io_channel = 0;
    codeletset_req_c1.codelet_descriptor[0].num_linked_maps = 0;

    // The path of the codelet
    if (!jbpf_path) {
        snprintf(
            codeletset_req_c1.codelet_descriptor[0].codelet_path,
            JBPF_PATH_LEN,
            "../../jbpf_tests/test_files/codelets/simple_output_shared/simple_output_shared.o");
    } else {
        snprintf(
            codeletset_req_c1.codelet_descriptor[0].codelet_path,
            JBPF_PATH_LEN,
            "%s/jbpf_tests/test_files/codelets/simple_output_shared/simple_output_shared.o",
            jbpf_path);
    }
    strcpy(codeletset_req_c1.codelet_descriptor[0].codelet_name, "simple_output_shared");
    strcpy(codeletset_req_c1.codelet_descriptor[0].hook_name, "test1");

    assert(jbpf_codeletset_load(&codeletset_req_c1, &err_msg) == JBPF_CODELET_PARAM_INVALID);
    assert(strlen(err_msg.err_msg) > 0);

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");

#else

    printf("Test not executed as JBPF_EXPERIMENTAL_FEATURES not set\n");

#endif

    return 0;
}
