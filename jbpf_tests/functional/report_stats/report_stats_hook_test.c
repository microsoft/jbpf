/*
 * This test does the following:
 * 1. It creates a process that uses the LCM IPC API to load a single codelet (C1) to a jbpf agent.
 * 2. The codelet has a single hook (report_stats) that is called by the agent internally at the interval of MAINTENANCE_CHECK_INTERVAL (see jbpf_perf.c).
 * 3. Then we check if the codelet (attached to report_stats hook) has been actually called by the agent.
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

#define LCM_IPC_SEM_NAME "/jbpf_e2e_lcm_report_stats_ipc_standalone_sem"
#define LCM_IPC_AGENT_SEM_NAME "/jbpf_e2e_report_stats_lcm_ipc_agent_sem"

sem_t *lcm_ipc_sem, *lcm_ipc_agent_sem;

jbpf_io_stream_id_t stream_id_c1 = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

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

static void
io_channel_check_output(jbpf_io_stream_id_t* stream_id, void** bufs, int num_bufs, void* ctx)
{
    int count = 0;
    for (int i = 0; i < num_bufs; i++) {
        if (memcmp(stream_id, &stream_id_c1, sizeof(stream_id_c1)) == 0) {
            // Output from C1. Check that the counter has the expected value
            count ++;
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
run_jbpf_agent(void)
{

    struct jbpf_config config = {0};

    jbpf_set_default_config_options(&config);
    sem_init(&sem, 0, 0);

    assert(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Register a callback to handle the buffers sent from the codelets
    jbpf_register_io_output_cb(io_channel_check_output);

    // Notify the LCM tool that we can now load the codeletset
    sem_post(lcm_ipc_sem);

    // Wait until the codeletset is loaded
    sem_wait(lcm_ipc_agent_sem);

    // Wait for all the output checks to finish
    sem_wait(&sem);

    // Test is done. Notify the LCM IPC tool that it can proceed with unloading the codelet
    sem_post(lcm_ipc_sem);

    // Wait for the LCM IPC tools to finish
    sem_wait(lcm_ipc_agent_sem);

    // Stop
    jbpf_stop();

    sem_destroy(&sem);

    return 0;
}

int
run_lcm_ipc_loader(void)
{
    struct jbpf_codeletset_load_req codeletset_req_c1 = {0};
    struct jbpf_codeletset_unload_req codeletset_unload_req_c1 = {0};
    jbpf_lcm_ipc_address_t address = {0};

    const char* jbpf_path = getenv("JBPF_PATH");

    snprintf(
        address.path,
        sizeof(address.path) - 1,
        "%s/%s/%s",
        JBPF_DEFAULT_RUN_PATH,
        JBPF_DEFAULT_NAMESPACE,
        JBPF_DEFAULT_LCM_SOCKET);

    // Wait until the agent is initialized and is expecting incoming connections
    sem_wait(lcm_ipc_sem);

    // Make a request to load codeletset C1 in hook "test1"

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

    assert(jbpf_path != NULL);
    snprintf(
        codeletset_req_c1.codelet_descriptor[0].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/simple_output/simple_output.o",
        jbpf_path);
    strcpy(codeletset_req_c1.codelet_descriptor[0].codelet_name, "simple_output");
    strcpy(codeletset_req_c1.codelet_descriptor[0].hook_name, "report_stats");

    // Make a request to load the codeletset
    assert(jbpf_lcm_ipc_send_codeletset_load_req(&address, &codeletset_req_c1) == JBPF_LCM_IPC_REQ_SUCCESS);

    // Loading was done, so the agent test can move on
    sem_post(lcm_ipc_agent_sem);

    // Wait for the test to finish
    sem_wait(lcm_ipc_sem);

    // Test is done. Let's unload the codeletset
    strcpy(codeletset_unload_req_c1.codeletset_id.name, "simple_output_codeletset");

    // Make a request to unload an existing codeletset
    assert(jbpf_lcm_ipc_send_codeletset_unload_req(&address, &codeletset_unload_req_c1) == JBPF_LCM_IPC_REQ_SUCCESS);

    // Tests are done. Let the agent know that it can terminate
    sem_post(lcm_ipc_agent_sem);
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
    sem_unlink(LCM_IPC_SEM_NAME);
    sem_unlink(LCM_IPC_AGENT_SEM_NAME);

    lcm_ipc_sem = sem_open(LCM_IPC_SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0);
    if (lcm_ipc_sem == SEM_FAILED) {
        exit(EXIT_FAILURE);
    }

    lcm_ipc_agent_sem = sem_open(LCM_IPC_AGENT_SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0);
    if (lcm_ipc_agent_sem == SEM_FAILED) {
        exit(EXIT_FAILURE);
    }

    child_pid = fork();
    assert(child_pid >= 0);

    if (child_pid == 0) {
        assert(run_lcm_ipc_loader() == 0);
    } else {
        cpid = child_pid; // TODO
        assert(run_jbpf_agent() == 0);
        res = wait(&secondary_status);
        assert(res != -1);
        assert(secondary_status == 0);

        printf("Test is now complete...\n");
    }

    sem_unlink(LCM_IPC_SEM_NAME);
    sem_unlink(LCM_IPC_AGENT_SEM_NAME);
    return 0;
}
