cmake_minimum_required(VERSION 3.20)
project(featheros-demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use same kernel target as parent
if(NOT DEFINED FEATHER_TARGET_KERNEL)
    set(FEATHER_TARGET_KERNEL "PORTAL" CACHE STRING "Target kernel: PORTAL | LINUX | WINDOWS")
endif()

add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
    -fstack-protector-strong
    -D_FORTIFY_SOURCE=2
)

add_executable(featheros-demo main.cpp)

target_include_directories(featheros-demo PRIVATE
    ${CMAKE_SOURCE_DIR}/../core/feather-hal/include
    ${CMAKE_SOURCE_DIR}/../services/feather-ipc/include
    ${CMAKE_SOURCE_DIR}/../services/feather-watch/include
)

target_link_libraries(featheros-demo PRIVATE
    feather-hal
    feather-ipc
    feather-watch
    portal-stub
    Threads::Threads
)