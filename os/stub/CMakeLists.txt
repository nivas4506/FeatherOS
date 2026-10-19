cmake_minimum_required(VERSION 3.20)
project(portal-stub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(portal-stub STATIC portal_stub.cpp)

target_include_directories(portal-stub PUBLIC
    ${CMAKE_SOURCE_DIR}/../kernel/include
)

target_compile_features(portal-stub PUBLIC cxx_std_20)