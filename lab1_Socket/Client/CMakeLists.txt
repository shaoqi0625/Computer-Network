cmake_minimum_required(VERSION 3.23)
project(Client)

set(CMAKE_CXX_STANDARD 14)

add_compile_options(-lpthread)

add_executable(Client main.cpp)

add_compile_options(-lwsock32)

target_link_libraries(Client ws2_32)