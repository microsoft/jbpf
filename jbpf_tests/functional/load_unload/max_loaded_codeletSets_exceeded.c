/*
 * The purpose of this test is to ensure that an attempt to load too many codeletSets will fail.
 *
 * This test does the following:
 * 1. It loads 65 codeletSets, wiith each codeletSet having one codelet.
 * 2. The code asserts that the first 64 codeletSets are loaded successfully and the 65th codeletSet fails to load.
 *    The code also checks the jbpf context stats to be sure that loaded maps and codelets are cleand up properly in
 * this case.
 * 3. The code then unloads all 65 codeletSets.  It asserts that the first 64 codeletSets are unloaded successfully and
 *    the 65th codeletSet fails to unload.
 * 4.  The jbpf context stats are checked to ensure that all codelets and maps counts are zero at the end of the test.
 */

#include <assert.h>
#include <semaphore.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "jbpf_int.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

#define NUM_CODELET_SETS (JBPF_MAX_LOADED_CODELETSETS + 1) // PLus 1 to exceed max codelets limit
#define NUM_CODELETS_IN_CODELETSET (1)
#define NUM_OUT_CHANNELS_PER_CODELET (1)
#define NUM_MAPS_PER_CODELET (2) // the simple_output_shared codelet has 2 maps

jbpf_io_stream_id_t stream_ids[NUM_CODELET_SETS * NUM_CODELETS_IN_CODELETSET * NUM_OUT_CHANNELS_PER_CODELET] = {0};

int
main(int argc, char** argv)
{
    uint16_t codset = 0;
    const char* jbpf_path = getenv("JBPF_PATH");
    struct jbpf_config config = {0};

    jbpf_set_default_config_options(&config);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    assert(jbpf_init(&config) == 0);

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
        // printf("Loading codletset %s\n", codset_req->codeletset_id.name);
        if (codset == NUM_CODELET_SETS - 1) {
            assert(jbpf_codeletset_load(codset_req, NULL) == JBPF_CODELET_CREATION_FAIL);
        } else {
            assert(jbpf_codeletset_load(codset_req, NULL) == JBPF_CODELET_LOAD_SUCCESS);
        }

        // check the stats of the jbpf_ctx
        struct jbpf_ctx_t* jbpf_ctx = jbpf_get_ctx();
        JBPF_UNUSED(jbpf_ctx);
        assert(jbpf_ctx != NULL);
        // printf(
        //     "jbpf_ctx after loading %s : num_codeletSets = %ld nmap_reg %d total_num_codelets %d \n",
        //     codset_req->codeletset_id.name,
        //     ck_ht_count(&jbpf_ctx->codeletset_registry),
        //     jbpf_ctx->nmap_reg,
        //     jbpf_ctx->total_num_codelets);
        int expected_num_loaded_codeletSets = (codset == NUM_CODELET_SETS - 1) ? codset : codset + 1;
        JBPF_UNUSED(expected_num_loaded_codeletSets);
        assert(ck_ht_count(&jbpf_ctx->codeletset_registry) == expected_num_loaded_codeletSets);
        assert(jbpf_ctx->total_num_codelets == expected_num_loaded_codeletSets * NUM_CODELETS_IN_CODELETSET);
        assert(
            jbpf_ctx->nmap_reg == expected_num_loaded_codeletSets * NUM_CODELETS_IN_CODELETSET * NUM_MAPS_PER_CODELET);
    }

    usleep(1000000);

    // unload the codeletSets
    for (codset = 0; codset < NUM_CODELET_SETS; codset++) {
        jbpf_codeletset_unload_req_s codeletset_unload_req;
        jbpf_codeletset_unload_req_s* codset_unload_req = &codeletset_unload_req;
        snprintf(codset_unload_req->codeletset_id.name, JBPF_CODELETSET_NAME_LEN, "simple_output_codeletset%d", codset);
        // printf("Unloading codeletset %s\n", codset_unload_req->codeletset_id.name);
        if (codset == NUM_CODELET_SETS - 1) {
            assert(jbpf_codeletset_unload(codset_unload_req, NULL) == JBPF_CODELET_UNLOAD_FAIL);
        } else {
            assert(jbpf_codeletset_unload(codset_unload_req, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);
        }
    }

    // check the stats of the jbpf_ctx
    struct jbpf_ctx_t* jbpf_ctx = jbpf_get_ctx();
    JBPF_UNUSED(jbpf_ctx);
    assert(jbpf_ctx != NULL);
    // printf(
    //     "jbpf_ctx after unloading all codelets : num_codeletSets = %ld nmap_reg %d total_num_codelets %d \n",
    //     ck_ht_count(&jbpf_ctx->codeletset_registry),
    //     jbpf_ctx->nmap_reg,
    //     jbpf_ctx->total_num_codelets);
    assert(ck_ht_count(&jbpf_ctx->codeletset_registry) == 0);
    assert(jbpf_ctx->total_num_codelets == 0);
    assert(jbpf_ctx->nmap_reg == 0);

    // Cleanup and stop
    jbpf_stop();

    printf("Test completed successfully\n");
    return 0;
}
