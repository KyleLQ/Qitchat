cmake_minimum_required(VERSION 3.10)

project(Qitchat)

find_package(Crow CONFIG REQUIRED)

add_executable(Qitchat-server server.cpp)

target_link_libraries(Qitchat-server PRIVATE Crow::Crow)
