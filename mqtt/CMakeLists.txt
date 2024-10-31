cmake_minimum_required(VERSION 3.16.3)
project(mqtt_adapter)

#Adding Complier flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -g -Wall -Wextra")

include_directories(inc)

#However, the file(GLOB...) allows for wildcard additions:
file(GLOB SOURCES "src/*.c")

add_executable(mqtt_adapter ${SOURCES})

target_link_libraries(mqtt_adapter pthread ssl crypto)

install(TARGETS mqtt_adapter
	    CONFIGURATIONS Release
        RUNTIME DESTINATION /usr/local/bin
)