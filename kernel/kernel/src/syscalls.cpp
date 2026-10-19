#include "syscalls.h"
#include "../../lib/include/printf.h"
#include "../../drivers/include/keyboard.h"
#include "../../task/include/task.h"
#include "../../arch/x86_64/include/cpu.h"

namespace feather::kernel {

enum Syscall : uint64_t {
    SYS_PUTCHAR = 0,
    SYS_PUTS    = 1,
    SYS_GETCHAR = 2,
    SYS_EXIT    = 3,
    SYS_YIELD   = 4,
};

void syscalls_init() {}

uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    switch (static_cast<Syscall>(num)) {
        case SYS_PUTCHAR:
            lib::kputc(static_cast<char>(a1));
            return 0;
        case SYS_PUTS:
            lib::kputs(reinterpret_cast<const char*>(a1));
            return 0;
        case SYS_GETCHAR:
            return static_cast<uint64_t>(static_cast<unsigned char>(drivers::keyboard_getc()));
        case SYS_EXIT:
            task::task_exit(static_cast<int>(a1));
        case SYS_YIELD:
            task::task_yield();
            return 0;
        default:
            lib::kprintf("unknown syscall %u\n", static_cast<unsigned>(num));
            return static_cast<uint64_t>(-1);
    }
}

} // namespace feather::kernel
