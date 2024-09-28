cmake_minimum_required(VERSION 3.28)
project(seaster_simple_db)

set(MODE "release")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "-g")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

add_executable(seastear_simple_db src/seastear_simple_db.cc)
target_include_directories(seastear_simple_db PRIVATE ${SEASTAR_INCLUDE_DIR})
target_compile_options(seastear_simple_db PRIVATE ${SEASTAR_CFLAGS})
target_link_libraries(seastear_simple_db ${SEASTAR_LIB} ${SEASTAR_LDFLAGS} ${SEASTAR_LIBS_PRIVATE_LIST})

add_executable(seastear_simple_db_client_tester tests/seastear_simple_db_client_tester.cc)
target_include_directories(seastear_simple_db_client_tester PRIVATE ${SEASTAR_INCLUDE_DIR})
target_compile_options(seastear_simple_db_client_tester PRIVATE ${SEASTAR_CFLAGS})
target_link_libraries(seastear_simple_db_client_tester ${SEASTAR_LIB} ${SEASTAR_LDFLAGS} ${SEASTAR_LIBS_PRIVATE_LIST})

add_executable(seastear_simple_db_tests tests/seastear_simple_db_tests.cc)
find_package(GTest REQUIRED)
target_include_directories(seastear_simple_db_tests PRIVATE ${SEASTAR_INCLUDE_DIR})
target_compile_options(seastear_simple_db_tests PRIVATE ${SEASTAR_CFLAGS})
target_link_libraries(seastear_simple_db_tests GTest::GTest GTest::Main ${SEASTAR_LIB} ${SEASTAR_LDFLAGS} ${SEASTAR_LIBS_PRIVATE_LIST})

enable_testing()
add_test(NAME seastear_simple_db_tests COMMAND seastear_simple_db_tests)

#add_custom_target(build_seastar
#    COMMAND cd ${SEASTAR_PROJECT_DIR} && ./configure.py --mode=${MODE} --disable-dpdk --disable-hwloc --cflags=${CMAKE_CXX_FLAGS} --compiler=${CMAKE_CXX_COMPILER}
#    COMMAND ninja -C ${SEASTAR_DIR}/build/${MODE} libseastar.a
#)
#
#add_dependencies(seastear_simple_db build_seastar)
#add_dependencies(seastear_simple_db_tests build_seastar)