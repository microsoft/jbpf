/*
 * The purpose of this test is to ensure that if too many input channels are configured, it is captured during
 * validation checks.
 *
 * This test does the following:
 * 1. It creates the config for a codeletset with a single codelet (C1).
 * 2. It populates the JBPF_MAX_IO_CHANNEL number of in_io_channel.
 * 3. It asserts sets num_in_io_channel to "JBPF_MAX_IO_CHANNEL + 1" i.e. configuring too many.
 * 4. It asserts that the return code from the jbpf_codeletset_load call is JBPF_CODELET_PARAM_INVALID).
 */

#include <assert.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "jbpf_utils.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"
#include "jbpf_test_lib.h"

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

    __assert__(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Load the codeletset with codelet C1 in hook "test1"

    // The name of the codeletset
    strcpy(codeletset_req_c1.codeletset_id.name, "max_input_shared_codeletset");

    // We have only one codelet in this codeletset
    codeletset_req_c1.num_codelet_descriptors = 1;

    codeletset_req_c1.codelet_descriptor[0].num_in_io_channel = JBPF_MAX_IO_CHANNEL + 1;

    for (int inCh = 0; inCh < JBPF_MAX_IO_CHANNEL; inCh++) {
        // The name of the input map that corresponds to the input channel of the codelet
        jbpf_io_channel_name_t ch_name = {0};
        snprintf(ch_name, JBPF_IO_CHANNEL_NAME_LEN, "input_map%d", inCh);
        strcpy(codeletset_req_c1.codelet_descriptor[0].in_io_channel[inCh].name, ch_name);

        // Link the map to a stream id
        stream_id_c1.id[(JBPF_STREAM_ID_LEN - 1)] = (uint8_t)inCh;
        memcpy(
            &codeletset_req_c1.codelet_descriptor[0].in_io_channel[inCh].stream_id, &stream_id_c1, JBPF_STREAM_ID_LEN);
        // The input channel of the codelet does not have a serializer
        codeletset_req_c1.codelet_descriptor[0].in_io_channel[inCh].has_serde = false;
    }
    codeletset_req_c1.codelet_descriptor[0].num_out_io_channel = 0;
    codeletset_req_c1.codelet_descriptor[0].num_linked_maps = 0;

    // The path of the codelet
    __assert__(jbpf_path != NULL);
    snprintf(
        codeletset_req_c1.codelet_descriptor[0].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/max_input_shared/max_input_shared.o",
        jbpf_path);
    strcpy(codeletset_req_c1.codelet_descriptor[0].codelet_name, "max_input_shared");
    strcpy(codeletset_req_c1.codelet_descriptor[0].hook_name, "test1");

    // Load the codeletset
    __assert__(jbpf_codeletset_load(&codeletset_req_c1, &err_msg) == JBPF_CODELET_PARAM_INVALID);
    assert(strlen(err_msg.err_msg) > 0);

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");
    return 0;
}
