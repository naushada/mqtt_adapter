cmake_minimum_required(VERSION 3.16.3)
project(smtc)
find_package(GTest REQUIRED)

include_directories(../inc)
include_directories(inc)

#Adding Complier flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++2a -g -Wall -Wextra")
#However, the file(GLOB...) allows for wildcard additions:
file(GLOB SOURCES "src/*.cpp" "../src/services.cpp" 
                              "../src/services_http.cpp"
                              "../src/services_lwm2m.cpp"
                              "../src/command_line_argument.cpp"
                              )

enable_testing()

add_executable(smtc_test ${SOURCES})
target_link_libraries(smtc_test gtest_main)
target_link_libraries(smtc_test pthread)
target_link_libraries(smtc_test ssl crypto)
target_link_libraries(smtc_test gtest)

add_test(smtc_gtests smtc_test)
gtest_discover_tests(smtc_test)