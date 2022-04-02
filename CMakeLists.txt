cmake_minimum_required(VERSION 3.10)

project(chatter)

add_executable(chatter-server chatter-server.cpp)
add_executable(chatter chatter.cpp)

set_target_properties(chatter-server chatter PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")
