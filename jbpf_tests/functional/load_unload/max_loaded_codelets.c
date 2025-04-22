/*
 * The purpose of this test is to ensure we can load the maximum number of codelets.
 *
 * This test does the following:
 * 1. It loads 32 codeletSets, with each codeletSet having two codelets.
 * 2. The maximum codelets that can be loaded is 64.  Therefore the code asserts that all the codeletSets are loaded
 * successfully.
 * 3. The code then unloads all the codeletSets.
 */

#include <assert.h>
#include <semaphore.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

#define NUM_CODELET_SETS (JBPF_MAX_LOADED_CODELETSETS / 2)
#define NUM_CODELETS_IN_CODELETSET (2)
#define NUM_OUT_CHANNELS_PER_CODELET (1)

jbpf_io_stream_id_t stream_ids[NUM_CODELET_SETS * NUM_CODELETS_IN_CODELETSET * NUM_OUT_CHANNELS_PER_CODELET] = {0};

int
main(int argc, char** argv)
{
    uint16_t codset = 0;
    const char* jbpf_path = getenv("JBPF_PATH");
    struct jbpf_config config = {0};

    assert(NUM_CODELET_SETS * NUM_CODELETS_IN_CODELETSET == JBPF_MAX_LOADED_CODELETS);

    jbpf_set_default_config_options(&config);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    int res = jbpf_init(&config);
    assert(res == 0);
#ifdef NDEBUG
    (void)res; // suppress unused-variable warning in release mode
#endif

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    for (codset = 0; codset < NUM_CODELET_SETS; codset++) {

        jbpf_codeletset_load_req_s codeletset_req = {0};

        jbpf_codeletset_load_req_s* codset_req = &codeletset_req;

        // The name of the codeletset
        snprintf(codset_req->codeletset_id.name, JBPF_CODELETSET_NAME_LEN, "simple_output_codeletset%d", codset);

        codset_req->num_codelet_descriptors = NUM_CODELETS_IN_CODELETSET;

        for (uint16_t cod = 0; cod < codset_req->num_codelet_descriptors; cod++) {

            jbpf_codelet_descriptor_s* cod_desc = &codset_req->codelet_descriptor[cod];

            cod_desc->num_out_io_channel = NUM_OUT_CHANNELS_PER_CODELET;

            for (int outCh = 0; outCh < cod_desc->num_out_io_channel; outCh++) {

                // The name of the output map that corresponds to the output channel of the codelet
                strcpy(cod_desc->out_io_channel[outCh].name, "output_map");

                // Link the map to a stream id
                uint16_t stream_id = (codset * NUM_CODELETS_IN_CODELETSET * NUM_OUT_CHANNELS_PER_CODELET) +
                                     (cod * NUM_OUT_CHANNELS_PER_CODELET) + outCh;
                // printf("stream_id = %d codeset = %d cod = %d outCh = %d\n", stream_id, codset, cod, outCh);
                uint8_t* stream_id_ptr = (uint8_t*)&stream_id;
                for (int b = 0; b < JBPF_STREAM_ID_LEN / 2; b++) {
                    stream_ids[stream_id].id[b * 2] = stream_id_ptr[0];
                    stream_ids[stream_id].id[b * 2 + 1] = stream_id_ptr[1];
                }

                memcpy(&cod_desc->out_io_channel[outCh].stream_id, &stream_ids[stream_id], JBPF_STREAM_ID_LEN);

                // The output channel of the codelet does not have a serializer
                cod_desc->out_io_channel[outCh].has_serde = false;
            }

            cod_desc->num_in_io_channel = 0;
            cod_desc->num_linked_maps = 0;

            // The path of the codelet
            assert(jbpf_path != NULL);
            snprintf(
                cod_desc->codelet_path,
                JBPF_PATH_LEN,
                "%s/jbpf_tests/test_files/codelets/simple_output_shared/simple_output_shared.o",
                jbpf_path);
            snprintf(cod_desc->codelet_name, JBPF_CODELET_NAME_LEN, "simple_output_shared%d", cod);
            strcpy(cod_desc->hook_name, "test1");
        }

        // Load the codeletsets
        // printf("Loading codeletset %s\n", codset_req->codeletset_id.name);
        res = jbpf_codeletset_load(codset_req, NULL);
        assert(res == JBPF_CODELET_LOAD_SUCCESS);
    }

    usleep(1000000);

    // unload the codeletSets
    for (codset = 0; codset < NUM_CODELET_SETS; codset++) {
        jbpf_codeletset_unload_req_s codeletset_unload_req;
        jbpf_codeletset_unload_req_s* codset_unload_req = &codeletset_unload_req;
        snprintf(codset_unload_req->codeletset_id.name, JBPF_CODELETSET_NAME_LEN, "simple_output_codeletset%d", codset);
        // printf("Unloading codeletset %s\n", codset_unload_req->codeletset_id.name);
        assert(jbpf_codeletset_unload(codset_unload_req, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);
    }

    // Cleanup and stop
    jbpf_stop();

    printf("Test completed successfully\n");
    return 0;
}
