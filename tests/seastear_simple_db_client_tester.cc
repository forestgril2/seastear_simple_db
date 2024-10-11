/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Version 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownership.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * Copyright (C) 2022 ScyllaDB Ltd.
 */

#include <seastar/core/app-template.hh>
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
    future<consumption_result<char>> operator()(temporary_buffer<char> buf) {
        if (buf.empty()) {
            return make_ready_future<consumption_result<char>>(stop_consuming(std::move(buf)));
        }
        fmt::print("{}\n", sstring(buf.get(), buf.size()));
        return make_ready_future<consumption_result<char>>(continue_consuming());
    }
};

class ClientTester {
public:
    ClientTester(const std::string& host, uint16_t port)
        : _host(host), _port(port) {}

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
            return in.consume(printer{}).then([in = std::move(in)]() mutable {
                return in.close();
            });
        });
    }

    future<> close() {
        if (_client) {
            return _client->close();
        } else {
            return make_ready_future<>();
        }
    }

private:
    std::string _host;
    uint16_t _port;
    std::unique_ptr<http::experimental::client> _client;
};

int main(int ac, char** av) {
    app_template app;

    auto host = std::string("localhost");
    auto path = std::string("/");
    auto method = std::string("GET");
    uint16_t port = 10000;

    return app.run(ac, av, [&] -> future<> {
        return seastar::do_with(ClientTester(host, port), [=](ClientTester& client) -> future<> {
            try {
                co_await client.connect();
                co_await client.make_request(method, path);
                co_await client.close();

            } catch (const std::exception& e) {
                    fmt::print("Error: {}\n", e.what());
            }
            co_return;
        });
    });
}