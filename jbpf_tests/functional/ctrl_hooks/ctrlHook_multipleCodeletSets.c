/*
 * The purpose of this test is to ensure that only one codelet can be added to a control hook.
 *
 * This test does the following:
 * 1. It attempts to load 2 codeletSets, where each codeletSet has 1 codelet.
 * 2. It asserts that the return code from the load of the 1st codeletSet is JBPF_CODELET_LOAD_SUCCESS, and the 2nd is
 * JBPF_CODELET_LOAD_FAIL.
 * 3. It tries to unload the codeletSets.  It asserts that th3 1st codeletSet unload request returns
 * JBPF_CODELET_UNLOAD_SUCCESS, and the 2nd returns JBPF_CODELET_UNLOAD_FAIL.
 */

#include <assert.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

#define NUM_CODELET_SETS (2)
#define NUM_CODELETS_IN_CODELETSET (1)

jbpf_io_stream_id_t out_stream_id = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};
jbpf_io_stream_id_t in_stream_id = {
    .id = {0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00}};

int
main(int argc, char** argv)
{
    uint16_t codset = 0;
    // struct jbpf_codeletset_unload_req codeletset_unload_req_c1 = {0};
    const char* jbpf_path = getenv("JBPF_PATH");
    struct jbpf_config config = {0};
    jbpf_codeletset_load_error_s err_msg = {0};

    jbpf_set_default_config_options(&config);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    assert(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    for (codset = 0; codset < NUM_CODELET_SETS; codset++) {

        jbpf_codeletset_load_req_s codeletset_req = {0};

        jbpf_codeletset_load_req_s* codset_req = &codeletset_req;

        // Load the codeletset with codelet C1 in hook "test1"

        // The name of the codeletset
        snprintf(
            codset_req->codeletset_id.name,
            JBPF_CODELETSET_NAME_LEN,
            "control-input-withFailures_codeletset-%d",
            codset);

        // Create max possible codelets
        codset_req->num_codelet_descriptors = NUM_CODELETS_IN_CODELETSET;

        for (uint16_t cod = 0; cod < codset_req->num_codelet_descriptors; cod++) {

            jbpf_codelet_descriptor_s* cod_desc = &codset_req->codelet_descriptor[cod];

            // The codelet has just one input channel and no shared maps
            cod_desc->num_in_io_channel = 1;
            // The name of the input map that corresponds to the input channel of the codelet
            strcpy(cod_desc->in_io_channel[0].name, "input_map");
            // Link the map to a stream id
            in_stream_id.id[(JBPF_STREAM_ID_LEN - 1)] = (uint8_t)((codset * NUM_CODELETS_IN_CODELETSET) + cod);
            memcpy(&cod_desc->in_io_channel[0].stream_id, &in_stream_id, JBPF_STREAM_ID_LEN);
            // The input channel of the codelet does not have a serializer
            cod_desc->in_io_channel[0].has_serde = false;
            cod_desc->num_linked_maps = 0;

            cod_desc->num_out_io_channel = 1;
            // The name of the output map that corresponds to the output channel of the codelet
            strcpy(cod_desc->out_io_channel[0].name, "output_map");
            // Link the map to a stream id
            out_stream_id.id[(JBPF_STREAM_ID_LEN - 1)] = (uint8_t)((codset * NUM_CODELETS_IN_CODELETSET) + cod);
            memcpy(&cod_desc->out_io_channel[0].stream_id, &out_stream_id, JBPF_STREAM_ID_LEN);
            // The output channel of the codelet does not have a serializer
            cod_desc->out_io_channel[0].has_serde = false;
            cod_desc->num_linked_maps = 0;

            // The path of the codelet
            assert(jbpf_path != NULL);
            snprintf(
                cod_desc->codelet_path,
                JBPF_PATH_LEN,
                "%s/jbpf_tests/test_files/codelets/codelet-control-input-withFailures/"
                "codelet-control-input-withFailures.o",
                jbpf_path);

            snprintf(cod_desc->codelet_name, JBPF_CODELET_NAME_LEN, "control-input-withFailures_%d", cod);
            strcpy(cod_desc->hook_name, "ctrl_test0");
        }

        if (codset == 0) {
            assert(jbpf_codeletset_load(codset_req, NULL) == JBPF_CODELET_LOAD_SUCCESS);
        } else {
            assert(jbpf_codeletset_load(codset_req, &err_msg) == JBPF_CODELET_LOAD_FAIL);
            printf("err_msg = %s", err_msg.err_msg);
        }
    }

    // unload the codeletSets
    for (codset = 0; codset < NUM_CODELET_SETS; codset++) {
        jbpf_codeletset_unload_req_s codeletset_unload_req;
        jbpf_codeletset_unload_req_s* codset_unload_req = &codeletset_unload_req;
        snprintf(
            codset_unload_req->codeletset_id.name,
            JBPF_CODELETSET_NAME_LEN,
            "control-input-withFailures_codeletset-%d",
            codset);
        if (codset == 0) {
            assert(jbpf_codeletset_unload(codset_unload_req, &err_msg) == JBPF_CODELET_UNLOAD_SUCCESS);
        } else {
            assert(jbpf_codeletset_unload(codset_unload_req, &err_msg) == JBPF_CODELET_UNLOAD_FAIL);
            assert(strlen(err_msg.err_msg) > 0);
            printf("err_msg = %s", err_msg.err_msg);
        }
    }

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");
    return 0;
}
