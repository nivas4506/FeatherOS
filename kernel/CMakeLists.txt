cmake_minimum_required(VERSION 3.20)

# Force bare-metal target - no host system
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

project(FeatherOS_Kernel
    VERSION 0.2.0
    DESCRIPTION "FeatherOS native monolithic kernel (C++20 + x86_64)"
    LANGUAGES CXX ASM_NASM
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

# No standard libraries
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffreestanding -nostdlib")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib")

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

set(KERNEL_CXX_SRCS
    kernel/src/main.cpp
    kernel/src/panic.cpp
    kernel/src/syscalls.cpp
    kernel/src/shell.cpp
    kernel/src/gui.cpp
    kernel/src/window_manager.cpp
    kernel/src/portal.cpp
    kernel/src/elf.cpp
    lib/src/string.cpp
    lib/src/printf.cpp
    lib/src/io.cpp
    arch/x86_64/src/gdt.cpp
    arch/x86_64/src/idt.cpp
    arch/x86_64/src/interrupts.cpp
    arch/x86_64/src/interrupt_dispatch.cpp
    arch/x86_64/src/cpu.cpp
    drivers/src/vga.cpp
    drivers/src/framebuffer.cpp
    drivers/src/mouse.cpp
    drivers/src/keyboard.cpp
    drivers/src/serial.cpp
    drivers/src/pit.cpp
    mm/src/paging.cpp
    mm/src/frame_allocator.cpp
    mm/src/heap.cpp
    task/src/task.cpp
    fs/src/vfs.cpp
)

set(KERNEL_ASM_SRCS
    arch/x86_64/src/boot.asm
    arch/x86_64/src/isr.asm
    arch/x86_64/src/context_switch.asm
    arch/x86_64/src/syscall_entry.asm
)

# Compile C++ sources to object files
foreach(src ${KERNEL_CXX_SRCS})
    get_filename_component(obj_name ${src} NAME_WE)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${obj_name}.o
        COMMAND ${CMAKE_CXX_COMPILER} ${FREESTANDING_CXX_FLAGS}
            -I${CMAKE_CURRENT_SOURCE_DIR}/arch/x86_64/include
            -I${CMAKE_CURRENT_SOURCE_DIR}/drivers/include
            -I${CMAKE_CURRENT_SOURCE_DIR}/kernel/include
            -I${CMAKE_CURRENT_SOURCE_DIR}/fs/include
            -I${CMAKE_CURRENT_SOURCE_DIR}/mm/include
            -I${CMAKE_CURRENT_SOURCE_DIR}/lib/include
            -I${CMAKE_CURRENT_SOURCE_DIR}/task/include
            -c ${CMAKE_CURRENT_SOURCE_DIR}/${src}
            -o ${CMAKE_CURRENT_BINARY_DIR}/${obj_name}.o
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${src}
        COMMENT "Compiling ${src}"
    )
    list(APPEND KERNEL_OBJS ${CMAKE_CURRENT_BINARY_DIR}/${obj_name}.o)
endforeach()

# Compile ASM sources
foreach(src ${KERNEL_ASM_SRCS})
    get_filename_component(obj_name ${src} NAME_WE)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${obj_name}.o
        COMMAND ${CMAKE_ASM_NASM_COMPILER} -f elf64
            -I${CMAKE_CURRENT_SOURCE_DIR}/arch/x86_64/include
            ${CMAKE_CURRENT_SOURCE_DIR}/${src}
            -o ${CMAKE_CURRENT_BINARY_DIR}/${obj_name}.o
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${src}
        COMMENT "Assembling ${src}"
    )
    list(APPEND KERNEL_OBJS ${CMAKE_CURRENT_BINARY_DIR}/${obj_name}.o)
endforeach()

# Link kernel ELF using ld directly (bypass GCC driver's Windows defaults)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/kernel.elf
    COMMAND ${CMAKE_LINKER}
        -nostdlib
        -static
        --oformat=elf64-x86-64
        -T ${CMAKE_CURRENT_SOURCE_DIR}/arch/x86_64/src/linker.ld
        --build-id=none
        --no-dynamic-linker
        -o ${CMAKE_CURRENT_BINARY_DIR}/kernel.elf
        ${KERNEL_OBJS}
    DEPENDS ${KERNEL_OBJS}
    COMMENT "Linking kernel ELF"
)

add_custom_target(kernel_elf DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/kernel.elf)

add_custom_target(iso
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/iso/boot/grub
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/kernel.elf ${CMAKE_CURRENT_BINARY_DIR}/iso/boot/kernel.elf
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/boot/grub-multiboot.cfg ${CMAKE_CURRENT_BINARY_DIR}/iso/boot/grub/grub.cfg
    COMMAND grub-mkrescue -o ${CMAKE_CURRENT_BINARY_DIR}/featheros.iso ${CMAKE_CURRENT_BINARY_DIR}/iso
    DEPENDS kernel_elf
    COMMENT "Building bootable ISO for VirtualBox / QEMU"
)

add_custom_target(clean-iso
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_CURRENT_BINARY_DIR}/iso ${CMAKE_CURRENT_BINARY_DIR}/featheros.iso
    COMMENT "Cleaning ISO build artifacts"
)

# Clean target
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E rm -f ${KERNEL_OBJS} ${CMAKE_CURRENT_BINARY_DIR}/kernel.elf
    COMMENT "Cleaning all build artifacts"
)