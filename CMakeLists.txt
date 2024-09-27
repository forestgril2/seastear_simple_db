cmake_minimum_required(VERSION 3.28)
project(seaster_simple_db)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "-g")
set(CMAKE_CXX_STANDARD_REQUIRED ON)


project(main LANGUAGES CXX)

#set(CMAKE_CXX_MODULE_STD 1)


set(SEASTAR_PROJECT_DIR "/home/vboxuser/Projects/seastar")
set(SEASTAR_INCLUDE_DIR "${SEASTAR_PROJECT_DIR}/include")
set(SEASTAR_LIBRARY_DIR "${SEASTAR_PROJECT_DIR}/build/release")
set(SEASTAR_LIB "${SEASTAR_LIBRARY_DIR}/libseastar.a")

set (Seastar_SCHEDULING_GROUPS_COUNT
  "16"
  CACHE
  STRING
  "A positive number to set Seastar's reactor number of allowed different scheduling groups.")

find_package(GTest REQUIRED)

add_executable(main src/main.cc)

#find_library(FMT_LIB fmt PATHS /lib/x86_64-linux-gnu/)

target_compile_definitions (main
  PUBLIC SEASTAR_SCHEDULING_GROUPS_COUNT=${Seastar_SCHEDULING_GROUPS_COUNT})
target_include_directories(main PRIVATE ${SEASTAR_INCLUDE_DIR})
target_link_libraries(main ${SEASTAR_LIB} ${FMT_LIB})
target_compile_features(main PRIVATE cxx_std_23)

add_executable(tests tests/tests.cc)

target_include_directories(tests PRIVATE ${SEASTAR_INCLUDE_DIR})
target_link_libraries(tests GTest::GTest GTest::Main ${SEASTAR_LIB})
target_compile_features(tests PRIVATE cxx_std_23)

enable_testing()
add_test(NAME tests COMMAND tests)

