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
 * Copyright 2015 Cloudius Systems
 */

#include "seastar/core/future.hh"
#include "seastar/core/io_queue.hh"
#include "seastar/core/sharded.hh"
#include "seastar/http/common.hh"
#include <optional>
#include <seastar/core/app-template.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/shared_ptr.hh>
#include <seastar/core/iostream.hh>
#include <seastar/core/fstream.hh>
#include <seastar/core/thread.hh>
#include <seastar/http/function_handlers.hh>
#include <seastar/http/httpd.hh>
#include <seastar/json/json_elements.hh>
#include <seastar/net/socket_defs.hh>
#include <seastar/http/httpd.hh>
#include <seastar/http/handlers.hh>
#include <seastar/http/function_handlers.hh>
#include <seastar/http/file_handler.hh>
#include <seastar/core/seastar.hh>
#include <seastar/core/reactor.hh>
#include "demo.json.hh"
#include <seastar/http/api_docs.hh>
#include <seastar/core/thread.hh>
#include <seastar/core/prometheus.hh>
#include <seastar/core/print.hh>
#include <seastar/net/inet_address.hh>
#include <seastar/util/defer.hh>
#include <seastar/core/signal.hh>
#include <unordered_map>


extern const std::string server_hello_message;

namespace bpo = boost::program_options;

using namespace seastar;
using namespace httpd;

class stop_signal {
    bool _caught = false;
    seastar::condition_variable _cond;
private:
    void signaled() {
        if (_caught) {
            return;
        }
        _caught = true;
        _cond.broadcast();
    }
public:
    stop_signal() {
        seastar::handle_signal(SIGINT, [this] { signaled(); });
        seastar::handle_signal(SIGTERM, [this] { signaled(); });
    }
    ~stop_signal() {
        // There's no way to unregister a handler yet, so register a no-op handler instead.
        seastar::handle_signal(SIGINT, [] {});
        seastar::handle_signal(SIGTERM, [] {});
    }
    seastar::future<> wait() {
        return _cond.wait([this] { return _caught; });
    }
    bool stopping() const {
        return _caught;
    }
};

struct Store 
{
   seastar::future<> stop() 
   {
        return make_ready_future<>();
    } 

   future<bool> put(std::string&& key, std::string&& val)
   {
        key_vals.insert_or_assign(std::move(key), std::move(val));
        co_return co_await make_ready_future<bool>(true);
    }

   future<std::optional<std::string>> get(const std::string& key)
   {
        auto it = key_vals.find(key);
        if (it==key_vals.end())
            co_return co_await make_ready_future<std::optional<std::string>>(std::nullopt);
        co_return co_await make_ready_future<std::optional<std::string>>(it->second);
    };

    std::unordered_map<std::string, std::string> key_vals;
};

int main(int ac, char** av) {
    app_template app;
    sharded<Store> db;
    app.add_options()("port", bpo::value<uint16_t>()->default_value(10000), "HTTP Server port");

    return app.run(ac, av, [&] ()->seastar::future<int> {
            stop_signal stop_signal;
            auto&& config = app.configuration();

            sharded<Store> db;
            co_await db.start();

            uint16_t port = config["port"].as<uint16_t>();
            auto server = std::make_unique<http_server_control>();
            server->start().get();

            auto stop_server = defer([&] () noexcept {
                std::cout << "Stoppping HTTP server" << std::endl; // This can throw, but won't, they say.
                server->stop().get();
            });

            co_await server->set_routes([&db](routes& r) {
                function_handler* hello_handler = new function_handler([](const_req req) {
                    return server_hello_message;
                });

                function_handler* put_val_handler = new function_handler([&db](const_req req) -> std::string {
                    const auto key = req.param.get_decoded_param("key");
                    if (key.size() > 255){
                        throw std::invalid_argument("Key must be a valid UTF-8 string up to 255 bytes.");
                    }
                    // This could be dynamic, based on the cache and/or disk size of the shard. 
                    uint64_t key_hash = std::hash<std::string>{}(key);
                    unsigned shard_id = key_hash % seastar::smp::count;

                    return std::string("OK");
                });
                
                function_handler* get_val_handler = new function_handler([&db](const_req req) -> std::string {
                    const auto key = req.param.get_decoded_param("key");
                    // This could be dynamic, based on the cache and/or disk size of the shard. 
                    uint64_t key_hash = std::hash<std::string>{}(key);
                    unsigned shard_id = key_hash % seastar::smp::count;

                    std::optional<std::string> resp = db.invoke_on(shard_id, [key](auto& instance) -> future<std::optional<std::string>>{
                        co_return co_await instance.get(key);
                    }).get();
                    //TODO: fix that in case not found. Probably possible to change the reply to something that is not directly a string.
                    return resp.value_or("404 not found");
                });

                r.add(operation_type::GET, url("/"), hello_handler);
                r.add(operation_type::GET, url("/").remainder("key"), get_val_handler);
                r.add(operation_type::PUT, url("/").remainder("key"), put_val_handler);
            });

            co_await server->listen(port);

            std::cout << "Seastar HTTP server listening on port " << port << " ...\n";

            co_await stop_signal.wait();
            co_await db.stop();
            co_return co_await make_ready_future<int>(0);
    });
}
