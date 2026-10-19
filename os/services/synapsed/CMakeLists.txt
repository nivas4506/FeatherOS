cmake_minimum_required(VERSION 3.20)
project(synapsed LANGUAGES CXX)

add_library(synapsed STATIC src/synapsed.cpp)
target_include_directories(synapsed PUBLIC include)
target_compile_features(synapsed PUBLIC cxx_std_20)
target_link_libraries(synapsed PUBLIC feather-hal Threads::Threads)
