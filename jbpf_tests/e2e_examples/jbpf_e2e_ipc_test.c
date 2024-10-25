/*
 * This test demonstrates how jbpf can work in IPC mode mode with shared memory.
 * It does the following:
 * 1. It loads a codeletset with a single codelet (C1) that sends a counter as an output.
 * 2. It loads a second codeletset with two codelets (C2, C3) sharing a map.
 * 3. C1 sends out a counter value, which is incremented by one on each hook call.
 * 3. A control input is sent to codelet C2 and is written to the shared map.
 * 4. C3 picks the data from the shared map and sends them out through an output map.
 */

#include <assert.h>
#include <semaphore.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "jbpf_utils.h"

#include "jbpf_io.h"
#include "jbpf_io_defs.h"
#include "jbpf_io_queue.h"
#include "jbpf_io_channel.h"

#define NUM_ITERATIONS 5

#define PRIMARY_SEM_NAME "/jbpf_e2e_ipc_test_primary_sem"
#define SECONDARY_SEM_NAME "/jbpf_e2e_ipc_test_secondary"

#define IPC_NAME "e2e_ipc_test_app"

pid_t cpid = -1;

sem_t *primary_sem, *secondary_sem;

jbpf_io_stream_id_t stream_id_c1 = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};
jbpf_io_stream_id_t stream_id_c2 = {
    .id = {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00}};
jbpf_io_stream_id_t stream_id_c3 = {
    .id = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11}};

int expected_c1_value = 0;
int expected_c2_c3_value = 100;

bool checks_done = false;

void
handle_sigchld(int sig)
{
    return;
}

void
handle_sigterm(int sig)
{
    if (cpid > 0) {
        kill(cpid, SIGKILL);
    }
    exit(EXIT_FAILURE);
}

void
handle_channel_bufs(
    struct jbpf_io_channel* io_channel, struct jbpf_io_stream_id* stream_id, void** bufs, int num_bufs, void* ctx)
{
    int* c1_output;
    int* c2_c3_output;

    JBPF_UNUSED(c1_output);
    JBPF_UNUSED(c2_c3_output);

    for (int i = 0; i < num_bufs; i++) {

        if (memcmp(stream_id, &stream_id_c1, sizeof(stream_id_c1)) == 0) {
            // Output from C1. Check that the counter has the expected value
            c1_output = bufs[i];
            assert(c1_output);
            assert(*c1_output == expected_c1_value);
            expected_c1_value++;
        } else if (memcmp(stream_id, &stream_id_c3, sizeof(stream_id_c3)) == 0) {
            // Output from C3. Check that the output is the same as the input we sent to C2
            c2_c3_output = bufs[i];
            assert(*c2_c3_output == expected_c2_c3_value);
            expected_c2_c3_value++;
        } else {
            // Unexpected output. Fail the test
            assert(false);
        }
    }

    if (expected_c1_value == NUM_ITERATIONS && expected_c2_c3_value == 105) {
        checks_done = true;
    }
}

// The primary process acts as a primary for the jbpf_io_lib
int
run_primary_process(void)
{
    struct jbpf_io_config io_config = {0};
    struct jbpf_io_ctx* io_ctx;

    io_config.type = JBPF_IO_IPC_PRIMARY;

    strncpy(io_config.ipc_config.addr.jbpf_io_ipc_name, IPC_NAME, JBPF_IO_IPC_MAX_NAMELEN);

    // Allocate 64MB of memory for the local IO of the primary process
    io_config.ipc_config.mem_cfg.memory_size = 1024 * 1024 * 64;
    // Configure the jbpf agent to operate in shared memory mode
    io_ctx = jbpf_io_init(&io_config);

    assert(io_ctx);

    assert(jbpf_io_register_thread());

    // Primary is initialized. Signal secondary and
    // wait until codelets are loaded
    sem_post(primary_sem);
    sem_wait(secondary_sem);

    // Send 5 control inputs
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        custom_api control_input = {.command = i + 100};
        JBPF_UNUSED(control_input);
        assert(jbpf_io_channel_send_msg(io_ctx, &stream_id_c2, &control_input, sizeof(control_input)) == 0);
    }

    // Wait until the secondary has finished with its responses
    sem_post(primary_sem);
    sem_wait(secondary_sem);

    // Check if you have received the expected data from the codelets
    while (!checks_done) {
        jbpf_io_channel_handle_out_bufs(io_ctx, handle_channel_bufs, NULL);
        sleep(1);
    }

    // Allow the secondary to terminate
    sem_post(primary_sem);

    return 0;
}

int
run_jbpf_agent(void)
{
    struct jbpf_codeletset_load_req codeletset_req_c1 = {0}, codeletset_req_c2_c3 = {0};
    struct jbpf_codeletset_unload_req codeletset_unload_req_c1 = {0}, codeletset_unload_req_c2_c3 = {0};
    const char* jbpf_path = getenv("JBPF_PATH");

    struct jbpf_config config = {0};

    jbpf_set_default_config_options(&config);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    // Configure the jbpf agent to operate in shared memory mode
    config.io_config.io_type = JBPF_IO_IPC_CONFIG;
    config.io_config.io_ipc_config.ipc_mem_size = JBPF_HUGEPAGE_SIZE_1GB;
    strncpy(config.io_config.io_ipc_config.ipc_name, IPC_NAME, JBPF_IO_IPC_MAX_NAMELEN);

    // Primary is ready, so let's initialize the agent
    sem_wait(primary_sem);

    assert(jbpf_init(&config) == 0);

    jbpf_register_thread();

    // Agent is ready, let the primary ru

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
    strcpy(codeletset_req_c1.codelet_descriptor[0].hook_name, "test1");

    // Load the codeletset
    assert(jbpf_codeletset_load(&codeletset_req_c1, NULL) == JBPF_CODELET_LOAD_SUCCESS);

    // Next, we load the codeletset with codelets C2 and C3 in hooks "test1" and "test2"
    strcpy(codeletset_req_c2_c3.codeletset_id.name, "shared_map_input_output_codeletset");
    // This codeletset has 2 codelets
    codeletset_req_c2_c3.num_codelet_descriptors = 2;

    // Codelet 1 (C2) has one input map and no output map
    codeletset_req_c2_c3.codelet_descriptor[0].num_out_io_channel = 0;
    codeletset_req_c2_c3.codelet_descriptor[0].num_linked_maps = 0;
    codeletset_req_c2_c3.codelet_descriptor[0].num_in_io_channel = 1;
    // The name of the input map is "input_map"
    strcpy(codeletset_req_c2_c3.codelet_descriptor[0].in_io_channel[0].name, "input_map");
    // Link the map to a stream id
    memcpy(&codeletset_req_c2_c3.codelet_descriptor[0].in_io_channel[0].stream_id, &stream_id_c2, JBPF_STREAM_ID_LEN);
    // The input channel of the codelet does not have a serializer
    codeletset_req_c2_c3.codelet_descriptor[0].in_io_channel[0].has_serde = false;

    assert(jbpf_path != NULL);
    snprintf(
        codeletset_req_c2_c3.codelet_descriptor[0].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/simple_input_shared/simple_input_shared.o",
        jbpf_path);
    strcpy(codeletset_req_c2_c3.codelet_descriptor[0].codelet_name, "simple_input_shared");
    strcpy(codeletset_req_c2_c3.codelet_descriptor[0].hook_name, "test1");

    // Codelet 2 (C3) has one output map and one shared map linked to C2
    codeletset_req_c2_c3.codelet_descriptor[1].num_in_io_channel = 0;
    codeletset_req_c2_c3.codelet_descriptor[1].num_out_io_channel = 1;
    strcpy(codeletset_req_c2_c3.codelet_descriptor[1].out_io_channel[0].name, "output_map");
    memcpy(&codeletset_req_c2_c3.codelet_descriptor[1].out_io_channel[0].stream_id, &stream_id_c3, JBPF_STREAM_ID_LEN);
    codeletset_req_c2_c3.codelet_descriptor[1].out_io_channel[0].has_serde = false;

    codeletset_req_c2_c3.codelet_descriptor[1].num_linked_maps = 1;
    // The name of the linked map as it appears in C3
    strcpy(codeletset_req_c2_c3.codelet_descriptor[1].linked_maps[0].map_name, "shared_map_output");
    // The codelet that contains the map that this map will be linked to
    strcpy(codeletset_req_c2_c3.codelet_descriptor[1].linked_maps[0].linked_codelet_name, "simple_input_shared");
    // The name of the linked map as it appears in C2
    strcpy(codeletset_req_c2_c3.codelet_descriptor[1].linked_maps[0].linked_map_name, "shared_map");

    assert(jbpf_path != NULL);
    snprintf(
        codeletset_req_c2_c3.codelet_descriptor[1].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/simple_output_shared/simple_output_shared.o",
        jbpf_path);
    strcpy(codeletset_req_c2_c3.codelet_descriptor[1].codelet_name, "simple_output_shared");
    strcpy(codeletset_req_c2_c3.codelet_descriptor[1].hook_name, "test2");

    // Load the codeletset
    assert(jbpf_codeletset_load(&codeletset_req_c2_c3, NULL) == JBPF_CODELET_LOAD_SUCCESS);

    // Wait until the primary sends the inputs
    sem_post(secondary_sem);
    sem_wait(primary_sem);

    // Call the hooks 5 times and check that we got the expected data
    struct packet p = {0, 0};

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        hook_test1(&p, 1);
        hook_test2(&p, 2);

        p.counter_a++;
    }

    // Allow the primary to check for the sent messages
    sem_post(secondary_sem);

    // Wait until the primary is done, to unload the codelets
    sem_wait(primary_sem);

    // Unload the codeletsets
    strcpy(codeletset_unload_req_c1.codeletset_id.name, "simple_output_codeletset");
    assert(jbpf_codeletset_unload(&codeletset_unload_req_c1, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    strcpy(codeletset_unload_req_c2_c3.codeletset_id.name, "shared_map_input_output_codeletset");
    assert(jbpf_codeletset_unload(&codeletset_unload_req_c2_c3, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    // Stop
    jbpf_stop();

    return 0;
}

int
main(int argc, char** argv)
{

    pid_t child_pid;
    int secondary_status;
    int res;

    JBPF_UNUSED(res);

    // Register some signals to kkill the test, if it fails
    struct sigaction sa;
    sa.sa_handler = &handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, 0) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    struct sigaction sa_child;
    sa_child.sa_handler = &handle_sigterm;
    sigemptyset(&sa_child.sa_mask);
    sa_child.sa_flags = 0;
    if (sigaction(SIGTERM, &sa_child, 0) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGINT, &sa_child, 0) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGABRT, &sa_child, 0) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Remove the semaphore if the test did not finish gracefully
    sem_unlink(PRIMARY_SEM_NAME);
    sem_unlink(SECONDARY_SEM_NAME);

    primary_sem = sem_open(PRIMARY_SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0);
    if (primary_sem == SEM_FAILED) {
        exit(EXIT_FAILURE);
    }

    secondary_sem = sem_open(SECONDARY_SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0);
    if (secondary_sem == SEM_FAILED) {
        exit(EXIT_FAILURE);
    }

    child_pid = fork();
    assert(child_pid >= 0);

    if (child_pid == 0) {
        assert(run_jbpf_agent() == 0);
    } else {
        cpid = child_pid;
        assert(run_primary_process() == 0);
        res = wait(&secondary_status);
        assert(res != -1);
        assert(secondary_status == 0);

        printf("Test completed successfully\n");
    }

    sem_unlink(PRIMARY_SEM_NAME);
    sem_unlink(SECONDARY_SEM_NAME);

    return 0;
}
