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

#---------------------------------------------------------------------------
# Toolchain configuration: Microsoft Visual C++ (MSVC) or GNU (GCC/Clang)
#---------------------------------------------------------------------------
if(MSVC)
    message(STATUS "FeatherOS: Configuring for Microsoft Visual C++ (MSVC) toolchain")
    # MSVC (cl.exe) does not support GCC-style -ffreestanding / -nostdlib flags.
    # CRT minimization is handled via /NODEFAULTLIB at link time.
    # Note: MSVC link.exe cannot produce ELF binaries or consume GNU linker
    # scripts, so the kernel_elf / iso targets are GNU-only (see below).
    set(FREESTANDING_CXX_FLAGS
        /EHs-c-          # Disable C++ exceptions
        /GR-             # Disable RTTI
        /GS-             # Disable stack security checks (/GS)
        /O2              # Maximize speed
        /W4              # Warning level 4
        /Zc:__cplusplus  # Correct __cplusplus value for C++20 feature tests
        /std:c++20       # C++20 standard
        /nologo          # Suppress startup banner
    )
    # MSVC x64 ABI has no red zone by default; no -mcmodel/-fno-pie equivalents.
    set(CMAKE_ASM_NASM_OBJECT_FORMAT win64)
    set(INCLUDE_FLAGS
        /I${CMAKE_CURRENT_SOURCE_DIR}/arch/x86_64/include
        /I${CMAKE_CURRENT_SOURCE_DIR}/drivers/include
        /I${CMAKE_CURRENT_SOURCE_DIR}/kernel/include
        /I${CMAKE_CURRENT_SOURCE_DIR}/fs/include
        /I${CMAKE_CURRENT_SOURCE_DIR}/mm/include
        /I${CMAKE_CURRENT_SOURCE_DIR}/lib/include
        /I${CMAKE_CURRENT_SOURCE_DIR}/task/include
    )
else()
    message(STATUS "FeatherOS: Configuring for GNU toolchain (GCC/Clang)")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffreestanding -nostdlib")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib")
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
    set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)
    set(INCLUDE_FLAGS
        -I${CMAKE_CURRENT_SOURCE_DIR}/arch/x86_64/include
        -I${CMAKE_CURRENT_SOURCE_DIR}/drivers/include
        -I${CMAKE_CURRENT_SOURCE_DIR}/kernel/include
        -I${CMAKE_CURRENT_SOURCE_DIR}/fs/include
        -I${CMAKE_CURRENT_SOURCE_DIR}/mm/include
        -I${CMAKE_CURRENT_SOURCE_DIR}/lib/include
        -I${CMAKE_CURRENT_SOURCE_DIR}/task/include
    )
endif()

set(KERNEL_CXX_SRCS
    kernel/src/main.cpp
    kernel/src/panic.cpp
    kernel/src/syscalls.cpp
    kernel/src/shell.cpp
    kernel/src/gui.cpp
    kernel/src/window_manager.cpp
    kernel/src/portal.cpp
    kernel/src/elf.cpp
    kernel/src/userspace.cpp
    kernel/src/ebpf.cpp
    kernel/src/telemetry.cpp
    kernel/src/zerocopy.cpp
    kernel/src/scheduler_hints.cpp
    kernel/src/sandbox.cpp
    lib/src/string.cpp
    lib/src/printf.cpp
    lib/src/io.cpp
    arch/x86_64/src/gdt.cpp
    arch/x86_64/src/idt.cpp
    arch/x86_64/src/interrupts.cpp
    arch/x86_64/src/interrupt_dispatch.cpp
    arch/x86_64/src/cpu.cpp
    arch/x86_64/src/acpi.cpp
    arch/x86_64/src/lapic.cpp
    arch/x86_64/src/ioapic.cpp
    arch/x86_64/src/smp.cpp
    arch/x86_64/src/percpu.cpp
    drivers/src/vga.cpp
    drivers/src/framebuffer.cpp
    drivers/src/mouse.cpp
    drivers/src/keyboard.cpp
    drivers/src/serial.cpp
    drivers/src/pit.cpp
    drivers/src/pci.cpp
    drivers/src/nvme.cpp
    drivers/src/net.cpp
    drivers/src/netstack.cpp
    drivers/src/xhci.cpp
    drivers/src/hda.cpp
    drivers/src/acpi_power.cpp
    mm/src/paging.cpp
    mm/src/frame_allocator.cpp
    mm/src/heap.cpp
    mm/src/slab.cpp
    mm/src/vma.cpp
    mm/src/security.cpp
    task/src/task.cpp
    fs/src/vfs.cpp
    fs/src/ext2.cpp
    fs/src/fat32.cpp
)

set(KERNEL_ASM_SRCS
    arch/x86_64/src/boot.asm
    arch/x86_64/src/isr.asm
    arch/x86_64/src/context_switch.asm
    arch/x86_64/src/syscall_entry.asm
    kernel/src/sysret.asm
)

# MSVC needs helper assembly for instructions without intrinsics (hlt, cli,
# sti, cr3/cr4, msr, idt/gdt load, invlpg, segment reload). These are
# assembled with -f win64 for consumption by link.exe.
if(MSVC)
    list(APPEND KERNEL_ASM_SRCS arch/x86_64/src/msvc_helpers.asm)
endif()

# Compile C++ sources to object files
foreach(src ${KERNEL_CXX_SRCS})
    get_filename_component(obj_name ${src} NAME_WE)
    if(MSVC)
        # cl.exe uses /c (compile only) and /Fo<object> (output object file)
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${obj_name}.o
            COMMAND ${CMAKE_CXX_COMPILER} ${FREESTANDING_CXX_FLAGS}
                ${INCLUDE_FLAGS}
                /c ${CMAKE_CURRENT_SOURCE_DIR}/${src}
                /Fo${CMAKE_CURRENT_BINARY_DIR}/${obj_name}.o
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${src}
            COMMENT "Compiling ${src} (MSVC)"
        )
    else()
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${obj_name}.o
            COMMAND ${CMAKE_CXX_COMPILER} ${FREESTANDING_CXX_FLAGS}
                ${INCLUDE_FLAGS}
                -c ${CMAKE_CURRENT_SOURCE_DIR}/${src}
                -o ${CMAKE_CURRENT_BINARY_DIR}/${obj_name}.o
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${src}
            COMMENT "Compiling ${src} (GNU)"
        )
    endif()
    list(APPEND KERNEL_OBJS ${CMAKE_CURRENT_BINARY_DIR}/${obj_name}.o)
endforeach()

# Compile ASM sources
foreach(src ${KERNEL_ASM_SRCS})
    get_filename_component(obj_name ${src} NAME_WE)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${obj_name}.o
        COMMAND ${CMAKE_ASM_NASM_COMPILER} -f ${CMAKE_ASM_NASM_OBJECT_FORMAT}
            -I${CMAKE_CURRENT_SOURCE_DIR}/arch/x86_64/include
            ${CMAKE_CURRENT_SOURCE_DIR}/${src}
            -o ${CMAKE_CURRENT_BINARY_DIR}/${obj_name}.o
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${src}
        COMMENT "Assembling ${src}"
    )
    list(APPEND KERNEL_OBJS ${CMAKE_CURRENT_BINARY_DIR}/${obj_name}.o)
endforeach()

# Link kernel ELF using ld directly (GNU toolchain only - MSVC link.exe
# cannot consume GNU linker scripts or produce ELF output).
if(NOT MSVC)
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
else()
    message(STATUS "FeatherOS: MSVC toolchain - kernel_elf/iso targets disabled "
                   "(MSVC link.exe cannot produce ELF or use GNU linker scripts). "
                   "Object files are still compiled for syntax/codegen checking.")
    add_custom_target(kernel_objects DEPENDS ${KERNEL_OBJS})
    add_custom_target(kernel_elf
        COMMENT "kernel_elf target not available with MSVC (use GNU toolchain)"
    )
    add_custom_target(iso
        COMMENT "iso target not available with MSVC (use GNU toolchain)"
    )
endif()

# Clean target
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E rm -f ${KERNEL_OBJS} ${CMAKE_CURRENT_BINARY_DIR}/kernel.elf
    COMMENT "Cleaning all build artifacts"
)