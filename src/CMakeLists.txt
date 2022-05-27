add_executable(chatter chatter.cpp server.cpp room.cpp)
set_target_properties(chatter PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}")
