#define BOOST_BIND_GLOBAL_PLACEHOLDERS

#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <iostream>
#include <thread>

#include "parser.hpp"

#include "jbpf_lcm_api.h"
#include "jbpf_lcm_ipc.h"
#include "jbpf_common.h"
#include "jbpf_version.h"

#define API_VERSION JBPF_VERSION_MAJOR "." JBPF_VERSION_MINOR "." JBPF_VERSION_PATCH "-" JBPF_COMMIT_HASH
#define THREAD_COUNT 1

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace po = boost::program_options;

namespace jbpf_reverse_proxy {
namespace listener {

using tcp = boost::asio::ip::tcp;

std::string address;
jbpf_lcm_ipc_address_t destination = {0};

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template <class Body, class Allocator, class Send>
void
handle_request(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send)
{
    // Returns a bad request response
    auto const bad_request = [&req](beast::string_view why) {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, API_VERSION);
        res.set(http::field::content_type, "application/text");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(why);
        res.prepare_payload();
        return res;
    };

    auto const success = [&req](http::status status_code) {
        http::response<http::empty_body> res{status_code, req.version()};
        res.set(http::field::server, API_VERSION);
        res.content_length(0);
        res.keep_alive(req.keep_alive());
        return res;
    };

    // Returns a server error response
    auto const server_error = [&req](beast::string_view what) {
        http::response<http::string_body> res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, API_VERSION);
        res.set(http::field::content_type, "application/text");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + std::string(what) + "'";
        res.prepare_payload();
        return res;
    };

    switch (req.method()) {
    case http::verb::post: {
        auto req_body = req.body();
        boost::property_tree::ptree pt;
        std::istringstream is(std::string(req_body.data(), req_body.size()));
        read_json(is, pt);
        jbpf_codeletset_load_req load_req = {0};

        std::vector<std::string> codeletset_elems;
        codeletset_elems.push_back(address);
        if (jbpf_reverse_proxy::parser::parse_jbpf_codeletset_load_req(pt, &load_req, codeletset_elems))
            return send(bad_request("Invalid JSON"));

        std::cout << "Loading codelet set: " << load_req.codeletset_id.name << std::endl;

        auto ret = jbpf_lcm_ipc_send_codeletset_load_req(&destination, &load_req);
        if (ret != JBPF_LCM_IPC_REQ_SUCCESS)
            return send(server_error("Request failed with response " + std::to_string(ret)));

        return send(success(http::status::created));
    }

    case http::verb::delete_: {
        auto codeletset_id = req.target();
        codeletset_id = codeletset_id.substr(1, codeletset_id.size() - 1);
        jbpf_codeletset_unload_req_s unload_req = {0};
        if (codeletset_id.length() > JBPF_CODELETSET_NAME_LEN - 1)
            return send(bad_request(
                "Expected codeletset_id.name length must be at most " + std::to_string(JBPF_CODELETSET_NAME_LEN - 1)));
        codeletset_id.copy(unload_req.codeletset_id.name, JBPF_CODELETSET_NAME_LEN - 1);

        std::cout << "Unloading codelet set: " << unload_req.codeletset_id.name << std::endl;

        auto ret = jbpf_lcm_ipc_send_codeletset_unload_req(&destination, &unload_req);
        if (ret != JBPF_LCM_IPC_REQ_SUCCESS)
            return send(server_error("Request failed with response " + std::to_string(ret)));

        return send(success(http::status::no_content));
    }
    }

    return send(bad_request("Unknown HTTP-method"));
}

// Report a failure
void
fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Handles an HTTP server connection
class session : public std::enable_shared_from_this<session>
{
    // This is the C++11 equivalent of a generic lambda.
    // The function object is used to send an HTTP message.
    struct send_lambda
    {
        session& self_;

        explicit send_lambda(session& self) : self_(self) {}

        template <bool isRequest, class Body, class Fields>
        void
        operator()(http::message<isRequest, Body, Fields>&& msg) const
        {
            // The lifetime of the message has to extend
            // for the duration of the async operation so
            // we use a shared_ptr to manage it.
            auto sp = std::make_shared<http::message<isRequest, Body, Fields>>(std::move(msg));

            // Store a type-erased version of the shared
            // pointer in the class to keep it alive.
            self_.res_ = sp;

            // Write the response
            http::async_write(
                self_.stream_,
                *sp,
                beast::bind_front_handler(&session::on_write, self_.shared_from_this(), sp->need_eof()));
        }
    };

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    std::shared_ptr<void> res_;
    send_lambda lambda_;

  public:
    // Take ownership of the stream
    session(tcp::socket&& socket) : stream_(std::move(socket)), lambda_(*this) {}

    // Start the asynchronous operation
    void
    run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        net::dispatch(stream_.get_executor(), beast::bind_front_handler(&session::do_read, shared_from_this()));
    }

    void
    do_read()
    {
        // Make the request empty before reading,
        // otherwise the operation behavior is undefined.
        req_ = {};

        // Set the timeout.
        stream_.expires_after(std::chrono::seconds(30));

        // Read a request
        http::async_read(stream_, buffer_, req_, beast::bind_front_handler(&session::on_read, shared_from_this()));
    }

    void
    on_read(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if (ec == http::error::end_of_stream)
            return do_close();

        if (ec)
            return fail(ec, "read");

        // Send the response
        handle_request(std::move(req_), lambda_);
    }

    void
    on_write(bool close, beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "write");

        if (close) {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            return do_close();
        }

        // We're done with the response so delete it
        res_ = nullptr;

        // Read another request
        do_read();
    }

    void
    do_close()
    {
        // Send a TCP shutdown
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

        // At this point the connection is closed gracefully
    }
};

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
    net::io_context& ioc_;
    tcp::acceptor acceptor_;

  public:
    listener(net::io_context& ioc, tcp::endpoint endpoint) : ioc_(ioc), acceptor_(net::make_strand(ioc))
    {
        beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            fail(ec, "open");
            return;
        }

        // Allow address reuse
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) {
            fail(ec, "set_option");
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if (ec) {
            fail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) {
            fail(ec, "listen");
            return;
        }
    }

    // Start accepting incoming connections
    void
    run()
    {
        do_accept();
    }

  private:
    void
    do_accept()
    {
        // The new connection gets its own strand
        acceptor_.async_accept(
            net::make_strand(ioc_), beast::bind_front_handler(&listener::on_accept, shared_from_this()));
    }

    void
    on_accept(beast::error_code ec, tcp::socket socket)
    {
        if (ec)
            fail(ec, "accept");
        else
            // Create the session and run it
            std::make_shared<session>(std::move(socket))->run();

        // Accept another connection
        do_accept();
    }
};

int
run_listener(int ac, char** av)
{
    std::stringstream default_address;
    default_address << JBPF_DEFAULT_RUN_PATH << "/" << JBPF_DEFAULT_LCM_SOCKET;
    address = default_address.str();
    uint16_t default_port = 8080;
    auto default_ip = "0.0.0.0";
    po::options_description desc("Allowed options");
    desc.add_options()("help", "produce help message")(
        "host-ip", po::value<std::string>()->implicit_value(default_ip), "host ip for http server")(
        "host-port", po::value<uint16_t>()->implicit_value(default_port), "host port for http server")(
        "address", po::value<std::string>()->implicit_value(default_address.str()), "jbpf lcm ipc address");

    po::variables_map vm;
    po::store(po::parse_command_line(ac, av, desc), vm);

    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return EXIT_SUCCESS;
    }

    if (vm.count("address"))
        address = vm.at("address").as<std::string>();

    if (address.length() > JBPF_LCM_IPC_ADDRESS_LEN - 1) {
        std::cerr << "--address length must be at most " << JBPF_LCM_IPC_ADDRESS_LEN - 1 << std::endl;
        return EXIT_FAILURE;
    }

    address.copy(destination.path, JBPF_LCM_IPC_ADDRESS_LEN - 1);
    destination.path[address.length()] = '\0';

    // The io_context is required for all I/O
    net::io_context ioc{THREAD_COUNT};

    auto addr = net::ip::make_address(vm.count("host-ip") ? vm.at("host-ip").as<std::string>() : default_ip);
    auto port = vm.count("host-port") ? vm.at("host-port").as<uint16_t>() : default_port;

    // Create and launch a listening port
    std::make_shared<listener>(ioc, tcp::endpoint{addr, port})->run();

    std::cout << "Forwarding messages to: " << address << std::endl;
    std::cout << "Listening on: " << addr << ":" << port << std::endl;

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(THREAD_COUNT - 1);
    for (auto i = THREAD_COUNT - 1; i > 0; --i)
        v.emplace_back([&ioc] { ioc.run(); });
    ioc.run();

    return EXIT_SUCCESS;
}

} // namespace listener
} // namespace jbpf_reverse_proxy