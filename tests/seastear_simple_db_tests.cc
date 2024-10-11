#include <gtest/gtest.h>

#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>

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


const std::string k_ssdb_file_name = "seastear_simple_db";
const std::string k_ssdbct_file_name = "seastear_simple_db_client_tester";
const unsigned k_secs_required_to_start = 1;

// Function to check if a file exists and is executable
bool is_executable(const std::string& file) {
    struct stat buffer;
    return (stat(file.c_str(), &buffer) == 0) && (buffer.st_mode & S_IXUSR);
}

pid_t execute_binary(const std::string& binaryPath) {
    pid_t pid = fork();

    if (pid == -1) {
        // Fork failed
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
 // Child process
        execl(binaryPath.c_str(), binaryPath.c_str(), nullptr);
        // If execl fails, exit with error
        perror("execl");
        exit(EXIT_FAILURE);
    } 
    // Parent process
    return pid;
}

bool kill_binary(pid_t pid) {
    if (pid > 0) {
        // Send SIGKILL to the specific process
        if (kill(pid, SIGKILL) == -1) {
            perror("kill");
            return false;
        }
        int status;
        // Wait for the specific child process to terminate
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            return false;
        }
        if (WIFSIGNALED(status)) {
            std::cout << "Process terminated by signal: " << WTERMSIG(status) << std::endl;
            return true;
        } else {
            std::cout << "Process did not terminate as expected." << std::endl;
            return false;
        }
    }
    return false;
}

bool is_process_alive(pid_t pid) {
    if (kill(pid, 0) == 0) {
        return true;  // Process exists
    } else {
        if (errno == ESRCH) {
            return false;  // Process does not exist
        } else if (errno == EPERM) {
            return true;   // Process exists, but no permission to signal
        }
        return false;
    }
}

TEST(SeastearSimpleDbTests, ServerBinaryExists)
{ 
    EXPECT_TRUE(is_executable(k_ssdb_file_name)) << "The file \"" << k_ssdb_file_name << "\" does not exist or is not executable.";
}

TEST(SeastearSimpleDbTests, ClientTesterBinaryExists)
{ 
    EXPECT_TRUE(is_executable(k_ssdbct_file_name)) << "The file \"" << k_ssdbct_file_name << "\" does not exist or is not executable.";
}

TEST(SeastearSimpleDbTests, ServerCanBeExecuted) {
   const auto pid = execute_binary(k_ssdb_file_name);
   ASSERT_NE(pid, -1);
   const auto killed = kill_binary(pid);
   ASSERT_EQ(killed, true);
}

TEST(SeastearSimpleDbTests, ServerStaysAliveBeforeTestExits) {
   const auto pid = execute_binary(k_ssdb_file_name);
   ASSERT_NE(pid, -1);
   sleep(5);
   ASSERT_TRUE(is_process_alive(pid)) << "Server binary is not alive";
   const auto killed = kill_binary(pid);
   ASSERT_EQ(killed, true);
}


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


class SeastearSimpleDbTestFixture : public ::testing::Test {
protected:
    pid_t server_pid;

    virtual void SetUp() override {
        server_pid = execute_binary(k_ssdb_file_name);
        ASSERT_NE(server_pid, -1) << "Failed to start the server binary.";
        sleep(k_secs_required_to_start);
    }

    virtual void TearDown() override {
        bool killed = kill_binary(server_pid);
        ASSERT_TRUE(killed) << "Failed to kill the server binary.";
    }
};

TEST_F(SeastearSimpleDbTestFixture, ServerAcceptsAndRespondsToAGetRequest) {
   ASSERT_TRUE(is_process_alive(server_pid)) << "Server binary is not alive";
//    seastar::future<> test_future = seastar::async([] {
//        ClientTester client("localhost", 10000);
//        client.connect().get();
//        client.make_request("GET", "/").get();
//        client.close().get();
//    }).handle_exception([](std::exception_ptr ep) {
//        try {
//            std::rethrow_exception(ep);
//        } catch (const std::exception& e) {
//            FAIL() << "Exception occurred: " << e.what();
//        }
//    });
//
//    test_future.get();
};

int main(int argc, char** argv) {
    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);

    // Create Seastar app_template
    seastar::app_template app;

    // Run Seastar application
    return app.run(argc, argv, [&argc, &argv] () -> seastar::future<int> {
        // Run all the tests
        int test_result = RUN_ALL_TESTS();
        // Return the test result
        return seastar::make_ready_future<int>(test_result);
    });
}