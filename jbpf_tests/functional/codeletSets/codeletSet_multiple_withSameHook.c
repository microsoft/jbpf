/*
 * The purpose of this test is to ensure we can configure the multiple codeletSets wih the same hook.
 *
 * This test does the following:
 * 1. It loads two codeletSets.  Each codeletSet has a single codelet.
 * 2. Each codelet uses the "test1" hook.
 * 3. The hook is called NUM_ITERATIONS times.
 * 4. The code then checks that each stream has 5 outputs with the expected values 0-4.
 * 3. It unloads the codeletSets.
 */

#include <assert.h>
#include <semaphore.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "jbpf_utils.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"
#include "jbpf_test_lib.h"

#define NUM_ITERATIONS 5
#define NUM_CODELET_SETS (2)
#define TOTAL_HOOK_CALLS (NUM_ITERATIONS * NUM_CODELET_SETS)

jbpf_io_stream_id_t stream_ids[] = {
    {.id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x00}},
    {.id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01}}};

sem_t sem;

int first_expected_value = 0;

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
        int s;
        for (s = 0; s < sizeof(stream_ids) / sizeof(stream_ids[0]); s++) {
            if (memcmp(stream_id, &stream_ids[s], sizeof(jbpf_io_stream_id_t)) == 0) {
                break;
            }
        }
        if (s == sizeof(stream_ids) / sizeof(stream_ids[0])) {
            // stream not found
            assert(false);
        }
        output = bufs[i];
        printf("stream=%d buf=%d\n", s, *output);

        // stream0 should have value 0, 1, 2, 3, 4
        // stream1 should have value 0, 1, 2, 3, 4
        assert((*output >= 0) && (*output < NUM_ITERATIONS));

        processed++;
    }

    if (processed == TOTAL_HOOK_CALLS) {
        sem_post(&sem);
    }
}

int
main(int argc, char** argv)
{
    struct jbpf_codeletset_load_req codeletset_load_req[NUM_CODELET_SETS] = {0};
    struct jbpf_codeletset_unload_req codeletset_unload_req[NUM_CODELET_SETS] = {0};
    const char* jbpf_path = getenv("JBPF_PATH");
    struct jbpf_config config = {0};

    JBPF_UNUSED(codeletset_load_req);
    JBPF_UNUSED(codeletset_unload_req);

    jbpf_set_default_config_options(&config);

    assert((sizeof(stream_ids) / sizeof(stream_ids[0])) == NUM_CODELET_SETS);

    sem_init(&sem, 0, 0);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    __assert__(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Register a callback to handle the buffers sent from the codelets
    jbpf_register_io_output_cb(io_channel_check_output);

    for (int i = 0; i < sizeof(codeletset_load_req) / sizeof(codeletset_load_req[0]); i++) {

        struct jbpf_codeletset_load_req* cs_load_req = &codeletset_load_req[i];

        // The name of the codeletset
        snprintf(cs_load_req->codeletset_id.name, JBPF_CODELETSET_NAME_LEN, "simple_output_codeletset_%d", i);

        // Create max possible codelets
        cs_load_req->num_codelet_descriptors = 1;

        // The codelet has just one output channel and no shared maps
        cs_load_req->codelet_descriptor[0].num_in_io_channel = 0;
        cs_load_req->codelet_descriptor[0].num_out_io_channel = 1;

        // set the priority of the codelet
        cs_load_req->codelet_descriptor[0].priority = i;

        // The name of the output map that corresponds to the output channel of the codelet
        strcpy(cs_load_req->codelet_descriptor[0].out_io_channel[0].name, "output_map");
        // Link the map to a stream id
        memcpy(&cs_load_req->codelet_descriptor[0].out_io_channel[0].stream_id, &stream_ids[i], JBPF_STREAM_ID_LEN);

        // The output channel of the codelet does not have a serializer
        cs_load_req->codelet_descriptor[0].out_io_channel[0].has_serde = false;

        // The path of the codelet
        __assert__(jbpf_path != NULL);
        snprintf(
            cs_load_req->codelet_descriptor[0].codelet_path,
            JBPF_PATH_LEN,
            "%s/jbpf_tests/test_files/codelets/simple_output/simple_output.o",
            jbpf_path);

        snprintf(cs_load_req->codelet_descriptor[0].codelet_name, JBPF_CODELET_NAME_LEN, "simple_output_%d", i);
        strcpy(cs_load_req->codelet_descriptor[0].hook_name, "test1");
        cs_load_req->codelet_descriptor[0].num_linked_maps = 0;

        // Load the codeletset
        __assert__(jbpf_codeletset_load(cs_load_req, NULL) == JBPF_CODELET_LOAD_SUCCESS);
    }

    // Call the hooks 5 times and check that we got the expected data
    struct packet p = {0, 0};

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        hook_test1(&p, 1);
        p.counter_a++;
    }

    // Wait for all the output checks to finish
    sem_wait(&sem);

    // Unload the codeletsets
    for (int i = 0; i < sizeof(codeletset_load_req) / sizeof(codeletset_load_req[0]); i++) {
        codeletset_unload_req[i].codeletset_id = codeletset_load_req[i].codeletset_id;
        __assert__(jbpf_codeletset_unload(&codeletset_unload_req[i], NULL) == JBPF_CODELET_UNLOAD_SUCCESS);
    }

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");
    return 0;
}
