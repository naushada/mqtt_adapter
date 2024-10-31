cmake_minimum_required(VERSION 3.16.3)
project(service)

#Adding Complier flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -g -Wall -Wextra")

#include_directories(inc)

#However, the file(GLOB...) allows for wildcard additions:
file(GLOB SOURCES "*.c")

add_executable(service ${SOURCES})

target_link_libraries(service pthread ssl crypto)

install(TARGETS service
	    CONFIGURATIONS Release
        RUNTIME DESTINATION /usr/local/bin
)