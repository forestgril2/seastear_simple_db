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
#include "ClientTester.hh"

int main(int ac, char** av) {
    app_template app;

    auto host = std::string("localhost");
    auto path = std::string("/");
    auto method = std::string("GET");
    uint16_t port = 10000;
    
    return app.run(ac, av, [=] -> future<> {
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
    });
}