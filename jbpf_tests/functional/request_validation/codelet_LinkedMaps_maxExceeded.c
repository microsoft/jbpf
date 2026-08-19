/*
 * The purpose of this test is to ensure that if too many linked maps are configured, it is captured during validation
 * checks.
 *
 * This test does the following:
 * 1. It loads a codeletset with two codelets (C1) .
 * 2. Codelet 1 is configured with file codelets/max_input_shared/max_input_shared.o, which has JBPF_MAX_IO_CHANNEL (5)
 * shared maps.
 * 3. Codelet 2 is populated with JBPF_MAX_LINKED_MAPS (5) linked maps, and these are linked to codelet 1 shared_map0
 * .. shared_map4.
 * 4. Codelet 2 is explicitly configured with num_linked_maps = "JBPF_MAX_LINKED_MAPS + 1", i.e.  too many.
 * 5. It asserts that the return code from the jbpf_codeletset_load call is JBPF_CODELET_PARAM_INVALID).
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
    const char* jbpf_path = getenv("JBPF_PATH");
    struct jbpf_config config = {0};
    jbpf_codeletset_load_error_s err_msg = {0};

    JBPF_UNUSED(err_msg);

    jbpf_set_default_config_options(&config);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    assert(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Load the codeletset with codelet C1 in hook "test1"

    // The name of the codeletset
    strcpy(codeletset_req_c1.codeletset_id.name, "max_linked_map_codeletset");

    // We have only one codelet in this codeletset
    codeletset_req_c1.num_codelet_descriptors = 2;

    // codelet 1

    codeletset_req_c1.codelet_descriptor[0].num_in_io_channel = JBPF_MAX_IO_CHANNEL;

    for (int inCh = 0; inCh < codeletset_req_c1.codelet_descriptor[0].num_in_io_channel; inCh++) {
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
    assert(jbpf_path != NULL);
    snprintf(
        codeletset_req_c1.codelet_descriptor[0].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/max_input_shared/max_input_shared.o",
        jbpf_path);
    strcpy(codeletset_req_c1.codelet_descriptor[0].codelet_name, "max_input_shared");
    strcpy(codeletset_req_c1.codelet_descriptor[0].hook_name, "test1");

    // codelet 2

    codeletset_req_c1.codelet_descriptor[1].num_out_io_channel = JBPF_MAX_IO_CHANNEL;

    for (int outCh = 0; outCh < codeletset_req_c1.codelet_descriptor[1].num_out_io_channel; outCh++) {
        // The name of the output map that corresponds to the output channel of the codelet
        jbpf_io_channel_name_t ch_name = {0};
        snprintf(ch_name, JBPF_IO_CHANNEL_NAME_LEN, "output_map%d", outCh);
        strcpy(codeletset_req_c1.codelet_descriptor[1].out_io_channel[outCh].name, ch_name);

        // Link the map to a stream id
        stream_id_c1.id[(JBPF_STREAM_ID_LEN - 1)] = (uint8_t)outCh;
        memcpy(
            &codeletset_req_c1.codelet_descriptor[1].out_io_channel[outCh].stream_id,
            &stream_id_c1,
            JBPF_STREAM_ID_LEN);
        // The output channel of the codelet does not have a serializer
        codeletset_req_c1.codelet_descriptor[1].out_io_channel[outCh].has_serde = false;
    }
    codeletset_req_c1.codelet_descriptor[1].num_in_io_channel = 0;

    codeletset_req_c1.codelet_descriptor[1].num_linked_maps = JBPF_MAX_LINKED_MAPS + 1;
    for (uint8_t m = 0; m < JBPF_MAX_LINKED_MAPS; m++) {

        // The name of the linked map as it appears in C3
        char map_name[JBPF_MAP_NAME_LEN];
        snprintf(map_name, JBPF_MAP_NAME_LEN, "shared_map_output%d", m);
        strcpy(codeletset_req_c1.codelet_descriptor[1].linked_maps[m].map_name, map_name);

        // The codelet that contains the map that this map will be linked to
        strcpy(codeletset_req_c1.codelet_descriptor[1].linked_maps[m].linked_codelet_name, "max_input_shared");

        // The name of the linked map as it appears in C2
        char linked_map_name[JBPF_MAP_NAME_LEN];
        snprintf(linked_map_name, JBPF_MAP_NAME_LEN, "shared_map%d", m);
        strcpy(codeletset_req_c1.codelet_descriptor[1].linked_maps[m].linked_map_name, linked_map_name);
    }

    // The path of the codelet
    assert(jbpf_path != NULL);
    snprintf(
        codeletset_req_c1.codelet_descriptor[1].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/max_output_shared/max_output_shared.o",
        jbpf_path);

    strcpy(codeletset_req_c1.codelet_descriptor[1].codelet_name, "max_output_shared");
    strcpy(codeletset_req_c1.codelet_descriptor[1].hook_name, "test1");

    // Load the codeletset
    assert(jbpf_codeletset_load(&codeletset_req_c1, &err_msg) == JBPF_CODELET_PARAM_INVALID);
    assert(strlen(err_msg.err_msg) > 0);

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");
    return 0;
}
