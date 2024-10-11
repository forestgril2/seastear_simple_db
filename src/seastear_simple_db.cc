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

#include "seastar/http/common.hh"
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

void set_routes(routes& r) {
    function_handler* h1 = new function_handler([](const_req req) {
        return "hello";
    });
    function_handler* h2 = new function_handler([](std::unique_ptr<http::request> req) {
        return make_ready_future<json::json_return_type>("json-future");
    });
    r.add(operation_type::GET, url("/"), h1);
    r.add(operation_type::GET, url("/jf"), h2);
    r.add(operation_type::GET, url("/file").remainder("path"),
            new directory_handler("/"));
    demo_json::hello_world.set(r, [] (const_req req) {
        demo_json::my_object obj;
        obj.var1 = req.param.at("var1");
        obj.var2 = req.param.at("var2");
        demo_json::ns_hello_world::query_enum v = demo_json::ns_hello_world::str2query_enum(req.query_parameters.at("query_enum"));
        // This demonstrate enum conversion
        obj.enum_var = v;
        return obj;
    });
}

int main(int ac, char** av) {
    app_template app;

    app.add_options()("port", bpo::value<uint16_t>()->default_value(10000), "HTTP Server port");

    return app.run(ac, av, [&] {
        return seastar::async([&] {
            stop_signal stop_signal;
            auto&& config = app.configuration();

            uint16_t port = config["port"].as<uint16_t>();
            auto server = std::make_unique<http_server_control>();
            auto rb = make_shared<api_registry_builder>("apps/httpd/");
            server->start().get();

            auto stop_server = defer([&] () noexcept {
                std::cout << "Stoppping HTTP server" << std::endl; // This can throw, but won't, they say.
                server->stop().get();
            });

            server->set_routes(set_routes).get();
            server->set_routes([rb](routes& r){rb->set_api_doc(r);}).get();
            server->set_routes([rb](routes& r) {rb->register_function(r, "demo", "hello world application");}).get();
            server->listen(port).get();

            std::cout << "Seastar HTTP server listening on port " << port << " ...\n";

            stop_signal.wait().get();
            return 0;
        });
    });
}
