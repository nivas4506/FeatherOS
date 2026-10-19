#include "syscalls.h"
#include "portal.h"
#include "elf.h"
#include "../../lib/include/printf.h"
#include "../../lib/include/string.h"
#include "../../drivers/include/keyboard.h"
#include "../../task/include/task.h"
#include "../../arch/x86_64/include/cpu.h"
#include "../../mm/include/paging.h"

namespace feather::kernel {

extern "C" uint64_t syscall_entry;
extern "C" uint64_t kernel_cr3_addr;

// Kernel stack for syscall handling (16 KiB)
alignas(16) static uint8_t kernel_stack[16384];
extern "C" {
uint64_t kernel_stack_top = (uint64_t)(kernel_stack + sizeof(kernel_stack));
uint64_t user_rsp_save = 0;
}

// MSR addresses
constexpr uint32_t MSR_IA32_STAR    = 0xC0000081;
constexpr uint32_t MSR_IA32_LSTAR   = 0xC0000082;
constexpr uint32_t MSR_IA32_FMASK   = 0xC0000084;
constexpr uint32_t MSR_IA32_KERNEL_GS_BASE = 0xC0000102;

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

enum Syscall : uint64_t {
    // Existing syscalls
    SYS_PUTCHAR    = 0,
    SYS_PUTS       = 1,
    SYS_GETCHAR    = 2,
    SYS_EXIT       = 3,
    SYS_YIELD      = 4,
    
    // PORTAL syscalls (0x100-0x10F)
    SYS_PORTAL_CREATE           = 0x100,
    SYS_PORTAL_CONNECT          = 0x101,
    SYS_PORTAL_SEND             = 0x102,
    SYS_PORTAL_RECV             = 0x103,
    SYS_PORTAL_CLOSE            = 0x104,
    SYS_PORTAL_SPAWN_RESTRICTED = 0x105,
    SYS_PORTAL_WAIT             = 0x106,
    SYS_PORTAL_KILL             = 0x107,
    SYS_PORTAL_GET_TELEMETRY    = 0x108,
    SYS_PORTAL_FS_OP            = 0x109,
    
    // Userspace syscalls (0x200-0x20F)
    SYS_USP_EXIT      = 0x200,
    SYS_USP_WRITE     = 0x201,
    SYS_USP_READ      = 0x202,
    SYS_USP_SPAWN     = 0x203,
    SYS_USP_MMAP      = 0x204,
    SYS_USP_BRK       = 0x205,
};

void syscalls_init() {
    portal::portal_init();
    elf::elf_init();

    // ─── Set up SYSCALL/SYSRET MSRs ─────────────────────────────
    // IA32_STAR:
    //   bits 47:32 = kernel CS selector (0x08)
    //   bits 63:48 = user CS selector base (0x18)
    // On SYSCALL: CS = 0x08, SS = 0x10
    // On SYSRET:  CS = 0x18 | 3 = 0x1B, SS = 0x20 | 3 = 0x23
    uint64_t star = ((uint64_t)0x18 << 48) | ((uint64_t)0x08 << 32);
    wrmsr(MSR_IA32_STAR, star);

    // IA32_LSTAR = entry point for SYSCALL
    wrmsr(MSR_IA32_LSTAR, (uint64_t)syscall_entry);

    // IA32_FMASK = flags to clear on syscall (clear IF, etc.)
    // 0x200 = IF bit
    wrmsr(MSR_IA32_FMASK, 0x200);

    // ─── Set up page table isolation ────────────────────────────
    // Tell the asm entry point where the kernel CR3 lives
    kernel_cr3_addr = mm::paging_cr3();

    lib::kputs("[syscalls] SYSCALL/SYSRET handler installed\n");
    lib::kputs("[syscalls] Page table isolation ready\n");
}

extern "C" uint64_t feather_kernel_syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    return syscall_handler(num, a1, a2, a3, a4, a5);
}

uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    switch (static_cast<Syscall>(num)) {
        // Existing syscalls
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
            return 0; // unreachable
        case SYS_YIELD:
            task::task_yield();
            return 0;
            
        // PORTAL syscalls
        case SYS_PORTAL_CREATE:
            return portal::portal_sys_create(
                reinterpret_cast<const char*>(a1),
                static_cast<portal::portal_caps_t>(a2),
                reinterpret_cast<portal::portal_handle_t*>(a3)
            );
        case SYS_PORTAL_CONNECT:
            return portal::portal_sys_connect(
                reinterpret_cast<const char*>(a1),
                static_cast<portal::portal_caps_t>(a2),
                reinterpret_cast<portal::portal_handle_t*>(a3)
            );
        case SYS_PORTAL_SEND:
            return portal::portal_sys_send(
                static_cast<portal::portal_handle_t>(a1),
                reinterpret_cast<const portal::portal_message_t*>(a2),
                reinterpret_cast<const portal::portal_handle_t*>(a3),
                static_cast<size_t>(a4)
            );
        case SYS_PORTAL_RECV:
            return portal::portal_sys_recv(
                static_cast<portal::portal_handle_t>(a1),
                reinterpret_cast<portal::portal_message_t*>(a2),
                reinterpret_cast<portal::portal_handle_t*>(a3),
                static_cast<size_t>(a4),
                reinterpret_cast<size_t*>(a5)
            );
        case SYS_PORTAL_CLOSE:
            return portal::portal_sys_close(static_cast<portal::portal_handle_t>(a1));
        case SYS_PORTAL_SPAWN_RESTRICTED:
            return portal::portal_sys_spawn_restricted(
                reinterpret_cast<const char*>(a1),
                reinterpret_cast<const char*>(a2),
                static_cast<portal::portal_caps_t>(a3),
                reinterpret_cast<uint32_t*>(a4),
                reinterpret_cast<portal::portal_handle_t*>(a5)
            );
        case SYS_PORTAL_WAIT:
            return portal::portal_sys_wait(
                static_cast<uint32_t>(a1),
                reinterpret_cast<int*>(a2)
            );
        case SYS_PORTAL_KILL:
            return portal::portal_sys_kill(
                static_cast<uint32_t>(a1),
                static_cast<int>(a2)
            );
        case SYS_PORTAL_GET_TELEMETRY:
            return portal::portal_sys_get_telemetry(
                reinterpret_cast<portal::portal_telemetry_t*>(a1)
            );
        case SYS_PORTAL_FS_OP:
            return portal::portal_sys_fs_op(
                reinterpret_cast<portal::portal_fs_request_t*>(a1)
            );
            
        // Userspace syscalls
        case SYS_USP_EXIT:
            task::task_exit(static_cast<int>(a1));
            return 0;
        case SYS_USP_WRITE:
            lib::kputs(reinterpret_cast<const char*>(a1));
            return static_cast<uint64_t>(lib::strlen(reinterpret_cast<const char*>(a1)));
        case SYS_USP_READ:
            return static_cast<uint64_t>(static_cast<unsigned char>(drivers::keyboard_getc()));
        case SYS_USP_SPAWN:
            return elf::userspace_task_create(
                reinterpret_cast<const void*>(a1),
                static_cast<size_t>(a2),
                reinterpret_cast<const char*>(a3)
            );
        case SYS_USP_MMAP:
            return static_cast<uint64_t>(-1);
        case SYS_USP_BRK:
            return static_cast<uint64_t>(-1);
            
        default:
            lib::kprintf("unknown syscall %u\n", static_cast<unsigned>(num));
            return static_cast<uint64_t>(-1);
    }
}

} // namespace feather::kernel