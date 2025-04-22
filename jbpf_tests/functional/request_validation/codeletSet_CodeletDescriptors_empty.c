/*
 * The purpose of this test is to ensure that at least 1 codelet is given, it is captured during validation
 * checks.
 *
 * This test does the following:
 * 1. It pupulates a codeletset with 0 codelets.
 * 2. It asserts that the return code from the jbpf_codeletset_load call is JBPF_CODELET_PARAM_INVALID).
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
    struct jbpf_config config = {0};
    jbpf_codeletset_load_error_s err_msg = {0};

    JBPF_UNUSED(err_msg);

    jbpf_set_default_config_options(&config);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    int res = jbpf_init(&config);
    assert(res == 0);
#ifdef NDEBUG
    (void)res; // suppress unused-variable warning in release mode
#endif

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // The name of the codeletset
    strcpy(codeletset_req_c1.codeletset_id.name, "simple_output_codeletset");

    // set num to zero to indicate zero codelets
    codeletset_req_c1.num_codelet_descriptors = 0;

    // Load the codeletset
    res = jbpf_codeletset_load(&codeletset_req_c1, &err_msg);
    assert(res == JBPF_CODELET_PARAM_INVALID);

    assert(strlen(err_msg.err_msg) > 0);

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");
    return 0;
}
