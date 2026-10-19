# Portal Kernel Backend for feather-hal
# Builds: process_portal, fs_portal, telemetry_portal, sandbox_portal

cmake_minimum_required(VERSION 3.20)

# Sources
set(PORTAL_HAL_SOURCES
    process_portal.cpp
    fs_portal.cpp
    telemetry_portal.cpp
    sandbox_portal.cpp
)

# Include portal kernel headers
target_include_directories(feather-hal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/portal
    ${CMAKE_SOURCE_DIR}/kernel/include  # for portal/portal.h
)

# Compile definitions for portal backend
target_compile_definitions(feather-hal PRIVATE
    FEATHER_HAL_PORTAL_BACKEND=1
)

# Link against portal syscall library (to be created)
# target_link_libraries(feather-hal PRIVATE portal_syscall)

message(STATUS "[FeatherOS] Building Portal HAL backend")