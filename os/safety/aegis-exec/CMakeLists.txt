cmake_minimum_required(VERSION 3.20)
project(aegis-exec LANGUAGES CXX)

add_library(aegis-exec STATIC src/safety.cpp)
target_include_directories(aegis-exec PUBLIC include)
target_compile_features(aegis-exec PUBLIC cxx_std_20)
target_link_libraries(aegis-exec PUBLIC feather-hal)
