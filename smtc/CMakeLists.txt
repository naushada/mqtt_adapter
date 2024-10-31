cmake_minimum_required(VERSION 3.16.3)
project(smtc)

#Adding Complier flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++2a -g -Wall -Wextra")

include_directories(inc)

#However, the file(GLOB...) allows for wildcard additions:
file(GLOB SOURCES "src/*.cpp")

add_executable(smtc ${SOURCES})

target_link_libraries(smtc pthread ssl crypto)

add_subdirectory(test)

install(TARGETS smtc
	    CONFIGURATIONS Release
        RUNTIME DESTINATION /usr/local/bin
)