#include "seastar/core/temporary_buffer.hh"
#include <string>

#include <seastar/coroutine/maybe_yield.hh>
#include <seastar/core/coroutine.hh>
#include <seastar/core/do_with.hh>
#include <seastar/util/later.hh>

#include <seastar/core/reactor.hh>
#include <seastar/core/shared_ptr.hh>
#include <seastar/core/iostream.hh>
#include <seastar/core/fstream.hh>
#include <seastar/core/thread.hh>
#include <seastar/http/client.hh>
#include <seastar/http/request.hh>
#include <seastar/http/reply.hh>
#include <seastar/net/inet_address.hh>
#include <seastar/net/dns.hh>
#include <seastar/net/tls.hh>

using namespace seastar;
namespace bpo = boost::program_options;

struct StreamConsumer {

    StreamConsumer()
    {
        //fmt::print("StreamConsumer constructor\n");
    }

    StreamConsumer(StreamConsumer&&)
    {
        //fmt::print("StreamConsumer MOVE constructor\n");
    }

    ~StreamConsumer()
    {
        //fmt::print("StreamConsumer destructor\n");
    }

    future<consumption_result<char>> operator()(temporary_buffer<char> buf) {
        if (buf.empty()) {
            //fmt::print("StreamConsumer buffer is empty, stop consuming return\n");
            return make_ready_future<consumption_result<char>>(stop_consuming(std::move(buf)));
        }
        //fmt::print("StreamConsumer buffer is not empty, printing the reply:\n");

        result = std::string(buf.get(), buf.size());
        //fmt::print("{}\n", result);
        return make_ready_future<consumption_result<char>>(continue_consuming());
    }
    std::string result;
};
class ClientTester {
public:
    ClientTester(const std::string& host, uint16_t port)
        : _host(host), _port(port) {}
    
    ClientTester(const ClientTester& other) = delete;

    ClientTester(ClientTester&& other)
        : _host(other._host), _port(other._port), _client(std::move(other._client))
    {
        //fmt::print("ClientTester MOVE constructor\n");
    }

    ~ClientTester()
    {
        //fmt::print("ClientTester DESTRUCTOR\n");
    }

    future<> connect() {
        return net::dns::get_host_by_name(_host, net::inet_address::family::INET).then([this](net::hostent e) {
            auto addr = e.addr_list.front();
            socket_address address(addr, _port);
            _client = std::make_unique<http::experimental::client>(address);
            return make_ready_future<>();
        });
    }

    // TODO: Prepare a version with a body writer, rather than a body string.
    future<std::string> make_request(const std::string& method, const std::string& path, const std::string& body) {
            std::string result;
            auto req = http::request::make(method, _host, path);
            req.write_body("json",body);
            co_await _client->make_request(std::move(req), [&result, this](const http::reply& rep, input_stream<char>&& in) -> future<> {
            //fmt::print("Reply status: {}\n", rep._status);
            try {
                    StreamConsumer consumer;
                    co_await in.consume(consumer);
                    //fmt::print("Closing reply input stream\n");
                    co_await in.close();
                    //fmt::print("Storing input stream reply\n");
                    result = std::move(consumer.result);
                }
            catch (const std::exception& e) {
                fmt::print("Error while consuming input stream: {}\n", e.what());
                throw;
            }
            co_return;
        });
        co_return result;
    }

    future<> close() {
        if (_client) {
            //fmt::print("Closing client\n");
            return _client->close();
        } else {
            fmt::print("Client doesn't exist already (can't close)\n");
            return make_ready_future<>();
        }
    }

private:
    std::string _host;
    uint16_t _port;
    std::unique_ptr<http::experimental::client> _client;
};


future<std::string> request(const std::string& method, const std::string& path, const std::string& body = "") {
    return seastar::async([=]() -> std::string {
        // Prepare the curl command
        std::string cmd = "curl -X " + method + " localhost:10000" + path;
        
        // If body exists, append it as data for POST requests
        if (!body.empty()) {
            cmd += " -d '" + body + "'";
        }

        // Capture the output of the curl command
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) {
            throw std::runtime_error("popen() failed!");
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }

        return result;
    });
}