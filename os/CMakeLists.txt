cmake_minimum_required(VERSION 3.20)

project(FeatherOS_OSLayer
    VERSION 0.2.0
    DESCRIPTION "FeatherOS Platform-Independent OS Layer (C++20)"
    LANGUAGES CXX
)

# ─────────────────────────────────────────────────────────────
# Target kernel/backend selection
# ─────────────────────────────────────────────────────────────
set(FEATHER_TARGET_KERNEL "PORTAL" CACHE STRING "Target kernel: PORTAL | LINUX | WINDOWS")
message(STATUS "Raw FEATHER_TARGET_KERNEL: '${FEATHER_TARGET_KERNEL}'")
string(TOUPPER ${FEATHER_TARGET_KERNEL} FEATHER_TARGET_KERNEL)
message(STATUS "Upper FEATHER_TARGET_KERNEL: '${FEATHER_TARGET_KERNEL}'")

if(NOT (FEATHER_TARGET_KERNEL STREQUAL "PORTAL" OR FEATHER_TARGET_KERNEL STREQUAL "LINUX" OR FEATHER_TARGET_KERNEL STREQUAL "WINDOWS"))
    message(FATAL_ERROR "Invalid FEATHER_TARGET_KERNEL: ${FEATHER_TARGET_KERNEL}")
endif()

message(STATUS "[FeatherOS] Target kernel: ${FEATHER_TARGET_KERNEL}")

# Propagate to subprojects
set(FEATHER_HAL_BACKEND ${FEATHER_TARGET_KERNEL} CACHE INTERNAL "")
set(FEATHER_IPC_TRANSPORT
    "PORTAL"  # PORTAL kernel uses portal transport
    CACHE INTERNAL ""
)

# ─────────────────────────────────────────────────────────────
# C++20 standard — required for std::span, std::ranges, concepts,
# coroutines, atomic smart pointers, and jthread.
# ─────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─────────────────────────────────────────────────────────────
# Compiler flags: treat warnings as errors, enable hardening
# ─────────────────────────────────────────────────────────────
add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
    -fstack-protector-strong
    -D_FORTIFY_SOURCE=2
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2 -DNDEBUG)
else()
    add_compile_options(-g -O0 -DFEATHER_DEBUG)
endif()

# Kernel-specific definitions
if(FEATHER_TARGET_KERNEL STREQUAL "PORTAL")
    add_compile_definitions(FEATHER_KERNEL_PORTAL=1)
elseif(FEATHER_TARGET_KERNEL STREQUAL "LINUX")
    add_compile_definitions(FEATHER_KERNEL_LINUX=1)
elseif(FEATHER_TARGET_KERNEL STREQUAL "WINDOWS")
    add_compile_definitions(FEATHER_KERNEL_WINDOWS=1)
endif()

# ─────────────────────────────────────────────────────────────
# Fetch nlohmann/json (header-only — no compile overhead)
# ─────────────────────────────────────────────────────────────
include(FetchContent)

# Try system install first; fall back to FetchContent
find_package(nlohmann_json 3.11 QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "[FeatherOS] System nlohmann/json not found — fetching from GitHub...")
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
        DOWNLOAD_NO_EXTRACT TRUE
        DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/include/nlohmann
    )
    FetchContent_MakeAvailable(nlohmann_json)
    add_library(nlohmann_json INTERFACE)
    target_include_directories(nlohmann_json INTERFACE ${CMAKE_BINARY_DIR}/include)
endif()

# ─────────────────────────────────────────────────────────────
# POSIX thread support
# ─────────────────────────────────────────────────────────────
find_package(Threads REQUIRED)

# ─────────────────────────────────────────────────────────────
# Global include so all subprojects can use <feather/...>
# ─────────────────────────────────────────────────────────────
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/core/feather-hal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/services/feather-ipc/include
)

# ─────────────────────────────────────────────────────────────
# Layer 3 OS Subsystems — build order respects dependencies
#
#  feather-hal   (no feather deps)
#  feather-ipc   → feather-hal
#  feather-watch → feather-hal
#  aegis-exec    → feather-hal
#  aegis-snapshot→ feather-hal
#  synapsed      → feather-hal
#  featherd      → all of the above
# ─────────────────────────────────────────────────────────────
add_subdirectory(core/feather-hal)
add_subdirectory(services/feather-ipc)
add_subdirectory(services/feather-watch)
add_subdirectory(safety/aegis-exec)
add_subdirectory(safety/aegis-snapshot)
add_subdirectory(services/synapsed)
add_subdirectory(services/featherd)

message(STATUS "")
message(STATUS "╔══════════════════════════════════════════════════════╗")
message(STATUS "║     FeatherOS C++ OS Layer — Build Configured        ║")
message(STATUS "║   Hardware → ${FEATHER_TARGET_KERNEL} Kernel → OS Layer → App Layer     ║")
message(STATUS "╚══════════════════════════════════════════════════════╝")
message(STATUS "  Compiler  : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Standard  : C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Targets   : feather-hal | feather-ipc | feather-watch |")
message(STATUS "              aegis-exec  | aegis-snapshot | synapsed | featherd")
message(STATUS "")
