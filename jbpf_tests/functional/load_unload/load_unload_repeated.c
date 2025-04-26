/*
 * The purpose of this test is to ensure that if codeletSets are loaded and unloaded repeatedly that there are no
 * failures, and that the underlying jbpf context stats have expected values.
 *
 * This test does the following:
 * 1. It loads 64 codeletSets.
 * 2. It repeatedly unloads 8 and loads 8.
 * 3. It finally unlaods all 64 which should be loaded.
 * 4. At each stage it asserst the load/unload is succesful, and that the jbpf context stats are chcked for correct
 * values.
 */

#include <assert.h>
#include <semaphore.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "jbpf_int.h"
#include "jbpf_utils.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"
#include "jbpf_test_lib.h"

#define NUM_CODELET_SETS (JBPF_MAX_LOADED_CODELETSETS)
#define NUM_CODELETS_IN_CODELETSET (1)
#define NUM_OUT_CHANNELS_PER_CODELET (1)

jbpf_io_stream_id_t stream_ids[NUM_CODELET_SETS * NUM_CODELETS_IN_CODELETSET * NUM_OUT_CHANNELS_PER_CODELET] = {0};

static void
load_codelet_set(jbpf_codeletset_load_req_s* codset_req, int codset)
{

    const char* jbpf_path = getenv("JBPF_PATH");

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
        __assert__(jbpf_path != NULL);
        snprintf(
            cod_desc->codelet_path,
            JBPF_PATH_LEN,
            "%s/jbpf_tests/test_files/codelets/simple_output_shared/simple_output_shared.o",
            jbpf_path);
        snprintf(cod_desc->codelet_name, JBPF_CODELET_NAME_LEN, "simple_output_shared%d", cod);
        strcpy(cod_desc->hook_name, "test1");
    }

    // Load the codeletsets
    printf("Loading codeletset %s\n", codset_req->codeletset_id.name);
    __assert__(jbpf_codeletset_load(codset_req, NULL) == JBPF_CODELET_LOAD_SUCCESS);
}

int
main(int argc, char** argv)
{
    uint16_t codset = 0;
    struct jbpf_config config = {0};
    int total_loaded = 0;
    int total_unloaded = 0;

    jbpf_set_default_config_options(&config);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    __assert__(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    for (codset = 0; codset < NUM_CODELET_SETS; codset++) {
        jbpf_codeletset_load_req_s codeletset_req = {0};
        load_codelet_set(&codeletset_req, codset);
        total_loaded++;
    }

    // in a loop, delete 8 and add 8 repeatedly
    assert(NUM_CODELET_SETS % 8 == 0);
    int codset_id = 0;

    struct jbpf_ctx_t* jbpf_ctx = jbpf_get_ctx();
    __assert__(jbpf_ctx != NULL);
    assert(ck_ht_count(&jbpf_ctx->codeletset_registry) == 64);
    __assert__(jbpf_ctx->total_num_codelets == 64);
    __assert__(jbpf_ctx->nmap_reg == 128);

    for (int i = 0; i < 1000; i++) {

        // unload 8 codeletSets
        for (int j = 0; j < 8; j++) {
            int cs_id = codset_id + j;
            if (cs_id >= NUM_CODELET_SETS) {
                cs_id = 0;
            }

            struct jbpf_ctx_t jbpf_ctx_old = *jbpf_ctx;
            JBPF_UNUSED(jbpf_ctx_old);
            int old_codeletset_registry_count = ck_ht_count(&jbpf_ctx->codeletset_registry);
            JBPF_UNUSED(old_codeletset_registry_count);

            jbpf_codeletset_unload_req_s codeletset_unload_req;
            jbpf_codeletset_unload_req_s* codset_unload_req = &codeletset_unload_req;
            snprintf(
                codset_unload_req->codeletset_id.name, JBPF_CODELETSET_NAME_LEN, "simple_output_codeletset%d", cs_id);
            printf("Unloading codeletset %s\n", codset_unload_req->codeletset_id.name);
            __assert__(jbpf_codeletset_unload(codset_unload_req, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);
            total_unloaded++;

            assert(ck_ht_count(&jbpf_ctx->codeletset_registry) == old_codeletset_registry_count - 1);
            __assert__(jbpf_ctx->total_num_codelets == jbpf_ctx_old.total_num_codelets - 1);
            __assert__(jbpf_ctx->nmap_reg == jbpf_ctx_old.nmap_reg - 2);
        }

        // load 8 codeletSets
        for (int j = 0; j < 8; j++) {
            int cs_id = codset_id + j;
            if (cs_id >= NUM_CODELET_SETS) {
                cs_id = 0;
            }
            jbpf_codeletset_load_req_s codeletset_req = {0};
            struct jbpf_ctx_t jbpf_ctx_old = *jbpf_ctx;
            JBPF_UNUSED(jbpf_ctx_old);
            int old_codeletset_registry_count = ck_ht_count(&jbpf_ctx->codeletset_registry);
            JBPF_UNUSED(old_codeletset_registry_count);
            load_codelet_set(&codeletset_req, cs_id);
            total_loaded++;
            assert(ck_ht_count(&jbpf_ctx->codeletset_registry) == old_codeletset_registry_count + 1);
            __assert__(jbpf_ctx->total_num_codelets == jbpf_ctx_old.total_num_codelets + 1);
            __assert__(jbpf_ctx->nmap_reg == jbpf_ctx_old.nmap_reg + 2);
        }

        codset_id += 8;
        if (codset_id >= NUM_CODELET_SETS) {
            codset_id = 0;
        }
    }

    // unload 64 codeletSets
    for (int j = 0; j < 64; j++) {

        int cs_id = codset_id + j;
        if (cs_id >= NUM_CODELET_SETS) {
            cs_id = 0;
        }
        jbpf_codeletset_unload_req_s codeletset_unload_req;
        jbpf_codeletset_unload_req_s* codset_unload_req = &codeletset_unload_req;
        snprintf(codset_unload_req->codeletset_id.name, JBPF_CODELETSET_NAME_LEN, "simple_output_codeletset%d", cs_id);
        printf("Unloading codeletset %s\n", codset_unload_req->codeletset_id.name);
        struct jbpf_ctx_t jbpf_ctx_old = *jbpf_ctx;
        int old_codeletset_registry_count = ck_ht_count(&jbpf_ctx->codeletset_registry);
        JBPF_UNUSED(jbpf_ctx_old);
        JBPF_UNUSED(old_codeletset_registry_count);
        __assert__(jbpf_codeletset_unload(codset_unload_req, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);
        total_unloaded++;
        assert(ck_ht_count(&jbpf_ctx->codeletset_registry) == old_codeletset_registry_count - 1);
        __assert__(jbpf_ctx->total_num_codelets == jbpf_ctx_old.total_num_codelets - 1);
        __assert__(jbpf_ctx->nmap_reg == jbpf_ctx_old.nmap_reg - 2);
    }

    printf("Total codeletsets loaded %d unloaded %d\n", total_loaded, total_unloaded);

    // check the stats of the jbpf_ctx
    jbpf_ctx = jbpf_get_ctx();
    __assert__(jbpf_ctx != NULL);
    // printf(
    //     "jbpf_ctx after unloading all codelets : num_codeletSets = %ld nmap_reg %d total_num_codelets %d \n",
    //     ck_ht_count(&jbpf_ctx->codeletset_registry),
    //     jbpf_ctx->nmap_reg,
    //     jbpf_ctx->total_num_codelets);
    assert(ck_ht_count(&jbpf_ctx->codeletset_registry) == 0);
    __assert__(jbpf_ctx->total_num_codelets == 0);
    __assert__(jbpf_ctx->nmap_reg == 0);

    // Cleanup and stop
    jbpf_stop();

    printf("Test completed successfully\n");
    return 0;
}
