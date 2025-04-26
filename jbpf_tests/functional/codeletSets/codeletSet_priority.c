/*
 * The purpose of this test is to check that codelets are called as per their configured priority.
 *
 * This test does the following:
 * 1. It loads a codeletSet which has 8 codelets.
 * 2. The priorities of the codelets are created in a way to ensure codelets must be "inserted" into the priority list.
 * Specifically... a. for even numbered codelets, use the codelet number b. for odd numbered codelets, use (UINT_MAX -
 * codelet number)
 * 3. All the codelets use the same hook.
 * 4. The hook is called NUM_ITERATIONS times.
 * 5. The output is asserted for the following ...
 *    a. only the expected streams are received.
 *    b. on a per stream basis, only the expected values are received.
 * 6. A semaphore is used to ensure that all the expected output messages are received.
 * 7. The codeletSet is unloaded.
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
#define TOTAL_HOOK_CALLS (NUM_ITERATIONS * JBPF_MAX_CODELETS_IN_CODELETSET)

jbpf_io_stream_id_t stream_ids[] = {
    {.id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x00}},
    {.id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01}},
    {.id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x02}},
    {.id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x03}},
    {.id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x04}},
    {.id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x05}},
    {.id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x06}},
    {.id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x07}}};

sem_t sem;

int first_expected_value = 1;

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
// printf("s=%d buf=%d\n", s, *output);

// stream0 should have value 8, 16, 24, 32, 40
// stream1 should have value 1, 9, 17, 25, 33
// stream2 should have value 7, 15, 23, 31, 39
// stream3 should have value 2, 10, 18, 26, 34
// stream4 should have value 6, 14, 22, 30, 38
// stream5 should have value 3, 11, 19, 27, 35
// stream6 should have value 5, 13, 21, 29, 37
// stream7 should have value 4, 12, 20, 28, 36
#define STREAM_COUNT 8
        const int expected_values[STREAM_COUNT][NUM_ITERATIONS] = {
            {8, 16, 24, 32, 40},
            {1, 9, 17, 25, 33},
            {7, 15, 23, 31, 39},
            {2, 10, 18, 26, 34},
            {6, 14, 22, 30, 38},
            {3, 11, 19, 27, 35},
            {5, 13, 21, 29, 37},
            {4, 12, 20, 28, 36}};

        // Check the values using assertions
        int it = 0;
        for (it = 0; it < NUM_ITERATIONS; ++it) {
            if (*output == expected_values[sidx][it])
                break;
        }
        assert(it != NUM_ITERATIONS);

        processed++;
    }

    if (processed == TOTAL_HOOK_CALLS) {
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

    assert((sizeof(stream_ids) / sizeof(stream_ids[0])) == JBPF_MAX_CODELETS_IN_CODELETSET);

    sem_init(&sem, 0, 0);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    __assert__(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Register a callback to handle the buffers sent from the codelets
    jbpf_register_io_output_cb(io_channel_check_output);

    // Load the codeletset with codelet C1 in hook "test1"

    // The name of the codeletset
    strcpy(codeletset_req_c1.codeletset_id.name, "simple_output_shared_counter_codeletset");

    // Create max possible codelets
    codeletset_req_c1.num_codelet_descriptors = JBPF_MAX_CODELETS_IN_CODELETSET;

    for (uint16_t i = 0; i < JBPF_MAX_CODELETS_IN_CODELETSET; i++) {

        // The codelet has just one output channel and no shared maps
        codeletset_req_c1.codelet_descriptor[i].num_in_io_channel = 0;
        codeletset_req_c1.codelet_descriptor[i].num_out_io_channel = 1;

        // set the priority of the codelet
        // for even indexes, use i,
        // for odd indexes, use (UINT_MAX-1)
        jbpf_codelet_priority_t priority = ((i % 2) == 0) ? i : UINT_MAX - i;

        codeletset_req_c1.codelet_descriptor[i].priority = priority;
        // printf("priority = %d\n", codeletset_req_c1.codelet_descriptor[i].priority);

        // The name of the output map that corresponds to the output channel of the codelet
        strcpy(codeletset_req_c1.codelet_descriptor[i].out_io_channel[0].name, "output_map");
        // Link the map to a stream id
        memcpy(
            &codeletset_req_c1.codelet_descriptor[i].out_io_channel[0].stream_id, &stream_ids[i], JBPF_STREAM_ID_LEN);
        // The output channel of the codelet does not have a serializer
        codeletset_req_c1.codelet_descriptor[i].out_io_channel[0].has_serde = false;

        // The path of the codelet
        __assert__(jbpf_path != NULL);
        snprintf(
            codeletset_req_c1.codelet_descriptor[i].codelet_path,
            JBPF_PATH_LEN,
            "%s/jbpf_tests/test_files/codelets/simple_output_shared_counter/simple_output_shared_counter.o",
            jbpf_path);

        snprintf(
            codeletset_req_c1.codelet_descriptor[i].codelet_name,
            JBPF_CODELET_NAME_LEN,
            "simple_output_shared_counter_%d",
            i);
        strcpy(codeletset_req_c1.codelet_descriptor[i].hook_name, "test1");

        // all codelets should have linked map to shared_counter_map of codelet 0
        if (i == 0) {
            codeletset_req_c1.codelet_descriptor[i].num_linked_maps = 0;
        } else {

            codeletset_req_c1.codelet_descriptor[i].num_linked_maps = 1;

            strcpy(codeletset_req_c1.codelet_descriptor[i].linked_maps[0].map_name, "shared_counter_map");
            strcpy(
                codeletset_req_c1.codelet_descriptor[i].linked_maps[0].linked_codelet_name,
                codeletset_req_c1.codelet_descriptor[0].codelet_name);
            strcpy(codeletset_req_c1.codelet_descriptor[i].linked_maps[0].linked_map_name, "shared_counter_map");
        }
    }

    // Load the codeletset
    __assert__(jbpf_codeletset_load(&codeletset_req_c1, NULL) == JBPF_CODELET_LOAD_SUCCESS);

    // Call the hooks 5 times and check that we got the expected data
    struct packet p = {0, 0};

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        hook_test1(&p, i);
    }

    // Wait for all the output checks to finish
    sem_wait(&sem);

    // Unload the codeletsets
    codeletset_unload_req_c1.codeletset_id = codeletset_req_c1.codeletset_id;
    __assert__(jbpf_codeletset_unload(&codeletset_unload_req_c1, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");
    return 0;
}
