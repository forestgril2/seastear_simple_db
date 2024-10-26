#include <exception>
#include <format>
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

#include "ClientTester.hh"
#include "seastar/core/future.hh"

extern const std::string server_hello_message;

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


//class SeastearSimpleDbTestFixture : public ::testing::Test {
//protected:
//    pid_t server_pid;
//
//    void SetUp() {
//         // Initialize the ClientTester for each test case
//        server_pid = execute_binary(k_ssdb_file_name);
//        ASSERT_NE(server_pid, -1) << "Failed to start the server binary.";
//        sleep(k_secs_required_to_start);
//    }
//
//    void TearDown() {
//        bool killed = kill_binary(server_pid);
//        ASSERT_TRUE(killed) << "Failed to kill the server binary.";
//    }
//};
//
//TEST_F(SeastearSimpleDbTestFixture, ClientBinarySendsGetMultipleTimesInARow) {
//    ASSERT_TRUE(is_process_alive(server_pid)) << "Server binary is not alive";
//
//    pid_t client_pid;
//    for(int i=0; i<100; ++i)
//    {
//        client_pid = execute_binary(k_ssdbct_file_name);
//        ASSERT_NE(client_pid, -1) << "Failed to start the client binary.";
//
//        usleep(500000);
//
//    }
//}
//


struct DbRespTest
{
    DbRespTest()
    {
        std::cout << " ################################" << std::endl;
        std::cout << " ### Starting a DB test suite ###" << std::endl;
        init();
    }

    ~DbRespTest() 
    {
        finalize();
        std::cout << " ### FINISHED DB test suite " << std::endl;
        if (!failed_cases.empty())
        {
            std::cout << " ### Suite FAILURE! Failed cases listed below. ###" << std::endl;
            for (auto fc : failed_cases)
            {
                std::cout << "     " << fc << std::endl;
            }
        }
        else
        {
            std::cout << " ### SUCCESS. Suite tests passed ###" << std::endl;
        }
    }

    future<bool> test_GET(std::string&& key, std::string&& res_expect)
    {
        co_return co_await test_req("GET", std::string("//") + std::move(key), "",std::move(res_expect));
    }

    future<bool> test_GET(const std::string& key, const std::string& res_expect)
    {
        co_return co_await test_req("GET", std::string("//") + key, "", res_expect);
    }

    future<bool> test_PUT(const std::string& key, const std::string& val, const std::string& res_expect)
    {
        co_return co_await test_req("PUT", std::string("//") + key, val, res_expect);
    }

    future<bool> test_PUT(std::string&& key, std::string&& val, std::string&& res_expect)
    {
        co_return co_await test_req("PUT", std::string("//") + std::move(key), std::move(val),std::move(res_expect));
    }

    future<bool> test_req(std::string&& method, std::string&& path, std::string&& body, std::string&& res_expect)
    {
        const auto response =  co_await request(method, path, body);
        co_return co_await seastar::make_ready_future<bool>(check(
            response, std::move(method), std::move(path), std::move(body), std::move(res_expect)
        )); 
    }

    future<bool> test_req(const std::string& method, 
                          const std::string& path, 
                          const std::string& body, 
                          const std::string& res_expect)
    {
        auto response =  co_await request(method, path, body);
        co_return co_await seastar::make_ready_future<bool>(check(
            response, std::move(method), std::move(path), std::move(body), std::move(res_expect)
        )); 
    }

    future<bool> test_PUT_GET(const std::string& key, const std::string& val)
    {
        co_await test_PUT(key, val, "OK");
        co_return co_await test_GET(key, val);
    }

    void init()
    {
        pid = execute_binary(k_ssdb_file_name);
        sleep(k_secs_required_to_start);
    }
    void finalize()
    {
        const auto killed = kill_binary(pid);
    }

private:

    bool check(const std::basic_string<char>& response,
                std::string&& method,
                std::string&& path, 
                std::string&& body, 
                std::string&& res_expect)
    {
        if (response != std::format("\"{}\"", res_expect))
        {
            failed_cases.emplace_back(
                std::format("FAIL: Method {} at path {} expected \"{}\" while the response was: {}.", 
                std::move(method), std::move(path), std::move(res_expect), std::move(response)
            ));
        }
        return failed_cases.size();
    }

    bool check(const std::basic_string<char>& response,
               const std::string& method,
               const std::string& path, 
               const std::string& body, 
               const std::string& res_expect)
    {
        if (response != std::format("\"{}\"", res_expect))
        {
            failed_cases.emplace_back(
                std::format("FAIL: Method {} at path {} expected \"{}\" while the response was: {}.", 
                std::move(method), std::move(path), std::move(res_expect), std::move(response)
            ));
        }
        return failed_cases.size();
    }

    pid_t pid;
    std::vector<std::string> failed_cases;
};

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    seastar::app_template app;

    return app.run(argc, argv, [&argc, &argv] () -> seastar::future<int> {
        bool test_result = RUN_ALL_TESTS();

        std::cout << " ###################################" << std::endl;
        std::cout << " ### Starting all DB test suites ###" << std::endl;
        std::cout << " ###################################" << std::endl;

        if(!test_result)
        {//Respond hello to GET on //
            DbRespTest db_suite{};
            test_result = co_await db_suite.test_GET("", server_hello_message);
        }
        {//Respond 404 to GET on //inexsting
            DbRespTest db_suite{};
            test_result = co_await db_suite.test_GET("inexisting", "404 not found");
        }
        if(!test_result)
        {//Respond OK to key/val body PUT on /
            DbRespTest db_suite{};
            test_result = co_await db_suite.test_req("PUT", "//k", "v", "OK");
        }
        if(!test_result)
        {//Respond OK to key/val body PUT on /, Respond with val, when GET key
            DbRespTest db_suite{};
            test_result = co_await db_suite.test_PUT_GET("key0", "val0");
        }
        if(!test_result)
        {//Respond OK to a different key/val body PUT on /, Respond with val, when GET key
            DbRespTest db_suite{};
            test_result = co_await db_suite.test_PUT_GET("key1", "val1");
        }
        if(!test_result)
        {// Can change the value for a key.
            DbRespTest db_suite{};
            test_result = co_await db_suite.test_PUT_GET("key1", "val1");
            test_result = co_await db_suite.test_PUT_GET("key1", "val2");
        }

        co_return co_await seastar::make_ready_future<int>(test_result);
    });
}