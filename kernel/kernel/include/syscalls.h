#pragma once
// FeatherOS Kernel — Syscalls

#include <cstdint>

namespace feather::kernel {

void syscalls_init();
uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5);

} // namespace feather::kernel
