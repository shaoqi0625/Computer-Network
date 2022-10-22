cmake_minimum_required(VERSION 3.23)
project(Server)

set(CMAKE_CXX_STANDARD 14)

add_compile_options(-lpthread)

add_executable(Server main.cpp)

add_compile_options(-lwsock32)

target_link_libraries(Server ws2_32)