cmake_minimum_required(VERSION 3.20)
project(FeatherOS_Kernel
    VERSION 0.2.0
    DESCRIPTION "FeatherOS native monolithic kernel (C++20 + x86_64)"
    LANGUAGES CXX ASM_NASM
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)
set(CMAKE_ASM_NASM_COMPILE_OBJECT
    "<CMAKE_ASM_NASM_COMPILER> <INCLUDES> <FLAGS> -f elf64 -o <OBJECT> <SOURCE>")

set(FREESTANDING_CXX_FLAGS
    -ffreestanding
    -fno-exceptions
    -fno-rtti
    -fno-stack-protector
    -fno-stack-check
    -mno-red-zone
    -mcmodel=small
    -fno-PIC
    -fno-pie
    -m64
    -mno-mmx
    -mno-sse
    -mno-sse2
    -Wall
    -Wextra
    -Wpedantic
)

set(KERNEL_SRCS
    arch/x86_64/src/boot.asm
    arch/x86_64/src/isr.asm
    arch/x86_64/src/context_switch.asm
    kernel/src/main.cpp
    kernel/src/panic.cpp
    kernel/src/syscalls.cpp
    kernel/src/shell.cpp
    lib/src/string.cpp
    lib/src/printf.cpp
    lib/src/io.cpp
    arch/x86_64/src/gdt.cpp
    arch/x86_64/src/idt.cpp
    arch/x86_64/src/interrupts.cpp
    arch/x86_64/src/interrupt_dispatch.cpp
    arch/x86_64/src/cpu.cpp
    drivers/src/vga.cpp
    drivers/src/keyboard.cpp
    drivers/src/serial.cpp
    drivers/src/pit.cpp
    mm/src/paging.cpp
    mm/src/frame_allocator.cpp
    mm/src/heap.cpp
    task/src/task.cpp
    fs/src/vfs.cpp
)

add_executable(kernel.elf ${KERNEL_SRCS})

target_compile_options(kernel.elf PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${FREESTANDING_CXX_FLAGS}>
)

target_include_directories(kernel.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/arch/x86_64/include
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/include
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/fs/include
    ${CMAKE_CURRENT_SOURCE_DIR}/mm/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/include
    ${CMAKE_CURRENT_SOURCE_DIR}/task/include
)

target_link_options(kernel.elf PRIVATE
    -nostdlib
    -static
    -no-pie
    -Wl,-n
    -Wl,-z,max-page-size=0x1000
    -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/arch/x86_64/src/linker.ld
    -Wl,--build-id=none
)

add_custom_target(iso
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/iso/boot/grub
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:kernel.elf> ${CMAKE_CURRENT_BINARY_DIR}/iso/boot/kernel.elf
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/boot/grub-multiboot.cfg ${CMAKE_CURRENT_BINARY_DIR}/iso/boot/grub/grub.cfg
    COMMAND grub-mkrescue -o ${CMAKE_CURRENT_BINARY_DIR}/featheros.iso ${CMAKE_CURRENT_BINARY_DIR}/iso
    DEPENDS kernel.elf
    COMMENT "Building bootable ISO for VirtualBox / QEMU"
)

add_custom_target(clean-iso
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_CURRENT_BINARY_DIR}/iso ${CMAKE_CURRENT_BINARY_DIR}/featheros.iso
    COMMENT "Cleaning ISO build artifacts"
)
