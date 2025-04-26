/*
 * The purpose of this test is to ensure we can configure the maximum number of linked maps for a codelet.
 *
 * This test does the following:
 * 1. It loads a codeletset with two codelets (C1) using lcm cli.
 * 2. Codelet 1 is configured with file codelets/max_input_shared/max_input_shared.o, which has JBPF_MAX_IO_CHANNEL (5)
 * shared maps.
 * 3. Codelet 2 is configured with JBPF_MAX_LINKED_MAPS (5) linked maps, and these are linked to codelet 1 shared_map0
 * .. shared_map4.
 * 4. It asserts that the return code from the jbpf_codeletset_load call is JBPF_CODELET_LOAD_SUCCESS).
 * 5. It unloads the codeletSet.
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

#define LCM_CLI_SEM_NAME "/jbpf_lcm_cli_loader_e2e_standalone_sem"
#define LCM_CLI_AGENT_SEM_NAME "/jbpf_lcm_cli_loader_e2e_agent_sem"

pid_t cpid = -1;

sem_t sem;

sem_t *lcm_cli_sem, *lcm_cli_agent_sem;

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

int
run_jbpf_agent()
{
    struct jbpf_config config = {0};

    jbpf_set_default_config_options(&config);
    sem_init(&sem, 0, 0);

    __assert__(jbpf_init(&config) == 0);

    // Register jbpf thread
    jbpf_register_thread();

    // Notify the LCM tool that we can now load the codeletset
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
            "${JBPF_PATH}/jbpf_tests/tools/lcm_cli/codeletset_req_with_linked_map.yaml")};

    // Make a request to load the codeletset
    assert(run_lcm_subproc(args) == jbpf_lcm_cli::loader::JBPF_LCM_CLI_REQ_SUCCESS);

    // Test is done. Let's unload the codeletset
    args.clear();
    args.insert(
        args.end(),
        {"-u",
         "-c",
         jbpf_lcm_cli::parser::expand_environment_variables(
             "${JBPF_PATH}/jbpf_tests/tools/lcm_cli/codeletset_unload_req_with_linked_map.yaml")});

    // Make a request to unload an existing codeletset
    assert(run_lcm_subproc(args) == jbpf_lcm_cli::loader::JBPF_LCM_CLI_REQ_SUCCESS);

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
        exit(EXIT_FAILURE);
    }

    lcm_cli_agent_sem = sem_open(LCM_CLI_AGENT_SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0);
    if (lcm_cli_agent_sem == SEM_FAILED) {
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
