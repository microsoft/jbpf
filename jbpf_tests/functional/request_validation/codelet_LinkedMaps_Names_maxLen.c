/*
 * The purpose of this test is to ensure we can configure "out" channels, "in" channels, and "linked map" channels with
 * the maximum name length.
 *
 * This test does the following:
 * 1. It creates the config for a codeletset with a two codelets.
 * 2. Codelet 1 has a single input channel, and a shared chhannel, each with a name of the maximum name length.
 * 3. Codelet 2 has a single output channel, and a shared chhannel, each with a name of the maximum name length.
 * 4. Codelet 2 has a linked map with a name of the maximum name length.
 * 3. It asserts that the return code from the jbpf_codeletset_load call is JBPF_CODELET_LOAD_SUCCESS).
 * 4. It unloads the codeletSet.
 */

#include <assert.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"
#include "jbpf_test_lib.h"

jbpf_io_stream_id_t stream_id_c1 = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

jbpf_io_stream_id_t stream_id_c2 = {
    .id = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11}};

int
main(int argc, char** argv)
{
    struct jbpf_codeletset_load_req codeletset_req_c1 = {0};
    struct jbpf_codeletset_unload_req codeletset_unload_req_c1 = {0};
    const char* jbpf_path = getenv("JBPF_PATH");
    struct jbpf_config config = {0};

    jbpf_set_default_config_options(&config);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    __assert__(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Load the codeletset with codelet C1 in hook "test1"

    // The name of the codeletset
    strcpy(codeletset_req_c1.codeletset_id.name, "linked_map_codeletset_Names_maxLen");

    // We have only one codelet in this codeletset
    codeletset_req_c1.num_codelet_descriptors = 2;

    // codelet 1

    codeletset_req_c1.codelet_descriptor[0].num_in_io_channel = 1;

    // set the channel name to the maximum length
    snprintf(
        codeletset_req_c1.codelet_descriptor[0].in_io_channel[0].name,
        JBPF_MAP_NAME_LEN,
        "input_map_"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcd"
        "ef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789ab"
        "cdef0123456789abcdef01234");

    memcpy(&codeletset_req_c1.codelet_descriptor[0].in_io_channel[0].stream_id, &stream_id_c1, JBPF_STREAM_ID_LEN);

    codeletset_req_c1.codelet_descriptor[0].in_io_channel[0].has_serde = false;

    codeletset_req_c1.codelet_descriptor[0].num_out_io_channel = 0;
    codeletset_req_c1.codelet_descriptor[0].num_linked_maps = 0;

    // The path of the codelet
    __assert__(jbpf_path != NULL);
    snprintf(
        codeletset_req_c1.codelet_descriptor[0].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/simple_input_shared_maxLen/simple_input_shared_maxLen.o",
        jbpf_path);
    strcpy(codeletset_req_c1.codelet_descriptor[0].codelet_name, "simple_input_shared_maxLen");
    strcpy(codeletset_req_c1.codelet_descriptor[0].hook_name, "test1");

    // codelet 2

    codeletset_req_c1.codelet_descriptor[1].num_out_io_channel = 1;

    // set the channel name to the maximum length
    snprintf(
        codeletset_req_c1.codelet_descriptor[1].out_io_channel[0].name,
        JBPF_MAP_NAME_LEN,
        "output_map_"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcd"
        "ef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789ab"
        "cdef0123456789abcdef0123");

    memcpy(&codeletset_req_c1.codelet_descriptor[1].out_io_channel[0].stream_id, &stream_id_c2, JBPF_STREAM_ID_LEN);

    codeletset_req_c1.codelet_descriptor[1].out_io_channel[0].has_serde = false;

    codeletset_req_c1.codelet_descriptor[1].num_in_io_channel = 0;

    codeletset_req_c1.codelet_descriptor[1].num_linked_maps = 1;

    // set the channel name to the maximum length
    snprintf(
        codeletset_req_c1.codelet_descriptor[1].linked_maps[0].map_name,
        JBPF_MAP_NAME_LEN,
        "shared_map_output_"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcd"
        "ef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789ab"
        "cdef0123456789abc");
    strcpy(codeletset_req_c1.codelet_descriptor[1].linked_maps[0].linked_codelet_name, "simple_input_shared_maxLen");
    // set the channel name to the maximum length
    snprintf(
        codeletset_req_c1.codelet_descriptor[1].linked_maps[0].linked_map_name,
        JBPF_MAP_NAME_LEN,
        "shared_map_"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcd"
        "ef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789ab"
        "cdef0123456789abcdef0123");

    // The path of the codelet
    __assert__(jbpf_path != NULL);
    snprintf(
        codeletset_req_c1.codelet_descriptor[1].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/simple_output_shared_maxLen/simple_output_shared_maxLen.o",
        jbpf_path);

    strcpy(codeletset_req_c1.codelet_descriptor[1].codelet_name, "simple_output_shared_maxLen");
    strcpy(codeletset_req_c1.codelet_descriptor[1].hook_name, "test1");

    // Load the codeletset
    __assert__(jbpf_codeletset_load(&codeletset_req_c1, NULL) == JBPF_CODELET_LOAD_SUCCESS);

    // Unload the codeletsets
    strcpy(codeletset_unload_req_c1.codeletset_id.name, "linked_map_codeletset_Names_maxLen");
    __assert__(jbpf_codeletset_unload(&codeletset_unload_req_c1, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");
    return 0;
}