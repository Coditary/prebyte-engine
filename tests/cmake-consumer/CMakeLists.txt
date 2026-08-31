cmake_minimum_required(VERSION 3.31)
project(prebyte_engine_install_smoke LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(prebyte-engine CONFIG REQUIRED)

add_executable(prebyte_engine_cpp_smoke smoke.cpp)
target_link_libraries(prebyte_engine_cpp_smoke PRIVATE prebyte::engine_static)

add_executable(prebyte_engine_c_smoke smoke_c.c)
target_link_libraries(prebyte_engine_c_smoke PRIVATE prebyte::engine_c)
