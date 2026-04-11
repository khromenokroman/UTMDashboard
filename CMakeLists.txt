cmake_minimum_required(VERSION 4.2)
project(web)

set(CMAKE_CXX_STANDARD 20)

find_package(fmt REQUIRED)

add_executable(web main.cpp web_utm.cpp)

target_link_libraries(web PRIVATE curl fmt::fmt)
