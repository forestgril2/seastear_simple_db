#include "seastar/core/temporary_buffer.hh"
#include <cstddef>
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
        fmt::print("StreamConsumer constructor\n");
    }

    StreamConsumer(StreamConsumer&&)
    {
        fmt::print("StreamConsumer MOVE constructor\n");
    }

    ~StreamConsumer()
    {
        fmt::print("StreamConsumer destructor\n");
    }

    future<consumption_result<char>> operator()(temporary_buffer<char> buf) {
        if (buf.empty()) {
            fmt::print("StreamConsumer buffer is empty, stop consuming return\n");
            return make_ready_future<consumption_result<char>>(stop_consuming(std::move(buf)));
        }
        fmt::print("StreamConsumer buffer is not empty, printing the reply:\n");

        result = std::string(buf.get(), buf.size());
        fmt::print("{}\n", result);
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
        fmt::print("ClientTester MOVE constructor\n");
    }

    ~ClientTester()
    {
        fmt::print("ClientTester DESTRUCTOR\n");
    }

    future<> connect() {
        return net::dns::get_host_by_name(_host, net::inet_address::family::INET).then([this](net::hostent e) {
            auto addr = e.addr_list.front();
            socket_address address(addr, _port);
            _client = std::make_unique<http::experimental::client>(address);
            return make_ready_future<>();
        });
    }

    future<std::string> make_request(const std::string& method, const std::string& path) {
            std::string result;
            co_await _client->make_request(http::request::make(method, _host, path), [&result, this](const http::reply& rep, input_stream<char>&& in) -> future<> {
            fmt::print("Reply status: {}\n", rep._status);
            try {
                    StreamConsumer consumer;
                    co_await in.consume(consumer);
                    fmt::print("Closing reply input stream\n");
                    co_await in.close();
                    fmt::print("Storing input stream reply\n");
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
            fmt::print("Closing client\n");
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


    
future<std::string> request(const std::string& method, const std::string& path) 
{
    auto host = std::string("localhost");
    uint16_t port = 10000;

    return seastar::do_with(ClientTester(host, port), [&] (ClientTester& client) -> future<std::string> {
        co_return co_await seastar::yield().then(seastar::coroutine::lambda([&] () -> future<std::string> {
            co_await seastar::coroutine::maybe_yield();
            std::string res;
            try {
                co_await client.connect();
                res = co_await client.make_request(method, path);
                co_await client.close();
            } catch (const std::exception& e) {
                    fmt::print("Error: {}\n", e.what());
            }
            co_return res;
        }));
    });
};

// A LEGIT CLIENT CLOSE:
//
// INFO  2024-10-21 12:15:27,070 seastar - Reactor backend: io_uring
// INFO  2024-10-21 12:15:27,087 seastar - Perf-based stall detector creation failed (EACCESS), try setting /proc/sys/kernel/perf_event_paranoid to 1 or less to enable kernel backtraces: falling back to posix timer.
// ClientTester MOVE constructor
// ClientTester DESTRUCTOR
// Reply status: 200 OK
// StreamConsumer constructor
// StreamConsumer MOVE constructor
// StreamConsumer buffer is not empty, will print the reply now:
// "hello"
// StreamConsumer buffer is empty, stop consuming return
// StreamConsumer destructor
// Closing reply input stream
// StreamConsumer destructor
// Closing client
// ClientTester DESTRUCTOR
// [1] + Done                       "/usr/bin/gdb" --interpreter=mi --tty=${DbgTerm} 0<"/tmp/Microsoft-MIEngine-In-cufvinrr.5kc" 1>"/tmp/Microsoft-MIEngine-Out-jvw5nsya.ihm"
// vboxuser@Ubuntu24:~/
// 
// FAILING WIHT A SEGFAULT in seastar::data_source::get()
//
// INFO  2024-10-21 12:17:19,803 seastar - Perf-based stall detector creation failed (EACCESS), try setting /proc/sys/kernel/perf_event_paranoid to 1 or less to enable kernel backtraces: falling back to posix timer.
// ClientTester MOVE constructor
// ClientTester DESTRUCTOR
// Reply status: 200 OK
// StreamConsumer constructor
// StreamConsumer MOVE constructor
// StreamConsumer buffer is not empty, will print the reply now:
// "hello"
// StreamConsumer MOVE constructor
// StreamConsumer destructor
// StreamConsumer destructor
// SEGFAULT in seastar::data_source::get()
