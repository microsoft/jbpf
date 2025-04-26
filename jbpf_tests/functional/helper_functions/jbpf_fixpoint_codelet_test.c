// Copyright (c) Microsoft Corporation. All rights reserved.
/**
 * This codelet tests the fixpoint helper function.
 * It loads the jbpf_helper_fixpoint codeletset and calls the test_single_result hook.
 * Inside the hook call, the codelet will call the helper functions:
 * 1. float_to_fixed
 * 2. fixed_to_float
 * 3. double_to_fixed
 * 4. fixed_to_double
 * 5. fixedpt_add
 * 6. fixedpt_sub
 * 7. fixedpt_mul
 * 8. fixedpt_div
 * 9. fixedpt_rconst
 * The values are verified here.
 * The test is repeated 10000 times.
 */

#include <assert.h>
#include <pthread.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "jbpf_helper_utils.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"
#include "jbpf_test_lib.h"
#include <math.h>

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

    __assert__(jbpf_init(&config) == 0);

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
    __assert__(jbpf_path != NULL);
    snprintf(
        codeletset_req_c1.codelet_descriptor[0].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/helper_function_fixpoint/jbpf_helper_fixpoint.o",
        jbpf_path);
    strcpy(codeletset_req_c1.codelet_descriptor[0].codelet_name, "jbpf-fixpoint-example");

    snprintf(codeletset_req_c1.codelet_descriptor[0].hook_name, JBPF_HOOK_NAME_LEN, "test_single_result");

    // Load the codeletset
    __assert__(jbpf_codeletset_load(&codeletset_req_c1, NULL) == JBPF_CODELET_LOAD_SUCCESS);

    // iterate the increment operation
    for (int iter = 0; iter < MAX_ITERATION; ++iter) {
        // call the hook to perform the tests in the codelet
        hook_test_single_result(&data, 1);
        // verify the correctness of the results
        // the values are computed in the codelet
        // Test case 1: convert two floats to fixedpt and add them, then convert back to float
        // 1.23 + 2.34 = 3.57
        assert(fabs(data.test_float_1 - 3.57) < 0.01);
        // Test case 2: convert two doubles to fixedpt and add them, then convert back to double
        // 3.45 + 4.56 = 8.01
        assert(fabs(data.test_double_1 - 8.01) < 0.01);
        // Test case 3: test fixed_to_float
        assert(fabs(data.test_float_2 - 3.14) < 0.01);
        // Test case 4: test fixed_to_double
        assert(fabs(data.test_double_2 - 2.71) < 0.01);
        // Test case 5: test float_to_fixed
        assert(data.test_int_1 == fixedpt_rconst(1.2));
        // Test case 6: test double_to_fixed
        assert(data.test_int64_1 == fixedpt_rconst(2.345));
        // Test case 7: test fixedpt_sub
        // 5.1 - 2.9 = 2.2
        assert(fabs(data.test_float_3 - 2.2) < 0.01);
        // Test case 8: test fixedpt_mul
        // 2.5 * 1.3 = 3.25
        assert(fabs(data.test_float_4 - 3.25) < 0.01);
        // Test case 9: test fixedpt_div
        // 7.5 / 2.5 = 3.0
        assert(fabs(data.test_float_5 - 3.0) < 0.01);
        // Test case 10: test fixedpt_sub for double
        // 5.1 - 2.9 = 2.2
        assert(fabs(data.test_double_3 - 2.2) < 0.01);
        // Test case 11: test fixedpt_mul for double
        // 2.5 * 1.3 = 3.25
        assert(fabs(data.test_double_4 - 3.25) < 0.01);
        // Test case 12: test fixedpt_div for double
        // 7.5 / 2.5 = 3.0
        assert(fabs(data.test_double_5 - 3.0) < 0.01);
        // Test case 13: mixed operations for float
        // (1.23 + 2.34 * 3.45) / 4.56 = 2.04
        assert(fabs(data.test_float_6 - 2.04) < 0.01);
        // Test case 14: mixed operations for double
        // (1.23 + 2.34 * 3.45) / 4.56 = 2.04
        assert(fabs(data.test_double_6 - 2.04) < 0.01);
    }

    // Unload the codeletsets
    codeletset_unload_req_c1.codeletset_id = codeletset_req_c1.codeletset_id;
    __assert__(jbpf_codeletset_unload(&codeletset_unload_req_c1, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");
    return 0;
}
