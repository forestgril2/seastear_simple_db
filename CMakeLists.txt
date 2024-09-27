cmake_minimum_required(VERSION 3.28)
project(seaster_simple_db)

set(MODE "release")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "-g")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(main LANGUAGES CXX)


set(SEASTAR_PROJECT_DIR "/home/vboxuser/Projects/seastar")

set(SEASTAR_INCLUDE_DIR "${SEASTAR_PROJECT_DIR}/include")
set(SEASTAR_LIBRARY_DIR "${SEASTAR_PROJECT_DIR}/build/${MODE}")
set(SEASTAR_LIB "${SEASTAR_LIBRARY_DIR}/libseastar.a")
set(SEASTAR_PC "${SEASTAR_LIBRARY_DIR}/seastar.pc")

find_package(PkgConfig REQUIRED)
pkg_check_modules(SEASTAR REQUIRED "${SEASTAR_PC}")

execute_process(COMMAND pkg-config --libs --static ${SEASTAR_PC}
    OUTPUT_VARIABLE SEASTAR_LIBS_PRIVATE
    OUTPUT_STRIP_TRAILING_WHITESPACE)

String(REPLACE " " ";" SEASTAR_LIBS_PRIVATE_LIST "${SEASTAR_LIBS_PRIVATE}")

message("Seastar LDFLAGS: ${SEASTAR_LDFLAGS}")
message("Seastar Libs.private: ${SEASTAR_LIBS_PRIVATE_LIST}")

#set (Seastar_SCHEDULING_GROUPS_COUNT
#  "16"
#  CACHE
#  STRING
#  "A positive number to set Seastar's reactor number of allowed different scheduling groups.")

add_executable(main src/main.cc)
#find_library(FMT_LIB fmt PATHS /lib/x86_64-linux-gnu/)
#target_compile_definitions (main
#  PUBLIC SEASTAR_SCHEDULING_GROUPS_COUNT=${Seastar_SCHEDULING_GROUPS_COUNT})
target_include_directories(main PRIVATE ${SEASTAR_INCLUDE_DIR})
target_compile_options(main PRIVATE ${SEASTAR_CFLAGS})
target_link_libraries(main ${SEASTAR_LIB} ${SEASTAR_LDFLAGS} ${SEASTAR_LIBS_PRIVATE_LIST})
#target_compile_features(main PRIVATE cxx_std_23)

add_executable(tests tests/tests.cc)

find_package(GTest REQUIRED)
target_include_directories(tests PRIVATE ${SEASTAR_INCLUDE_DIR})
target_compile_options(tests PRIVATE ${SEASTAR_CFLAGS})
target_link_libraries(tests GTest::GTest GTest::Main ${SEASTAR_LIB} ${SEASTAR_LDFLAGS} ${SEASTAR_LIBS_PRIVATE_LIST})
#target_compile_features(tests PRIVATE cxx_std_23)

enable_testing()
add_test(NAME tests COMMAND tests)

#add_custom_target(build_seastar
#    COMMAND cd ${SEASTAR_PROJECT_DIR} && ./configure.py --mode=${MODE} --disable-dpdk --disable-hwloc --cflags=${CMAKE_CXX_FLAGS} --compiler=${CMAKE_CXX_COMPILER}
#    COMMAND ninja -C ${SEASTAR_DIR}/build/${MODE} libseastar.a
#)
#
#add_dependencies(main build_seastar)
#add_dependencies(tests build_seastar)