/*
 * The purpose of this test is to ensure that if, for a given codeletm  we can configure a linked map and point to
 * itself, this fails. validation. will fail.
 *
 * This test does the following:
 * 1. It loads a codeletset with two codelets.
 * 2. Codelet 1 is configured with file codelets/simple_input_shared/simple_input_shared.o.  This an one input channel
 * ("input_map"), and one shared channel ("shared_map").
 * 3. Codelet 2 is configured with file codelets/simple_output_2shared/simple_output_2shared.o.  This has one input
 * channel
 * ("output_map"), and two shared channel ("shared_map_output0" and "shared_map_output1").
 * 4. Codelet 2 is configured with 1 linked map, whuich links simple_output_2shared/shared_map_output0 -->
 * simple_output_2shared/shared_map_output1.  This should fail.
 * 4. The code assert that load will fail with return code JBPF_CODELET_CREATION_FAIL.
 */

#include <assert.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "jbpf_utils.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

jbpf_io_stream_id_t stream_id_c1 = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

jbpf_io_stream_id_t stream_id_c2 = {
    .id = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11}};

int
main(int argc, char** argv)
{
    struct jbpf_codeletset_load_req codeletset_req_c1 = {0};
    const char* jbpf_path = getenv("JBPF_PATH");
    jbpf_codeletset_load_error_s err_msg = {0};
    struct jbpf_config config = {0};

    JBPF_UNUSED(err_msg);

    jbpf_set_default_config_options(&config);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    assert(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Load the codeletset with codelet C1 in hook "test1"

    // The name of the codeletset
    strcpy(codeletset_req_c1.codeletset_id.name, "linked_map_codeletset");

    // We have only one codelet in this codeletset
    codeletset_req_c1.num_codelet_descriptors = 2;

    // codelet 1

    codeletset_req_c1.codelet_descriptor[0].num_in_io_channel = 1;

    strcpy(codeletset_req_c1.codelet_descriptor[0].in_io_channel[0].name, "input_map");

    memcpy(&codeletset_req_c1.codelet_descriptor[0].in_io_channel[0].stream_id, &stream_id_c1, JBPF_STREAM_ID_LEN);

    codeletset_req_c1.codelet_descriptor[0].in_io_channel[0].has_serde = false;

    codeletset_req_c1.codelet_descriptor[0].num_out_io_channel = 0;
    codeletset_req_c1.codelet_descriptor[0].num_linked_maps = 0;

    // The path of the codelet
    assert(jbpf_path != NULL);
    snprintf(
        codeletset_req_c1.codelet_descriptor[0].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/simple_input_shared/simple_input_shared.o",
        jbpf_path);
    strcpy(codeletset_req_c1.codelet_descriptor[0].codelet_name, "simple_input_shared");
    strcpy(codeletset_req_c1.codelet_descriptor[0].hook_name, "test1");

    // codelet 2

    codeletset_req_c1.codelet_descriptor[1].num_out_io_channel = 1;

    strcpy(codeletset_req_c1.codelet_descriptor[1].out_io_channel[0].name, "output_map");

    memcpy(&codeletset_req_c1.codelet_descriptor[1].out_io_channel[0].stream_id, &stream_id_c2, JBPF_STREAM_ID_LEN);

    codeletset_req_c1.codelet_descriptor[1].out_io_channel[0].has_serde = false;

    codeletset_req_c1.codelet_descriptor[1].num_in_io_channel = 0;

    codeletset_req_c1.codelet_descriptor[1].num_linked_maps = 1;

    strcpy(codeletset_req_c1.codelet_descriptor[1].linked_maps[0].map_name, "shared_map_output0");
    // linked codelet name is same as the codelet name !! This should fail
    strcpy(codeletset_req_c1.codelet_descriptor[1].linked_maps[0].linked_codelet_name, "simple_output_2shared");
    strcpy(codeletset_req_c1.codelet_descriptor[1].linked_maps[0].linked_map_name, "shared_map_output1");

    // The path of the codelet
    assert(jbpf_path != NULL);
    snprintf(
        codeletset_req_c1.codelet_descriptor[1].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/simple_output_2shared/simple_output_2shared.o",
        jbpf_path);

    strcpy(codeletset_req_c1.codelet_descriptor[1].codelet_name, "simple_output_2shared");
    strcpy(codeletset_req_c1.codelet_descriptor[1].hook_name, "test1");

    // Load the codeletset
    assert(jbpf_codeletset_load(&codeletset_req_c1, &err_msg) == JBPF_CODELET_PARAM_INVALID);
    assert(strlen(err_msg.err_msg) > 0);
    assert(
        strncmp(
            err_msg.err_msg,
            "codelet simple_output_2shared has a linked map in the same codelet",
            strlen("codelet simple_output_2shared has a linked map in the same codelet")) == 0);

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");
    return 0;
}
