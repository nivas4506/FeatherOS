# feather-hal CMake Build — Multi-backend Support
# Supports: PORTAL (custom kernel), LINUX (existing), WINDOWS

cmake_minimum_required(VERSION 3.20)
project(feather-hal LANGUAGES CXX)

# ─────────────────────────────────────────────────────────────
# Backend selection
# ─────────────────────────────────────────────────────────────
set(FEATHER_HAL_BACKEND "PORTAL" CACHE STRING "HAL backend: PORTAL | LINUX | WINDOWS")
string(TOUPPER ${FEATHER_HAL_BACKEND} FEATHER_HAL_BACKEND)

if(NOT (FEATHER_HAL_BACKEND STREQUAL "PORTAL" OR FEATHER_HAL_BACKEND STREQUAL "LINUX" OR FEATHER_HAL_BACKEND STREQUAL "WINDOWS"))
    message(FATAL_ERROR "Invalid FEATHER_HAL_BACKEND: ${FEATHER_HAL_BACKEND}")
endif()

message(STATUS "[feather-hal] Backend: ${FEATHER_HAL_BACKEND}")

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

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2 -DNDEBUG)
else()
    add_compile_options(-g -O0 -DFEATHER_DEBUG)
endif()

# ─────────────────────────────────────────────────────────────
# Sources by backend
# ─────────────────────────────────────────────────────────────
set(COMMON_SOURCES
    src/hal.cpp  # trivial factory dispatch
)

if(FEATHER_HAL_BACKEND STREQUAL "PORTAL")
    set(BACKEND_SOURCES
        src/portal/process_portal.cpp
        src/portal/fs_portal.cpp
        src/portal/telemetry_portal.cpp
        src/portal/sandbox_portal.cpp
    )
    add_compile_definitions(FEATHER_HAL_PORTAL_BACKEND=1)
    # Portal kernel syscall library
    # find_library(PORTAL_SYSCALL portal_syscall)
    # target_link_libraries(feather-hal PRIVATE ${PORTAL_SYSCALL})

elseif(FEATHER_HAL_BACKEND STREQUAL "LINUX")
    set(BACKEND_SOURCES
        src/linux/process_linux.cpp
        src/linux/fs_linux.cpp
        src/linux/telemetry_linux.cpp
        src/linux/sandbox_linux.cpp
    )
    add_compile_definitions(FEATHER_HAL_LINUX_BACKEND=1)

elseif(FEATHER_HAL_BACKEND STREQUAL "WINDOWS")
    set(BACKEND_SOURCES
        src/windows/process_windows.cpp
        src/windows/fs_windows.cpp
        src/windows/telemetry_windows.cpp
        src/windows/sandbox_windows.cpp
    )
    add_compile_definitions(FEATHER_HAL_WINDOWS_BACKEND=1)
endif()

# ─────────────────────────────────────────────────────────────
# Library target
# ─────────────────────────────────────────────────────────────
add_library(feather-hal STATIC
    ${COMMON_SOURCES}
    ${BACKEND_SOURCES}
)

target_include_directories(feather-hal PUBLIC
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/src  # for backend-specific includes
    ${CMAKE_SOURCE_DIR}/../kernel/include  # for portal/portal.h
)

target_compile_features(feather-hal PUBLIC cxx_std_20)
target_link_libraries(feather-hal PUBLIC Threads::Threads)

# ─────────────────────────────────────────────────────────────
# Install
# ─────────────────────────────────────────────────────────────
install(TARGETS feather-hal
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(DIRECTORY include/feather DESTINATION include)