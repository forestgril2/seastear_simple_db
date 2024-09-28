#include <gtest/gtest.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <iostream>

//#include <algorithm>
//#include <seastar/core/app-template.hh>
//#include <seastar/core/file.hh>
//#include <seastar/core/reactor.hh>
//#include <seastar/core/seastar.hh>
//#include <seastar/core/semaphore.hh>
//#include <iostream>

//using namespace seastar;

const std::string hostname = "";
const std::string k_ssdb_file_name = "seastear_simple_db";
const std::string k_ssdbct_file_name = "seastear_simple_db_client_tester";

// Function to check if a file exists and is executable
bool is_executable(const std::string& file) {
    struct stat buffer;
    return (stat(file.c_str(), &buffer) == 0) && (buffer.st_mode & S_IXUSR);
}

// Function to start a process and execute the binary
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
        // Send kill signal to the child process
        kill(pid, SIGKILL);
        int status;
        waitpid(pid, &status, 0);  // Wait for the process to terminate
        if (WIFSIGNALED(status)) {
            std::cout << "Process terminated by signal: " << WTERMSIG(status) << std::endl;
            return true;
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

TEST(SeastearSimpleDbTests, ServerStaysAliveBeforeBeingKilled) {
   const auto pid = execute_binary(k_ssdb_file_name);
   ASSERT_NE(pid, -1);
   sleep(5);
   ASSERT_TRUE(is_process_alive(pid)) << "Server binary is not alive";
   //const auto killed = kill_binary(pid);
   //ASSERT_EQ(killed, true);
}
