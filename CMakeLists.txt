cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(chatter)

add_executable(chatter-server chatter-server.cpp server.cpp room.cpp common.cpp)
add_executable(chatter chatter.cpp common.cpp)

set_target_properties(chatter-server chatter PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")
