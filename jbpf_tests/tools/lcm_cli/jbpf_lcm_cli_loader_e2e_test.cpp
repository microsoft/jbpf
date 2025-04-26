/*
 * This test does the following:
 * 1. It creates a process that uses the LCM IPC CLI to load a single codelet (C1) to a jbpf agent.
 * 2. C1 sends out a counter value, which is incremented by one on each hook call.
 * 3. It uses the LCM IPC CLI to unload the codelet.
 * 4. It makes a request to load a codeletset in a non-existent hook.
 * 5. It makes a request to unload a non-existent codeletset
 */

#include <assert.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <string>
#include <vector>

#include "getopt.h"
#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "loader.hpp"
#include "parser.hpp"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"
#include "jbpf_test_lib.h"

#define NUM_ITERATIONS 5

#define LCM_CLI_SEM_NAME "/jbpf_lcm_cli_loader_e2e_standalone_sem"
#define LCM_CLI_AGENT_SEM_NAME "/jbpf_lcm_cli_loader_e2e_agent_sem"

pid_t cpid = -1;

sem_t sem;

sem_t *lcm_cli_sem, *lcm_cli_agent_sem;

jbpf_io_stream_id_t stream_id_c1 = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

int expected_c1_value = 0;

const std::string inline_bin_arg("./jbpf_lcm_ipc_cli");

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
    int* c1_output;

    for (int i = 0; i < num_bufs; i++) {

        if (memcmp(stream_id, &stream_id_c1, sizeof(stream_id_c1)) == 0) {
            // Output from C1. Check that the counter has the expected value
            c1_output = static_cast<int*>(bufs[i]);
            assert(c1_output);
            assert(*c1_output == expected_c1_value);
            expected_c1_value++;
        } else {
            // Unexpected output. Fail the test
            assert(false);
        }
    }

    if (expected_c1_value == NUM_ITERATIONS) {
        sem_post(&sem);
    }
}

int
run_jbpf_agent()
{

    struct jbpf_config config = {0};

    jbpf_set_default_config_options(&config);
    sem_init(&sem, 0, 0);

    __assert__(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Register a callback to handle the buffers sent from the codelets
    jbpf_register_io_output_cb(io_channel_check_output);

    // Notify the LCM tool that we can now load the codeletset
    sem_post(lcm_cli_sem);

    // Wait until the codeletset is loaded
    sem_wait(lcm_cli_agent_sem);

    // Call the hooks 5 times and check that we got the expected data
    struct packet p = {0, 0};

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        hook_test1(&p, 1);
        p.counter_a++;
    }

    // Wait for all the output checks to finish
    sem_wait(&sem);

    // Test is done. Notify the LCM CLI tool that it can proceed with unloading the codelet
    sem_post(lcm_cli_sem);

    // Wait for the LCM CLI tools to finish
    sem_wait(lcm_cli_agent_sem);

    // Stop
    jbpf_stop();

    sem_destroy(&sem);

    return 0;
}

jbpf_lcm_cli::loader::load_req_outcome
run_lcm_subproc(std::vector<std::string> str_args)
{
    // loader reads opts from argv using getopts
    // so we need to reset the getopts global to start from the beginning
    optind = 1;

    char** args = new char*[str_args.size() + 1];
    args[0] = new char[inline_bin_arg.length() + 1];
    strcpy(args[0], inline_bin_arg.c_str());
    args[0][inline_bin_arg.length()] = '\0';

    for (int i = 0; i < str_args.size(); i++) {
        args[i + 1] = new char[str_args[i].length() + 1];
        strcpy(args[i + 1], str_args[i].c_str());
        args[i + 1][str_args[i].length()] = '\0';
    }

    auto ret = jbpf_lcm_cli::loader::run_loader(str_args.size() + 1, args);

    for (int i = 0; i < str_args.size() + 1; i++)
        delete[] args[i];
    delete[] args;

    return ret;
}

int
run_lcm_ipc_loader()
{
    // Wait until the agent is initialized and is expecting incoming connections
    sem_wait(lcm_cli_sem);

    // Make a request to load codeletset C1 in hook "test1"
    std::vector<std::string> args{
        "-l",
        "-c",
        jbpf_lcm_cli::parser::expand_environment_variables(
            "${JBPF_PATH}/jbpf_tests/tools/lcm_cli/codeletset_req_c1.yaml")};

    // Make a request to load the codeletset
    assert(run_lcm_subproc(args) == jbpf_lcm_cli::loader::JBPF_LCM_CLI_REQ_SUCCESS);

    // Make a 2nd request to load the codeletSet
    assert(run_lcm_subproc(args) == jbpf_lcm_cli::loader::JBPF_LCM_CLI_REQ_SUCCESS);

    // Loading was done, so the agent test can move on
    sem_post(lcm_cli_agent_sem);

    // Wait for the test to finish
    sem_wait(lcm_cli_sem);

    // Test is done. Let's unload the codeletset
    args.clear();
    args.insert(
        args.end(),
        {"-u",
         "-c",
         jbpf_lcm_cli::parser::expand_environment_variables(
             "${JBPF_PATH}/jbpf_tests/tools/lcm_cli/codeletset_unload_req_c1.yaml")});

    // Make a request to unload an existing codeletset
    assert(run_lcm_subproc(args) == jbpf_lcm_cli::loader::JBPF_LCM_CLI_REQ_SUCCESS);

    // Make a request to load a codeletset that will fail, due to non-existent hook
    args.clear();
    args.insert(
        args.end(),
        {"-l",
         "-c",
         jbpf_lcm_cli::parser::expand_environment_variables(
             "${JBPF_PATH}/jbpf_tests/tools/lcm_cli/codeletset_req_error.yaml")});

    assert(run_lcm_subproc(args) == jbpf_lcm_cli::loader::JBPF_LCM_CLI_REQ_FAILURE);

    // Make a request to unload a non-existent codeletset
    args.clear();
    args.insert(
        args.end(),
        {"-u",
         "-c",
         jbpf_lcm_cli::parser::expand_environment_variables(
             "${JBPF_PATH}/jbpf_tests/tools/lcm_cli/codeletset_unload_req_error.yaml")});

    assert(run_lcm_subproc(args) == jbpf_lcm_cli::loader::JBPF_LCM_CLI_REQ_FAILURE);

    // Tests are done. Let the agent know that it can terminate
    sem_post(lcm_cli_agent_sem);
    return 0;
}

int
main(int argc, char** argv)
{

    pid_t child_pid;
    int secondary_status;
    int res;

    // Register some signals to kill the test, if it fails
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
    sem_unlink(LCM_CLI_SEM_NAME);
    sem_unlink(LCM_CLI_AGENT_SEM_NAME);

    lcm_cli_sem = sem_open(LCM_CLI_SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0);
    if (lcm_cli_sem == SEM_FAILED) {
        sem_unlink(LCM_CLI_SEM_NAME);
        perror("failed to open semaphore");
        exit(EXIT_FAILURE);
    }

    lcm_cli_agent_sem = sem_open(LCM_CLI_AGENT_SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0);
    if (lcm_cli_agent_sem == SEM_FAILED) {
        sem_unlink(LCM_CLI_AGENT_SEM_NAME);
        perror("failed to open agent semaphore");
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

    sem_unlink(LCM_CLI_SEM_NAME);
    sem_unlink(LCM_CLI_AGENT_SEM_NAME);
    return 0;
}
