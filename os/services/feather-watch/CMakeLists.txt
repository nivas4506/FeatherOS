# feather-watch CMake Build — Multi-platform Support
# Supports: PORTAL (custom kernel), LINUX (inotify)

cmake_minimum_required(VERSION 3.20)
project(feather-watch LANGUAGES CXX)

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
    set(WATCH_SOURCES
        src/portal/watch_portal.cpp
    )
    add_compile_definitions(FEATHER_WATCH_PORTAL=1)

elseif(FEATHER_TARGET_KERNEL STREQUAL "LINUX")
    set(WATCH_SOURCES
        src/watch.cpp
    )
    add_compile_definitions(FEATHER_WATCH_LINUX=1)

elseif(FEATHER_TARGET_KERNEL STREQUAL "WINDOWS")
    # TODO: Windows implementation using ReadDirectoryChangesW
    set(WATCH_SOURCES
        src/windows/watch_windows.cpp
    )
    add_compile_definitions(FEATHER_WATCH_WINDOWS=1)
endif()

# ─────────────────────────────────────────────────────────────
# Library target
# ─────────────────────────────────────────────────────────────
add_library(feather-watch STATIC ${WATCH_SOURCES})

target_include_directories(feather-watch PUBLIC
    include
)

target_compile_features(feather-watch PUBLIC cxx_std_20)
target_link_libraries(feather-watch PUBLIC feather-hal Threads::Threads)

# ─────────────────────────────────────────────────────────────
# Install
# ─────────────────────────────────────────────────────────────
install(TARGETS feather-watch
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(DIRECTORY include/feather DESTINATION include)