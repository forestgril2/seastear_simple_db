#include <algorithm>
#include <seastar/core/app-template.hh>
#include <seastar/core/file.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/seastar.hh>
#include <seastar/core/semaphore.hh>
#include <iostream>

using namespace seastar;

logger applog("app");

int main(int argc, char** argv) {
    seastar::app_template app;
    return app.run(argc, argv, [] {
        return seastar::make_ready_future<>().then([] {
            std::cout << "Hello, World!" << std::endl;
        });
    });
}
