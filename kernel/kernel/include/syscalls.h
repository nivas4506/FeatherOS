#pragma once
// FeatherOS Kernel — Syscalls


#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>
#include "elf.h"

namespace feather::kernel {

void syscalls_init();
uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5);

// Userspace entry point (called from syscall_entry.asm)
extern "C" uint64_t feather_kernel_syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5);

} // namespace feather::kernel
