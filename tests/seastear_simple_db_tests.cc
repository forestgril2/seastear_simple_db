#include <gtest/gtest.h>

#include <sys/stat.h>

//#include <algorithm>
//#include <seastar/core/app-template.hh>
//#include <seastar/core/file.hh>
//#include <seastar/core/reactor.hh>
//#include <seastar/core/seastar.hh>
//#include <seastar/core/semaphore.hh>
//#include <iostream>

//using namespace seastar;


// Function to check if a file exists and is executable
bool isExecutable(const std::string& file) {
    struct stat buffer;
    return (stat(file.c_str(), &buffer) == 0) && (buffer.st_mode & S_IXUSR);
}

TEST(SeastearSimpleDbTests, ServerBinaryExists)
{ 
    std::string file_name = "seastear_simple_db";
    EXPECT_TRUE(isExecutable(file_name)) << "The file \"" << file_name << "\" does not exist or is not executable.";
}

TEST(SeastearSimpleDbTests, ClientTesterBinaryExists)
{ 
    std::string file_name = "seastear_simple_db_client_tester";
    EXPECT_TRUE(isExecutable(file_name)) << "The file \"" << file_name << "\" does not exist or is not executable.";
}