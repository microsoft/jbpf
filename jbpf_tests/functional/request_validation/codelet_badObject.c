/*
 * The purpose of this test is to ensure that an attempt to load a codelet with bad object is handled gracefully.
 *
 * This test does the following:
 * 1. It creates the codeletset with a single codelet.
 * 2. Np input/linked/linked maps are defined.  The codelet uses codelets/bad_object/bad_object.o which is a file of
 * random garbage.
 * 3. It asserts that the return code from the jbpf_codeletset_load call is JBPF_CODELET_CREATION_FAIL.
 */

#include <assert.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

int
main(int argc, char** argv)
{
    struct jbpf_codeletset_load_req codeletset_req_c1 = {0};
    const char* jbpf_path = getenv("JBPF_PATH");
    struct jbpf_config config = {0};
    jbpf_codeletset_load_error_s err_msg = {0};
    jbpf_set_default_config_options(&config);

    JBPF_UNUSED(err_msg);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    assert(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Load the codeletset with codelet C1 in hook "test1"

    // The name of the codeletset
    strcpy(codeletset_req_c1.codeletset_id.name, "bad_object_codeletset");

    // We have only one codelet in this codeletset
    codeletset_req_c1.num_codelet_descriptors = 1;

    codeletset_req_c1.codelet_descriptor[0].num_in_io_channel = 0;
    codeletset_req_c1.codelet_descriptor[0].num_out_io_channel = 0;
    codeletset_req_c1.codelet_descriptor[0].num_linked_maps = 0;

    // The path of the codelet
    if (!jbpf_path) {
        snprintf(
            codeletset_req_c1.codelet_descriptor[0].codelet_path,
            JBPF_PATH_LEN,
            "../../jbpf_tests/test_files/codelets/bad_object/bad_object.o");
    } else {
        snprintf(
            codeletset_req_c1.codelet_descriptor[0].codelet_path,
            JBPF_PATH_LEN,
            "%s/jbpf_tests/test_files/codelets/bad_object/bad_object.o",
            jbpf_path);
    }
    strcpy(codeletset_req_c1.codelet_descriptor[0].codelet_name, "bad_object_codeletset");
    strcpy(codeletset_req_c1.codelet_descriptor[0].hook_name, "test1");

    assert(jbpf_codeletset_load(&codeletset_req_c1, &err_msg) == JBPF_CODELET_CREATION_FAIL);
    assert(strlen(err_msg.err_msg) > 0);

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");

    return 0;
}
