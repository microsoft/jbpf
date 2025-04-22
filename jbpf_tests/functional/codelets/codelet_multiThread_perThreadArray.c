/*
 * The purpose of this test is to ensure that perThread array maps operate as expected.
 *
 * This test does the following:
 * 1. It creates 5 threads.  Each thread calls hook_test1 5 times.
 * 2. It loads codelet using objtect simple_output_shared_counter_perThread.  This codelet object does the following ...
 *   a. It reads from a perThread array.
 *   b. It increments the value and writes it back to the array.
 *   c. It sends the value to an output channel.
 * 3. Since the codelet uses a perThread array, each thread should send values 1..5 to the output buffer.  To check
 * this, the output buffer is read, and bins are used to count the number of times each value is seen in the output
 * buffer.
 * 4. Each value should be seen 5 times i.e. once for per thread.  This is asserted.
 */

#include <assert.h>
#include <semaphore.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "jbpf_utils.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

#define NUM_ITERATIONS (5)
#define NUM_THREADS (5)
#define TOTAL_HOOK_CALLS (NUM_ITERATIONS * NUM_THREADS)

jbpf_io_stream_id_t stream_id_c1 = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

sem_t sem;
sem_t sync_sem;

static pthread_t test_threads[NUM_THREADS];

static uint16_t processed = 0;
#define MIN_EXPECTED_VALUE (1)
#define MAX_EXPECTED_VALUE (NUM_ITERATIONS)
static int value_bins[MAX_EXPECTED_VALUE - MIN_EXPECTED_VALUE + 1] = {0};

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
        assert(memcmp(stream_id, &stream_id_c1, sizeof(jbpf_io_stream_id_t)) == 0);

        output = bufs[i];
        printf("output = %d\n", *output);

        assert(*output >= MIN_EXPECTED_VALUE && *output <= MAX_EXPECTED_VALUE);

        // increment the bin corresponding to the value
        int bin = *output - MIN_EXPECTED_VALUE;
        value_bins[bin]++;

        processed++;
        printf("processed = %d\n", processed);
    }

    if (processed == TOTAL_HOOK_CALLS) {
        sem_post(&sem);
    }
}

static void*
test_thread(void* arg)
{
    // uint64_t id = (uint64_t)arg;

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // wait before calling hooks
    sem_wait(&sync_sem);

    // Call the hooks 5 times and check that we got the expected data
    struct packet p = {0, 0};

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // printf("thread %ld: calling hook_test1 iter %d\n", id, i);
        hook_test1(&p, 0);
    }
    return NULL;
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

    sem_init(&sync_sem, 0, 0);
    sem_init(&sem, 0, 0);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    int res = jbpf_init(&config);
    assert(res == 0);
#ifdef NDEBUG
    (void)res; // suppress unused-variable warning in release mode
#endif

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Register a callback to handle the buffers sent from the codelets
    jbpf_register_io_output_cb(io_channel_check_output);

    for (uint64_t i = 0; i < sizeof(test_threads) / sizeof(test_threads[0]); i++) {
        int ret = pthread_create(&test_threads[i], NULL, &test_thread, (void*)i);
        JBPF_UNUSED(ret);
        assert(ret == 0);
    }

    // The name of the codeletset
    strcpy(codeletset_req_c1.codeletset_id.name, "simple_output_shared_counter_perThread_codeletset");

    // Create max possible codelets
    codeletset_req_c1.num_codelet_descriptors = 1;

    // The codelet has just one output channel and no shared maps
    codeletset_req_c1.codelet_descriptor[0].num_in_io_channel = 0;
    codeletset_req_c1.codelet_descriptor[0].num_out_io_channel = 1;

    // The name of the output map that corresponds to the output channel of the codelet
    strcpy(codeletset_req_c1.codelet_descriptor[0].out_io_channel[0].name, "output_map");
    // Link the map to a stream id
    memcpy(&codeletset_req_c1.codelet_descriptor[0].out_io_channel[0].stream_id, &stream_id_c1, JBPF_STREAM_ID_LEN);
    // The output channel of the codelet does not have a serializer
    codeletset_req_c1.codelet_descriptor[0].out_io_channel[0].has_serde = false;
    codeletset_req_c1.codelet_descriptor[0].num_linked_maps = 0;

    // The path of the codelet
    assert(jbpf_path != NULL);
    snprintf(
        codeletset_req_c1.codelet_descriptor[0].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/simple_output_shared_counter_perThread/"
        "simple_output_shared_counter_perThread.o",
        jbpf_path);
    strcpy(codeletset_req_c1.codelet_descriptor[0].codelet_name, "simple_output_shared_counter_perThread");

    snprintf(codeletset_req_c1.codelet_descriptor[0].hook_name, JBPF_HOOK_NAME_LEN, "test1");

    // Load the codeletset
    res = jbpf_codeletset_load(&codeletset_req_c1, NULL);
    assert(res == JBPF_CODELET_LOAD_SUCCESS);

    for (int i = 0; i < sizeof(test_threads) / sizeof(test_threads[0]); i++) {
        sem_post(&sync_sem);
    }

    // Wait for all the output checks to finish
    sem_wait(&sem);

    // chech the bins are the expected counts
    for (int i = 0; i < MAX_EXPECTED_VALUE - MIN_EXPECTED_VALUE + 1; i++) {
        assert(value_bins[i] == NUM_ITERATIONS);
    }

    // Unload the codeletsets
    codeletset_unload_req_c1.codeletset_id = codeletset_req_c1.codeletset_id;
    assert(jbpf_codeletset_unload(&codeletset_unload_req_c1, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    // Stop
    jbpf_stop();

    printf("Test completed successfully\n");
    return 0;
}
