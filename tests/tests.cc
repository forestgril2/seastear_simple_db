#include <gtest/gtest.h>

#include <algorithm>
#include <seastar/core/app-template.hh>
#include <seastar/core/file.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/seastar.hh>
#include <seastar/core/semaphore.hh>
#include <iostream>

using namespace seastar;

TEST(SeastarTest, HelloWorldTest) {
    seastar::app_template app;
    app.run_deprecated(0, nullptr, [] {
        return seastar::make_ready_future<>().then([] {
            ASSERT_TRUE(true); // Simple success test
        });
    });
}