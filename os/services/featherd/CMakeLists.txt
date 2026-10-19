# featherd CMake Build — Multi-platform Supervisor Daemon
# Supports: PORTAL (custom kernel), LINUX (Unix sockets), WIN32 (Named pipes)

cmake_minimum_required(VERSION 3.20)
project(featherd LANGUAGES CXX)

# ─────────────────────────────────────────────────────────────
# Platform selection (inherited from parent)
# ─────────────────────────────────────────────────────────────
if(NOT DEFINED FEATHER_TARGET_KERNEL)
    set(FEATHER_TARGET_KERNEL "LINUX" CACHE STRING "Target kernel: PORTAL | LINUX | WINDOWS")
endif()

string(TOUPPER ${FEATHER_TARGET_KERNEL} FEATHER_TARGET_KERNEL)

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
# Sources by platform
# ─────────────────────────────────────────────────────────────
if(FEATHER_TARGET_KERNEL STREQUAL "PORTAL")
    set(DAEMON_SOURCES
        src/main.cpp
        src/portal/daemon_portal.cpp
    )
    add_compile_definitions(FEATHERD_PORTAL=1)

elseif(FEATHER_TARGET_KERNEL STREQUAL "LINUX")
    set(DAEMON_SOURCES
        src/main.cpp
        src/daemon.cpp
    )
    add_compile_definitions(FEATHERD_LINUX=1)

elseif(FEATHER_TARGET_KERNEL STREQUAL "WINDOWS")
    set(DAEMON_SOURCES
        src/main.cpp
        src/windows/daemon_windows.cpp
    )
    add_compile_definitions(FEATHERD_WINDOWS=1)
endif()

# ─────────────────────────────────────────────────────────────
# Executable target
# ─────────────────────────────────────────────────────────────
add_executable(featherd ${DAEMON_SOURCES})

target_include_directories(featherd PRIVATE
    include
)

target_compile_features(featherd PRIVATE cxx_std_20)
target_link_libraries(featherd PRIVATE
    feather-hal
    feather-ipc
    aegis-exec
    aegis-snapshot
    synapsed
    feather-watch
    nlohmann_json
    Threads::Threads
)

# ─────────────────────────────────────────────────────────────
# Install
# ─────────────────────────────────────────────────────────────
install(TARGETS featherd
    RUNTIME DESTINATION bin
)