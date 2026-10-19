# feather-ipc CMake Build — Multi-transport Support
# Supports: PORTAL (custom kernel), UNIX (Linux), WIN32 (Windows)

cmake_minimum_required(VERSION 3.20)
project(feather-ipc LANGUAGES CXX)

# ─────────────────────────────────────────────────────────────
# Transport selection
# ─────────────────────────────────────────────────────────────
set(FEATHER_IPC_TRANSPORT "PORTAL" CACHE STRING "IPC transport: PORTAL | UNIX | WIN32")
string(TOUPPER ${FEATHER_IPC_TRANSPORT} FEATHER_IPC_TRANSPORT)

if(NOT (FEATHER_IPC_TRANSPORT STREQUAL "PORTAL" OR FEATHER_IPC_TRANSPORT STREQUAL "UNIX" OR FEATHER_IPC_TRANSPORT STREQUAL "WIN32"))
    message(FATAL_ERROR "Invalid FEATHER_IPC_TRANSPORT: ${FEATHER_IPC_TRANSPORT}")
endif()

message(STATUS "[feather-ipc] Transport: ${FEATHER_IPC_TRANSPORT}")

# ─────────────────────────────────────────────────────────────
# Common settings
# ─────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
    -fstack-protector-strong
    -D_FORTIFY_SOURCE=2
)

# ─────────────────────────────────────────────────────────────
# Sources by transport
# ─────────────────────────────────────────────────────────────
if(FEATHER_IPC_TRANSPORT STREQUAL "PORTAL")
    set(IPC_SOURCES
        src/portal/ipc_portal.hpp  # header-only implementation
    )
    add_compile_definitions(FEATHER_IPC_PORTAL=1)

elseif(FEATHER_IPC_TRANSPORT STREQUAL "UNIX")
    set(IPC_SOURCES
        src/ipc.cpp
    )
    add_compile_definitions(FEATHER_IPC_UNIX=1)

elseif(FEATHER_IPC_TRANSPORT STREQUAL "WIN32")
    set(IPC_SOURCES
        src/ipc.cpp  # same file has #ifdef _WIN32
    )
    add_compile_definitions(FEATHER_IPC_WIN32=1)
endif()

# ─────────────────────────────────────────────────────────────
# Library target
# ─────────────────────────────────────────────────────────────
add_library(feather-ipc STATIC ${IPC_SOURCES})

target_include_directories(feather-ipc PUBLIC
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/kernel/include  # for portal/portal.h
)

target_compile_features(feather-ipc PUBLIC cxx_std_20)
target_link_libraries(feather-ipc PUBLIC feather-hal Threads::Threads)

# ─────────────────────────────────────────────────────────────
# Install
# ─────────────────────────────────────────────────────────────
install(TARGETS feather-ipc
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(DIRECTORY include/feather DESTINATION include)