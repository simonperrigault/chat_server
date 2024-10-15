

cmake_minimum_required(VERSION 3.0)

project(socket)

add_executable(server server.c queue.c)
add_executable(client client.c)

