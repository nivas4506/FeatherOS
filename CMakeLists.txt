cmake_minimum_required(VERSION 3.20)

project(FeatherOS
    VERSION 0.2.0
    DESCRIPTION "FeatherOS - AI-Native Operating System Substrate"
    LANGUAGES CXX ASM_NASM
)

# The bare-metal kernel lives in kernel/ and is configured with its own
# CMakeLists.txt. It supports both the Microsoft Visual C++ (MSVC) and GNU
# toolchains. Select the toolchain via the CMake preset or -G/-A/-T flags.
add_subdirectory(kernel)
