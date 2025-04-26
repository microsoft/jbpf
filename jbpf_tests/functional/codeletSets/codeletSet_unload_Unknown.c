/*
 * The purpose of this test is to check that a request to unload an unknown codeletSet is safely handled.
 *
 * This test does the following:
 * 1. It loads a codeletSet named "simple_output_codeletset".
 * 2. It sends an unload request for unknown codeletSet "unknown_codeletset" and asserts that the return code is
 * JBPF_CODELET_UNLOAD_FAIL.
 * 3. It sends an unload request for "simple_output_codeletset" and asserts that the return code is
 * JBPF_CODELET_UNLOAD_SUCCESS.
 */

#include <assert.h>
#include <semaphore.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"
#include "jbpf_test_lib.h"

jbpf_io_stream_id_t stream_ids[] = {
    {.id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x00}}};

int
main(int argc, char** argv)
{
    struct jbpf_codeletset_load_req codeletset_load_req = {0};
    struct jbpf_codeletset_unload_req codeletset_unload_req = {0};
    const char* jbpf_path = getenv("JBPF_PATH");
    struct jbpf_config config = {0};

    jbpf_set_default_config_options(&config);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    __assert__(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    struct jbpf_codeletset_load_req* cs_load_req = &codeletset_load_req;

    // Use the same codelet name
    strcpy(cs_load_req->codeletset_id.name, "simple_output_shared_counter_codeletset");

    // Create max possible codelets
    cs_load_req->num_codelet_descriptors = 1;

    // The codelet has just one output channel and no shared maps
    cs_load_req->codelet_descriptor[0].num_in_io_channel = 0;
    cs_load_req->codelet_descriptor[0].num_out_io_channel = 1;

    // set the priority of the codelet
    cs_load_req->codelet_descriptor[0].priority = 0;

    // The name of the output map that corresponds to the output channel of the codelet
    strcpy(cs_load_req->codelet_descriptor[0].out_io_channel[0].name, "output_map");
    // Link the map to a stream id
    memcpy(&cs_load_req->codelet_descriptor[0].out_io_channel[0].stream_id, &stream_ids[0], JBPF_STREAM_ID_LEN);

    // The output channel of the codelet does not have a serializer
    cs_load_req->codelet_descriptor[0].out_io_channel[0].has_serde = false;

    // The path of the codelet
    __assert__(jbpf_path != NULL);
    snprintf(
        cs_load_req->codelet_descriptor[0].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/simple_output/simple_output.o",
        jbpf_path);

    snprintf(cs_load_req->codelet_descriptor[0].codelet_name, JBPF_CODELET_NAME_LEN, "simple_output_shared_counter");
    strcpy(cs_load_req->codelet_descriptor[0].hook_name, "test1");
    cs_load_req->codelet_descriptor[0].num_linked_maps = 0;

    // Load the codeletset
    __assert__(jbpf_codeletset_load(cs_load_req, NULL) == JBPF_CODELET_LOAD_SUCCESS);

    // unload unknown codeletSet
    snprintf(codeletset_unload_req.codeletset_id.name, JBPF_CODELETSET_NAME_LEN, "unknown_codeletset");
    __assert__(jbpf_codeletset_unload(&codeletset_unload_req, NULL) == JBPF_CODELET_UNLOAD_FAIL);

    // unload the correct codeletSet
    codeletset_unload_req.codeletset_id = codeletset_load_req.codeletset_id;
    __assert__(jbpf_codeletset_unload(&codeletset_unload_req, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");
    return 0;
}
