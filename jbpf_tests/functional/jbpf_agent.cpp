// Copyright (c) Microsoft Corporation. All rights reserved.
/**
 * This is more like an example to show that we can integrate jbpf to C++ applications
 */
#include <signal.h>
#include <unistd.h>

#include "jbpf.h"
#include "jbpf_hook.h"
#include "jbpf_logging.h"
#include "jbpf_test_def.h"
#include "jbpf_agent_common.h"

#include <iostream>
using namespace std;

void*
worker1(void* ctx)
{
    jbpf_register_thread();
    struct packet p = {1, 2};

    while (1) {
        hook_test1(&p, 1);
        hook_test2(&p, 1);
        usleep(100000);
    }

    jbpf_cleanup_thread();
}

int
main(int argc, char** argv)
{
    cout << "====== C++ Version of jbpf ======" << endl;
    int ret = 0;
    // pthread_t id1, id2, id3;
    //  set your own logging
    jbpf_set_logging_level(DEBUG);
    jbpf_logger(JBPF_INFO, "Starting jbpf...\n");

    // init jbpf
    struct jbpf_config* jbpf_config = static_cast<struct jbpf_config*>(malloc(sizeof(struct jbpf_config)));
    jbpf_set_default_config_options(jbpf_config);

    if (!handle_signal()) {
        jbpf_logger(JBPF_WARN, "Could not handle SIGINT signal\n");
    }

    if (jbpf_init(jbpf_config) < 0) {
        return -1;
    }

    // pthread_create(&id1, NULL, worker1, NULL);
    // pthread_create(&id2, NULL, worker1, NULL);
    // pthread_create(&id3, NULL, worker1, NULL);

    jbpf_register_thread();

    struct packet p = {1, 2};

    while (1) {
        hook_test1(&p, 1);
        hook_test2(&p, 1);
        usleep(100000);
    }

    jbpf_stop();

    return ret;
}
