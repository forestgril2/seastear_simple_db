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

struct printer {

    printer()
    {
        fmt::print("Printer constructor\n");
    }

    printer(printer&&)
    {
        fmt::print("Printer MOVE constructor\n");
    }

    ~printer()
    {
        fmt::print("Printer destructor\n");
    }

    future<consumption_result<char>> operator()(temporary_buffer<char> buf) {
        if (buf.empty()) {
            fmt::print("Printer buffer is empty, stop consuming return\n");
            return make_ready_future<consumption_result<char>>(stop_consuming(std::move(buf)));
        }
        fmt::print("Printer buffer is not empty, will print the reply now:\n");
        fmt::print("{}\n", sstring(buf.get(), buf.size()));
        return make_ready_future<consumption_result<char>>(continue_consuming());
    }
};

// A LEGIT CLIENT CLOSE:
//
// INFO  2024-10-21 12:15:27,070 seastar - Reactor backend: io_uring
// INFO  2024-10-21 12:15:27,087 seastar - Perf-based stall detector creation failed (EACCESS), try setting /proc/sys/kernel/perf_event_paranoid to 1 or less to enable kernel backtraces: falling back to posix timer.
// ClientTester MOVE constructor
// ClientTester DESTRUCTOR
// Reply status: 200 OK
// Printer constructor
// Printer MOVE constructor
// Printer buffer is not empty, will print the reply now:
// "hello"
// Printer buffer is empty, stop consuming return
// Printer destructor
// Closing reply input stream
// Printer destructor
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
// Printer constructor
// Printer MOVE constructor
// Printer buffer is not empty, will print the reply now:
// "hello"
// Printer MOVE constructor
// Printer destructor
// Printer destructor
// SEGFAULT in seastar::data_source::get()

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

    future<> make_request(const std::string& method, const std::string& path) {
        auto req = http::request::make(method, _host, path);
        return _client->make_request(std::move(req), [this](const http::reply& rep, input_stream<char>&& in) {
            fmt::print("Reply status: {}\n", rep._status);
              // Wrap consume in try/catch to log potential exceptions
            try {
                return in.consume(printer{}).then([in = std::move(in)]() mutable {
                    fmt::print("Closing reply input stream\n");
                    return in.close();
                });
            } catch (const std::exception& e) {
                fmt::print("Error while consuming input stream: {}\n", e.what());
                return make_exception_future<>(std::current_exception());
            }
        });
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


    
future<> request(std::string&& method, std::string&& path) 
{
    auto host = std::string("localhost");
    uint16_t port = 10000;

    return seastar::do_with(ClientTester(host, port), [&] (ClientTester& client) -> future<> {
        co_await seastar::yield().then(seastar::coroutine::lambda([&] () -> future<> {
            co_await seastar::coroutine::maybe_yield();
            try {
                co_await client.connect();
                co_await client.make_request(method, path);
                co_await client.close();
            } catch (const std::exception& e) {
                    fmt::print("Error: {}\n", e.what());
            }
        }));
    });
};