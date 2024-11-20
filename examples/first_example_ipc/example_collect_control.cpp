#define BOOST_BIND_GLOBAL_PLACEHOLDERS

#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <unistd.h>

#include "jbpf.h"
#include "jbpf_defs.h"
#include "jbpf_io.h"
#include "jbpf_io_channel.h"

#include "common.h"

#define IPC_NAME "example_ipc_app"

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

static void
handle_channel_bufs(
    struct jbpf_io_channel* io_channel, struct jbpf_io_stream_id* stream_id, void** bufs, int num_bufs, void* ctx)
{
    struct jbpf_io_ctx* io_ctx = static_cast<struct jbpf_io_ctx*>(ctx);

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

            // Send an acknowledgement message back to the codelet using IPC API
            PacketResp resp = {p.seq_no};
            jbpf_io_channel_send_msg(io_ctx, &control_input_stream_id, &resp, sizeof(resp));

            jbpf_io_channel_release_buf(bufs[i]);
        }
    }
}

int
main(int argc, char** argv)
{

    struct jbpf_io_config io_config = {0};
    struct jbpf_io_ctx* io_ctx;

    // Designate the data collection framework as a primary for the IPC
    io_config.type = JBPF_IO_IPC_PRIMARY;

    strncpy(io_config.ipc_config.addr.jbpf_io_ipc_name, IPC_NAME, JBPF_IO_IPC_MAX_NAMELEN);

    // Configure memory size for the IO buffer
    io_config.ipc_config.mem_cfg.memory_size = JBPF_HUGEPAGE_SIZE_1GB;

    // Configure the jbpf agent to operate in shared memory mode
    io_ctx = jbpf_io_init(&io_config);

    if (!io_ctx) {
        return -1;
    }

    // Every thread that sends or receives jbpf data needs to be registered using this call
    jbpf_io_register_thread();

    while (true) {
        // Continuously poll IPC output buffers
        jbpf_io_channel_handle_out_bufs(io_ctx, handle_channel_bufs, io_ctx);
        sleep(1);
    }

    return 0;
}
