cmake_minimum_required(VERSION 4.2)
project(nanomon)

set(CMAKE_CXX_STANDARD 20)

find_package(nanogui CONFIG REQUIRED)

add_executable(nanomon main.cpp)

target_link_libraries(nanomon PRIVATE nanogui)