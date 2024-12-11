/*
 * This test does the following:
 * 1. It creates a process that uses the LCM IPC API to load a single codelet (C1) to a jbpf agent.
 * 2. The codelet has a single hook (report_stats) that is called by the agent internally at the interval of
 * MAINTENANCE_CHECK_INTERVAL (see jbpf_perf.c).
 * 3. Then we check if the codelet (attached to report_stats hook) has been actually called by the agent. If it has been
 * called, we should get the output from the codelet.
 * 4. It uses the LCM IPC API to unload the codelet.
 */

#include <assert.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "jbpf_utils.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

pid_t cpid = -1;

sem_t sem;

jbpf_io_stream_id_t stream_id_c1 = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

static void
io_channel_check_output(jbpf_io_stream_id_t* stream_id, void** bufs, int num_bufs, void* ctx)
{
    int count = 0;
    for (int i = 0; i < num_bufs; i++) {
        if (memcmp(stream_id, &stream_id_c1, sizeof(stream_id_c1)) == 0) {
            // Output from C1. Check that the counter has the expected value
            count++;
        } else {
            // Unexpected output. Fail the test
            assert(false);
        }
    }

    // This means the report_stats hook has been called
    if (count > 0) {
        sem_post(&sem);
    } else {
        // we don't get any output from the codelet, meaning the report_stats hook has not been called
        assert(false);
    }
}

int
main(int argc, char** argv)
{
    struct jbpf_codeletset_load_req codeletset_req_c1 = {0};
    struct jbpf_codeletset_unload_req codeletset_unload_req_c1 = {0};
    const char* jbpf_path = getenv("JBPF_PATH");
    struct jbpf_config config = {0};

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
    strcpy(codeletset_req_c1.codeletset_id.name, "simple_output_codeletset");

    // We have only one codelet in this codeletset
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
        "%s/jbpf_tests/test_files/codelets/simple_output/simple_output.o",
        jbpf_path);
    strcpy(codeletset_req_c1.codelet_descriptor[0].codelet_name, "simple_output");
    strcpy(codeletset_req_c1.codelet_descriptor[0].hook_name, "report_stats");

    // Load the codeletset
    assert(jbpf_codeletset_load(&codeletset_req_c1, NULL) == JBPF_CODELET_LOAD_SUCCESS);

    // Wait for all the output checks to finish
    sem_wait(&sem);

    // Unload the codeletsets
    strcpy(codeletset_unload_req_c1.codeletset_id.name, "simple_output_codeletset");
    assert(jbpf_codeletset_unload(&codeletset_unload_req_c1, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    // Stop
    jbpf_stop();

    sem_destroy(&sem);

    printf("Test completed successfully.\n");
    return 0;
}
