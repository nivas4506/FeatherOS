// FeatherOS Kernel — Panic implementation
#include "panic.h"
#include "../../lib/include/printf.h"
#include "../../arch/x86_64/include/cpu.h"

namespace feather::kernel {

[[noreturn]] void panic(const char* msg) {
    using feather::arch::cpu::hlt;
    lib::kprintf("KERNEL PANIC: %s\n", msg);
    for (;;) hlt();
}

} // namespace feather::kernel
