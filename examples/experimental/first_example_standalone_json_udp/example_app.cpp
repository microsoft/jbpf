// Copyright (c) Microsoft Corporation. All rights reserved.
#define BOOST_BIND_GLOBAL_PLACEHOLDERS

#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <string>
#include <cstring>
#include <csignal>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"

#define JBPF_EXPERIMENTAL_FEATURES
#include "jbpf.h"
#include "jbpf_hook.h"
#include "jbpf_defs.h"

using namespace std;

struct output_socket
{
    int sockfd;
    struct sockaddr_in server_addr;
};

// Hook declaration and definition.
DECLARE_JBPF_HOOK(
    example,
    struct jbpf_generic_ctx ctx,
    ctx,
    HOOK_PROTO(Packet* p, int ctx_id),
    HOOK_ASSIGN(ctx.ctx_id = ctx_id; ctx.data = (uint64_t)(void*)p; ctx.data_end = (uint64_t)(void*)(p + 1);))

DEFINE_JBPF_HOOK(example)

// A unique stream id that is used for the control input channel. This ID is defined in the codeletset definition file
// ("codeletset_load_request.yaml")
jbpf_io_stream_id_t control_input_stream_id = {
    .id = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11}};

// A unique stream id that is used for the output channel. This ID is defined in the codeletset definition file
// ("codeletset_load_request.yaml")
jbpf_io_stream_id_t output_data_stream_id = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

// Handler function that is invoked every time that jbpf receives one or more buffers of data from a codelet
static void
io_channel_print_output(jbpf_io_stream_id_t* stream_id, void** bufs, int num_bufs, void* ctx)
{
    output_socket* s = (output_socket*)ctx;
    if (stream_id && num_bufs > 0) {

        // Check that the data corresponds to a known output channel
        if (memcmp(&output_data_stream_id, stream_id, sizeof(jbpf_io_stream_id_t)) != 0) {
            std::cout << "ERROR: Unkown stream_id" << std::endl;
            return;
        }

        // Fetch the data and print in JSON format
        for (auto i = 0; i < num_bufs; i++) {
            char buffer[2048];
            int msg_size = jbpf_io_channel_pack_msg(jbpf_get_io_ctx(), bufs[i], buffer, 2048);

            if (msg_size > 0) {
                int sent_bytes =
                    sendto(s->sockfd, buffer, msg_size, 0, (struct sockaddr*)&s->server_addr, sizeof(s->server_addr));
                if (sent_bytes <= 0) {
                    std::cout << "Unable to send message to collector" << std::endl;
                }
            } else {
                std::cout << "Unable to serialize message" << std::endl;
            }

            Packet p = *static_cast<Packet*>(bufs[i]);

            // Send an acknowledgement message back to the codelet
            PacketResp resp = {p.seq_no};
            jbpf_send_input_msg(&control_input_stream_id, &resp, sizeof(resp));
        }
    }
}

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
    output_socket s;

    // Create a UDP socket to send messages out
    s.sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (s.sockfd < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }

    memset(&s.server_addr, 0, sizeof(s.server_addr));
    s.server_addr.sin_family = AF_INET;
    s.server_addr.sin_port = htons(8888);
    s.server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    struct jbpf_config jbpf_config = {0};
    jbpf_set_default_config_options(&jbpf_config);

    // Enable LCM IPC interface using UNIX socket at the default socket path (the default is through C API)
    jbpf_config.lcm_ipc_config.has_lcm_ipc_thread = true;
    snprintf(
        jbpf_config.lcm_ipc_config.lcm_ipc_name,
        sizeof(jbpf_config.lcm_ipc_config.lcm_ipc_name) - 1,
        "%s",
        JBPF_DEFAULT_LCM_SOCKET);

    jbpf_config.io_config.io_type = JBPF_IO_THREAD_CONFIG;
    jbpf_config.io_config.io_thread_config.output_handler_ctx = &s;

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

    // Register the callback to handle output messages from codelets
    jbpf_register_io_output_cb(io_channel_print_output);

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
