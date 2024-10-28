#include <cstdlib>
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
#include <thread>

#include "ClientTester.hh"
#include "seastar/core/future.hh"

#include <sched.h>
#include <thread>
#include <iostream>

#include <iostream>
#include <string>
#include <array>
#include <cstdio>
#include <sstream>
#include <signal.h>
#include <unistd.h>

std::string get_process_info(const std::string& binaryName) 
{
    std::array<char, 128> buffer;
    std::string result;

    std::string command = "ps -aux | grep " + binaryName + " | grep -v grep";
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cout << " ### popen() failed!" << std::endl;
        return "";
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    pclose(pipe);
    std::cout << " ### Process info: " << result << std::endl;
    return result;
}

pid_t extract_pid(const std::string& processInfo) 
{
    std::istringstream stream(processInfo);
    std::string token;
    pid_t pid = -1;

    // The second column in the ps output is the PID
    stream >> token;  // Skip the first column (USER)
    stream >> pid;    // Get the second column (PID)

    return pid;
}

void set_cpu_affinity() 
{
    cpu_set_t set;
    CPU_ZERO(&set);
    
    for (int i = 0; i < std::thread::hardware_concurrency(); i++) {
        CPU_SET(i, &set);
    }

    if (sched_setaffinity(0, sizeof(set), &set) == -1) {
        perror("sched_setaffinity");
    }
}

extern const std::string server_hello_message;

const std::string k_ssdb_file_name = "seastear_simple_db_server";
const std::string k_ssdbct_file_name = "seastear_simple_db_client_tester";
const unsigned k_secs_required_to_start = 1;
const unsigned k_secs_required_to_terminate = 1;

// Function to check if a file exists and is executable
bool is_executable(const std::string& file) 
{
    struct stat buffer;
    return (stat(file.c_str(), &buffer) == 0) && (buffer.st_mode & S_IXUSR);
}

int execute_binary(const std::string& binaryPath, const std::vector<std::string>& args = {})
{
    std::string binary_with_exec_prefix = "./";
    binary_with_exec_prefix += binaryPath + " &";
    int ret = std::system(binary_with_exec_prefix.c_str());
    if (ret == -1) {
        std::cout << "Error executing the binary." << std::endl;
        return -1;
    }

    std::cout << " ### Executing the binary." << std::endl;
    sleep(k_secs_required_to_start);
    return ret;
}

bool kill_binary(pid_t pid) 
{
    if (kill(pid, SIGTERM) == -1) 
    {
        perror("kill");
        return false;
    }
    return true;
}

bool is_process_alive(const std::string& binary_name)
{
    const auto info = get_process_info(binary_name);
    if (info.empty()) 
    {
        std::cout << " ### Process not found!" << std::endl;
        return false;
    }
    std::cout << " ### Process is alive." << std::endl;
    return true;
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
   const auto result = execute_binary(k_ssdb_file_name, {"--smp",{"12"}});
   ASSERT_NE(result, -1);
   ASSERT_TRUE(is_process_alive(k_ssdb_file_name)) << "Server binary is not alive";
   const auto pid = extract_pid(get_process_info(k_ssdb_file_name));
   const auto killed = kill_binary(pid);
   ASSERT_EQ(killed, true);
}

TEST(SeastearSimpleDbTests, ServerStaysAliveBeforeTestExits) {
   const auto result = execute_binary(k_ssdb_file_name);
   ASSERT_NE(result, -1);
   sleep(5);
   ASSERT_TRUE(is_process_alive(k_ssdb_file_name)) << "Server binary is not alive";
   const auto pid = extract_pid(get_process_info(k_ssdb_file_name));
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
    // An indendpendent suite launches and kills the db binary with each test case.
    // A dependedt one is depentend on an externally launched db.
    DbRespTest(bool independent = true) : is_independent(independent)
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
    future<bool> test_DELETE(std::string&& key, std::string&& res_expect)
    {
        co_return co_await test_req("DELETE", std::string("//") + std::move(key), "",std::move(res_expect));
    }

    future<bool> test_DELETE(const std::string& key, const std::string& res_expect)
    {
        co_return co_await test_req("DELETE", std::string("//") + key, "", res_expect);
    }

    future<bool> test_req(const std::string& method, 
                          const std::string& path, 
                          const std::string& body, 
                          const std::string& res_expect)
    {
        auto response =  co_await request(method, path, body);
        co_return co_await seastar::make_ready_future<bool>(check(
            //TODO this will only copy the args anyway.
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
        if (is_independent)
            execute_binary(k_ssdb_file_name);

        if (!is_process_alive(k_ssdb_file_name))
        {
            throw("ERROR: Could not start the db in the test suite!");
        }
    }

    void finalize()
    {
        if (!is_independent)
            return;

        const auto pid = extract_pid(get_process_info(k_ssdb_file_name));
        const auto killed = kill_binary(pid);
        if (!kill_binary(pid))
        {
            throw("ERROR: Could not terminate the db in the test suite!");
        }
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

    std::vector<std::string> failed_cases;
    bool is_independent = false;
};

future<bool> run_independent_suites()
{
    bool test_result = RUN_ALL_TESTS();

    std::cout << " ###################################" << std::endl;
    std::cout << " ### Starting all DB test suites ###" << std::endl;
    std::cout << " ### Available cores: " << std::thread::hardware_concurrency() << std::endl;
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
    {// Can put same values under different keys.
        DbRespTest db_suite{};
        test_result = co_await db_suite.test_PUT_GET("key1", "val1");
        test_result = co_await db_suite.test_PUT_GET("key2", "val1");
    }
    {// Can DELETE a key
        DbRespTest db_suite{};
        test_result = co_await db_suite.test_PUT_GET("key1", "val1");
        test_result = co_await db_suite.test_DELETE("key1", "OK");
        test_result = co_await db_suite.test_GET("key1", "404 not found");
    }
    co_return co_await seastar::make_ready_future<int>(test_result);
}

int main(int argc, char** argv) 
{
    ::testing::InitGoogleTest(&argc, argv);

    seastar::app_template app;

    return app.run(argc, argv, [&argc, &argv] () -> seastar::future<int> {
        // TODO: Resolve issues when killing it and launching it from this process multiple times.
        //set_cpu_affinity();
        //bool test_result = co_await run_independent_suites();

        return seastar::do_with(DbRespTest(false), [&] (DbRespTest& db_suite) -> future<int> {
            co_return co_await seastar::yield().then(seastar::coroutine::lambda([&] () -> future<bool> {
                co_await seastar::coroutine::maybe_yield();
                int test_result = false;
                if(!test_result)
                {//Respond hello to GET on //
                    test_result = co_await db_suite.test_GET("", server_hello_message);
                }
                if(!test_result)
                {//Respond 404 to GET on //inexsting
                    test_result = co_await db_suite.test_GET("inexisting", "404 not found");
                }
                if(!test_result)
                {//Respond OK to key/val body PUT on /
                    test_result = co_await db_suite.test_req("PUT", "//k", "v", "OK");
                }
                if(!test_result)
                {//Respond OK to key/val body PUT on /, Respond with val, when GET key
                    test_result = co_await db_suite.test_PUT_GET("key0", "val0");
                }
                if(!test_result)
                {//Respond OK to a different key/val body PUT on /, Respond with val, when GET key
                    test_result = co_await db_suite.test_PUT_GET("key1", "val1");
                }
                if(!test_result)
                {// Can change the value for a key.
                    test_result = co_await db_suite.test_PUT_GET("key1", "val2");
                }
                if(!test_result)
                {// Can put same values under different keys.
                    test_result = co_await db_suite.test_PUT_GET("key3", "val1");
                    test_result = co_await db_suite.test_PUT_GET("key4", "val1");
                }
                if(!test_result)
                {// Can DELETE a key
                    test_result = co_await db_suite.test_PUT_GET("key5", "val1");
                    test_result = co_await db_suite.test_DELETE("key5", "OK");
                    test_result = co_await db_suite.test_GET("key5", "404 not found");
                }
                co_return co_await make_ready_future<int>(test_result);
            }));
        });
    });
}