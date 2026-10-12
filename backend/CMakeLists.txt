cmake_minimum_required(VERSION 3.10)

project(Qitchat)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Crow CONFIG REQUIRED)
find_package(SQLiteCpp CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)

add_executable(Qitchat-server 
    server.cpp 
    db.cpp
    routes/auth.cpp
    routes/common.cpp
    routes/messages.cpp
)

target_link_libraries(Qitchat-server 
    PRIVATE 
        Crow::Crow 
        SQLiteCpp
        unofficial-sodium::sodium
)
