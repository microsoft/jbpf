// Copyright (c) Microsoft Corporation. All rights reserved.
/**
 * This codelet tests the fixpoint helper function.
 * It loads the jbpf_helper_fixpoint codeletset and calls the test_single_result hook.
 * Inside the hook call, the codelet will call the helper functions:
 * 1. float_to_fixed
 * 2. fixed_to_float
 * 3. double_to_fixed
 * 4. fixed_to_double
 * The test should expect the data.test_passed value to be 10.0 e.g. 3.0+7.0 = 10.0
 * The test should also expect the data.test_passed_32 and data.test_passed_64 values to be 12.3 and 45.6 respectively.
 * The test is repeated 10000 times.
 */

#include <assert.h>
#include <pthread.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "jbpf_helper_utils.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

jbpf_io_stream_id_t stream_id_c1 = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

#define MAX_ITERATION (10000)

int
main(int argc, char** argv)
{
    struct jbpf_codeletset_load_req codeletset_req_c1 = {0};
    struct jbpf_codeletset_unload_req codeletset_unload_req_c1 = {0};
    const char* jbpf_path = getenv("JBPF_PATH");
    struct jbpf_config config = {0};
    struct test_packet data = {0};

    JBPF_UNUSED(codeletset_unload_req_c1);

    jbpf_set_default_config_options(&config);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    assert(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // The name of the codeletset
    strcpy(codeletset_req_c1.codeletset_id.name, "jbpf_fixpoint_test");

    // Create max possible codelets
    codeletset_req_c1.num_codelet_descriptors = 1;

    // The codelet has just one output channel and no shared maps
    codeletset_req_c1.codelet_descriptor[0].num_in_io_channel = 0;
    codeletset_req_c1.codelet_descriptor[0].num_out_io_channel = 0;

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
        "%s/jbpf_tests/test_files/codelets/helper_function_fixpoint/jbpf_helper_fixpoint.o",
        jbpf_path);
    strcpy(codeletset_req_c1.codelet_descriptor[0].codelet_name, "jbpf-fixpoint-example");

    snprintf(codeletset_req_c1.codelet_descriptor[0].hook_name, JBPF_HOOK_NAME_LEN, "test_single_result");

    // Load the codeletset
    assert(jbpf_codeletset_load(&codeletset_req_c1, NULL) == JBPF_CODELET_LOAD_SUCCESS);

    // iterate the increment operation
    for (int iter = 0; iter < MAX_ITERATION; ++iter) {
        // call the hook to perform the tests in the codelet
        hook_test_single_result(&data, 1);
        // check the result which indicates success (set in the codelet)
        assert(data.test_passed == fixedpt_rconst(10.0));
        assert(abs(data.test_passed_32 - 12.3) < 0.0001);
        assert(abs(data.test_passed_64 - 45.6) < 0.0001);
    }

    // Unload the codeletsets
    codeletset_unload_req_c1.codeletset_id = codeletset_req_c1.codeletset_id;
    assert(jbpf_codeletset_unload(&codeletset_unload_req_c1, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");
    return 0;
}
