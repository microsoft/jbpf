/*
 * The purpose of this test is to ensure we can configure linked maps with cyclic codelet.  What that means is, for
 * instance as in this test, if a codleletSet has 3 codelets, codelet1 can linkd to map in codelet3, and codelet3 can
 * link to a map in codelet1.
 *
 * This test does the following:
 * 1. It loads a codeletset with 3 codelets.
 * 2. Codelet 1 links to a map in Codelet 3.
 * 3. Codelet 3 links to a map in Codelet 1.
 * 4  The hook is called numerous times and data checked for correctness.
 * 5. It unloads the codeletSet.
 */

#include <assert.h>
#include <semaphore.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "jbpf_utils.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

#define NUM_CODELETS (3)

#define NUM_ITERATIONS (5)

jbpf_io_stream_id_t stream_ids[NUM_CODELETS] = {0};

sem_t sem;

static uint16_t processed = 0;

static void
io_channel_check_output(jbpf_io_stream_id_t* stream_id, void** bufs, int num_bufs, void* ctx)
{
    int* output;

    for (int i = 0; i < num_bufs; i++) {

        // printf("stream = ");
        // for (int j = 0; j < JBPF_STREAM_ID_LEN; j++) {
        //   printf("%02x", stream_id->id[j]);
        // }
        // printf("\n");

        // get stream idx
        int sidx = -1;
        int stream_id_len = sizeof(stream_ids) / sizeof(stream_ids[0]);
        for (int s = 0; s < stream_id_len; s++) {
            if (memcmp(stream_id, &stream_ids[s], sizeof(jbpf_io_stream_id_t)) == 0) {
                sidx = s;
                break;
            }
        }
        assert(sidx != -1);
        output = bufs[i];
        // printf("#### s=%d buf=%d\n", s, *output);

#define STREAM_COUNT 3
        const int expected_values[STREAM_COUNT][NUM_ITERATIONS * 2] = {
            {1, 1, 12, 12, 122, 122, 1222, 1222, 12222, 12222},
            {1, 11, 11, 111, 111, 1111, 1111, 11111, 1111},
            {2, 2, 22, 22, 222, 222, 2222, 2222, 22222, 22222}};

        // Check the values using assertions
        int it = 0;
        for (it = 0; it < NUM_ITERATIONS * 2; ++it) {
            if (*output == expected_values[sidx][it])
                break;
        }
        assert(it != NUM_ITERATIONS * 2);

        processed++;
    }

    // printf("processed = %d\n", processed);

#define MSGS_PER_ITERATION (2)
#define TOTAL_EXPECTED_MSGS (NUM_ITERATIONS * MSGS_PER_ITERATION * NUM_CODELETS)

    if (processed == TOTAL_EXPECTED_MSGS) {
        sem_post(&sem);
    }
}

int
main(int argc, char** argv)
{

    struct jbpf_codeletset_load_req codeletset_req_c1 = {0};
    struct jbpf_codeletset_unload_req codeletset_unload_req_c1 = {0};
    const char* jbpf_path = getenv("JBPF_PATH");
    struct jbpf_config config = {0};

    JBPF_UNUSED(codeletset_unload_req_c1);

    jbpf_set_default_config_options(&config);

    sem_init(&sem, 0, 0);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    assert(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Register a callback to handle the buffers sent from the codelets
    jbpf_register_io_output_cb(io_channel_check_output);

    // Load the codeletset with codelet C1 in hook "test1"

    // The name of the codeletset
    strcpy(codeletset_req_c1.codeletset_id.name, "linked_map_cyclicCodelets_codeletset");

    // We have only one codelet in this codeletset
    codeletset_req_c1.num_codelet_descriptors = 3;

    for (uint16_t cod = 0; cod < NUM_CODELETS; cod++) {

        jbpf_codelet_descriptor_s* cod_desc = &codeletset_req_c1.codelet_descriptor[cod];

        snprintf(cod_desc->codelet_name, JBPF_CODELET_NAME_LEN, "simple_output_2shared%d", cod);

        cod_desc->priority = UINT_MAX - cod;

        // The path of the codelet
        assert(jbpf_path != NULL);
        snprintf(
            cod_desc->codelet_path,
            JBPF_PATH_LEN,
            "%s/jbpf_tests/test_files/codelets/simple_output_2shared/simple_output_2shared.o",
            jbpf_path);

        strcpy(cod_desc->hook_name, "test1");

        cod_desc->num_in_io_channel = 0;

        cod_desc->num_out_io_channel = 1;

        strcpy(cod_desc->out_io_channel[0].name, "output_map");

        // Link the map to a stream id
        uint16_t stream_id = cod;
        uint8_t* stream_id_ptr = (uint8_t*)&stream_id;
        for (int b = 0; b < JBPF_STREAM_ID_LEN / 2; b++) {
            stream_ids[stream_id].id[b * 2] = stream_id_ptr[0];
            stream_ids[stream_id].id[b * 2 + 1] = stream_id_ptr[1];
        }
        memcpy(&cod_desc->out_io_channel[0].stream_id, &stream_ids[stream_id], JBPF_STREAM_ID_LEN);

        cod_desc->out_io_channel[0].has_serde = false;

        if (cod == 0) {
            cod_desc->num_linked_maps = 1;
            strcpy(cod_desc->linked_maps[0].map_name, "shared_map_output0");
            snprintf(
                cod_desc->linked_maps[0].linked_codelet_name,
                JBPF_CODELET_NAME_LEN,
                "simple_output_2shared%d",
                NUM_CODELETS - 1);
            strcpy(cod_desc->linked_maps[0].linked_map_name, "shared_map_output0");
        } else if (cod == (NUM_CODELETS - 1)) {
            cod_desc->num_linked_maps = 1;
            strcpy(cod_desc->linked_maps[0].map_name, "shared_map_output1");
            snprintf(cod_desc->linked_maps[0].linked_codelet_name, JBPF_CODELET_NAME_LEN, "simple_output_2shared%d", 0);
            strcpy(cod_desc->linked_maps[0].linked_map_name, "shared_map_output1");
        }
    }

    // Load the codeletset
    assert(jbpf_codeletset_load(&codeletset_req_c1, NULL) == JBPF_CODELET_LOAD_SUCCESS);

    // Call the hooks 5 times and check that we got the expected data
    struct packet p = {1, 0};

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        hook_test1(&p, i);
        p.counter_a *= 10;
    }

    // Wait for all the output checks to finish
    sem_wait(&sem);

    // Unload the codeletsets
    codeletset_unload_req_c1.codeletset_id = codeletset_req_c1.codeletset_id;
    assert(jbpf_codeletset_unload(&codeletset_unload_req_c1, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");

    return 0;
}
