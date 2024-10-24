

cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD 11)

project(chat_server)

add_executable(server server.cpp)
add_executable(client client.cpp)

target_compile_options(server PRIVATE -Wall -Wextra -pedantic)
target_compile_options(client PRIVATE -Wall -Wextra -pedantic)