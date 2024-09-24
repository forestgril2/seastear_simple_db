cmake_minimum_required(VERSION 3.22)
project(seaster_simple_db)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "-g")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(Seastar REQUIRED)
find_package(GTest REQUIRED)

add_executable(main src/main.cc)

target_link_libraries(main Seastar::seastar)

add_executable(test tests/tests.cc)

target_link_libraries(test GTest::GTest GTest::Main Seastar::seastar)

enable_testing()
add_test(NAME test COMMAND test)

