

cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD 11)

project(chat_server)

add_executable(server server.cpp)
add_executable(client client.cpp)

