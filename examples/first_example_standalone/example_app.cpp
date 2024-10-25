// Copyright (c) Microsoft Corporation. All rights reserved.
#define BOOST_BIND_GLOBAL_PLACEHOLDERS

#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <string>
#include <cstring>
#include <csignal>

#include "common.h"

#include "jbpf.h"
#include "jbpf_hook.h"
#include "jbpf_defs.h"

using namespace std;

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

// Used to covert data of type Packet to JSON format
boost::property_tree::ptree
toPtree(const Packet& packet)
{
    boost::property_tree::ptree pt;
    pt.put("seq_no", packet.seq_no);
    pt.put("value", packet.value);
    pt.put("name", std::string(packet.name));
    return pt;
}

std::string
toJson(const boost::property_tree::ptree& pt)
{
    std::ostringstream oss;
    boost::property_tree::write_json(oss, pt);
    return oss.str();
};

// Handler function that is invoked every time that jbpf receives one or more buffers of data from a codelet
static void
io_channel_print_output(jbpf_io_stream_id_t* stream_id, void** bufs, int num_bufs, void* ctx)
{
    if (stream_id && num_bufs > 0) {

        // Check that the data corresponds to a known output channel
        if (memcmp(&output_data_stream_id, stream_id, sizeof(jbpf_io_stream_id_t)) != 0) {
            std::cout << "ERROR: Unknown stream_id" << std::endl;
            return;
        }

        // Fetch the data and print in JSON format
        for (auto i = 0; i < num_bufs; i++) {

            Packet p = *static_cast<Packet*>(bufs[i]);

            boost::property_tree::ptree pt = toPtree(p);
            std::string json = toJson(pt);

            std::cout << json << std::endl;

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

    struct jbpf_config jbpf_config = {0};
    jbpf_set_default_config_options(&jbpf_config);

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
