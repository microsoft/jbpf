// Copyright (c) Microsoft Corporation. All rights reserved.
#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <csignal>

#include "common.h"

#include "jbpf.h"
#include "jbpf_hook.h"
#include "jbpf_defs.h"

using namespace std;

#define IPC_NAME "example_ipc_app"

// Hook declaration and definition.
DECLARE_JBPF_HOOK(
    example,
    struct jbpf_generic_ctx ctx,
    ctx,
    HOOK_PROTO(Packet* p, int ctx_id),
    HOOK_ASSIGN(ctx.ctx_id = ctx_id; ctx.data = (uint64_t)(void*)p; ctx.data_end = (uint64_t)(void*)(p + 1);))

DEFINE_JBPF_HOOK(example)

bool done = false;

void
sig_handler(int signo)
{
    done = true;
}

int
handle_signal()
{
    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        return 0;
    }
    if (signal(SIGTERM, sig_handler) == SIG_ERR) {
        return 0;
    }
    return -1;
}

int
main(int argc, char** argv)
{

    struct jbpf_config jbpf_config = {0};
    jbpf_set_default_config_options(&jbpf_config);

    // Instruct libjbpf to use an external IPC interface
    jbpf_config.io_config.io_type = JBPF_IO_IPC_CONFIG;
    // Configure memory size for the IO buffer
    jbpf_config.io_config.io_ipc_config.ipc_mem_size = JBPF_HUGEPAGE_SIZE_1GB;
    strncpy(jbpf_config.io_config.io_ipc_config.ipc_name, IPC_NAME, JBPF_IO_IPC_MAX_NAMELEN);

    // Enable LCM IPC interface using UNIX socket at the default socket path (the default is through C API)
    jbpf_config.lcm_ipc_config.has_lcm_ipc_thread = true;
    snprintf(
        jbpf_config.lcm_ipc_config.lcm_ipc_name,
        sizeof(jbpf_config.lcm_ipc_config.lcm_ipc_name) - 1,
        "%s",
        JBPF_DEFAULT_LCM_SOCKET);

    if (!handle_signal()) {
        std::cout << "Could not register signal handler" << std::endl;
        return -1;
    }

    // Initialize jbpf
    if (jbpf_init(&jbpf_config) < 0) {
        return -1;
    }

    // Any thread that calls a hook must be registered
    jbpf_register_thread();

    int i = 0;

    // Sample application code calling a hook every second
    while (!done) {
        Packet p;
        p.seq_no = i;
        p.value = -i;

        std::stringstream ss;
        ss << "instance " << i;

        std::strcpy(p.name, ss.str().c_str());

        // Call hook and pass packet
        hook_example(&p, 1);
        sleep(1);
        i++;
    }

    jbpf_stop();
    exit(EXIT_SUCCESS);

    return 0;
}
