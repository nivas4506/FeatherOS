// FeatherOS Kernel — Syscalls
// Implements the syscall dispatch layer that connects userspace to kernel
// services (OS layer, hardware layer via drivers).
//
// Four-layer connection:
//   Layer 4 (Apps) -> Layer 3 (OS Services) via SYSCALL/SYSRET
//   Layer 3 (OS Services) -> Layer 2 (Kernel Core) via subsystem APIs
//   Layer 2 (Kernel Core) -> Layer 1 (Hardware) via driver HAL

#include "syscalls.h"
#include "portal.h"
#include "elf.h"
#include "userspace.h"
#include "ebpf.h"
#include "zerocopy.h"
#include "scheduler_hints.h"
#include "sandbox.h"
#include "../../lib/include/printf.h"
#include "../../lib/include/string.h"
#include "../../drivers/include/keyboard.h"
#include "../../drivers/include/netstack.h"
#include "../../task/include/task.h"
#include "../../arch/x86_64/include/cpu.h"
#include "../../mm/include/paging.h"
#include "../../mm/include/heap.h"
#include "../../fs/include/vfs.h"

#ifdef _MSC_VER
extern "C" uint64_t __rdmsr(uint32_t msr);
extern "C" void __wrmsr(uint32_t msr, uint64_t val);
#endif

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
#ifdef _MSC_VER
    return __rdmsr(msr);
#else
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
#endif
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
#ifdef _MSC_VER
    __wrmsr(msr, val);
#else
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
#endif
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
    SYS_USP_GETPID    = 0x206,
    SYS_USP_WAITPID   = 0x207,
    SYS_USP_KILL      = 0x208,
    SYS_USP_OPEN      = 0x209,
    SYS_USP_CLOSE     = 0x20A,
    SYS_USP_READ_FD   = 0x20B,
    SYS_USP_WRITE_FD  = 0x20C,

    // Network syscalls (0x300-0x30F)
    SYS_NET_SOCKET    = 0x300,
    SYS_NET_BIND      = 0x301,
    SYS_NET_LISTEN    = 0x302,
    SYS_NET_ACCEPT    = 0x303,
    SYS_NET_CONNECT   = 0x304,
    SYS_NET_SEND      = 0x305,
    SYS_NET_RECV      = 0x306,
    SYS_NET_CLOSE     = 0x307,

    // Phase 7: AI-native OS syscalls (0x400-0x40F)
    SYS_EBPF_LOAD     = 0x400,
    SYS_EBPF_RUN      = 0x401,
    SYS_ZC_CREATE     = 0x410,
    SYS_ZC_SEND       = 0x411,
    SYS_ZC_RECV       = 0x412,
    SYS_ZC_DESTROY    = 0x413,
    SYS_SCHED_HINTS   = 0x420,
    SYS_SANDBOX_CREATE= 0x430,
    SYS_SANDBOX_RULE  = 0x431,
};

void syscalls_init() {
    portal::portal_init();
    elf::elf_init();
    userspace::init();

    // ─── Set up SYSCALL/SYSRET MSRs ─────────────────────────────
    uint64_t star = ((uint64_t)0x18 << 48) | ((uint64_t)0x08 << 32);
    wrmsr(MSR_IA32_STAR, star);

    // IA32_LSTAR = entry point for SYSCALL
    wrmsr(MSR_IA32_LSTAR, (uint64_t)syscall_entry);

    // IA32_FMASK = flags to clear on syscall (clear IF, etc.)
    wrmsr(MSR_IA32_FMASK, 0x200);

    // ─── Set up page table isolation ────────────────────────────
    kernel_cr3_addr = mm::paging_cr3();

    lib::kputs("[syscalls] SYSCALL/SYSRET handler installed\n");
    lib::kputs("[syscalls] Page table isolation ready\n");
    lib::kputs("[syscalls] Userspace subsystem ready\n");
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
        case SYS_USP_GETPID: {
            auto* t = task::task_current();
            return t ? t->id : 0;
        }
        case SYS_USP_WAITPID:
            task::task_yield();
            return 0;
        case SYS_USP_KILL: {
            auto* t = task::task_get(static_cast<uint64_t>(a1));
            if (t) {
                t->state = task::task_state::TERMINATED;
                return 0;
            }
            return static_cast<uint64_t>(-1);
        }
        case SYS_USP_OPEN: {
            const char* path = reinterpret_cast<const char*>(a1);
            auto* node = fs::vfs_lookup(path);
            if (!node) return static_cast<uint64_t>(-1);
            return reinterpret_cast<uint64_t>(node);
        }
        case SYS_USP_CLOSE: {
            auto* node = reinterpret_cast<fs::vfs_node*>(a1);
            (void)node;
            return 0;
        }
        case SYS_USP_READ_FD: {
            return 0;
        }
        case SYS_USP_WRITE_FD: {
            return 0;
        }
        case SYS_NET_SOCKET: {
            int domain = static_cast<int>(a1);
            int type = static_cast<int>(a2);
            int protocol = static_cast<int>(a3);
            return zerocopy::zc_create_channel(
                static_cast<zerocopy::channel_dir>(domain),
                static_cast<uint32_t>(type),
                static_cast<uint32_t>(protocol)
            );
        }
        case SYS_NET_BIND: {
            int fd = static_cast<int>(a1);
            const net::stack::sockaddr_in* addr =
                reinterpret_cast<const net::stack::sockaddr_in*>(a2);
            return net::stack::socket_bind(fd, addr);
        }
        case SYS_NET_LISTEN: {
            int fd = static_cast<int>(a1);
            int backlog = static_cast<int>(a2);
            return net::stack::socket_listen(fd, backlog);
        }
        case SYS_NET_ACCEPT: {
            int fd = static_cast<int>(a1);
            net::stack::sockaddr_in* addr =
                reinterpret_cast<net::stack::sockaddr_in*>(a2);
            return net::stack::socket_accept(fd, addr);
        }
        case SYS_NET_CONNECT: {
            int fd = static_cast<int>(a1);
            const net::stack::sockaddr_in* addr =
                reinterpret_cast<const net::stack::sockaddr_in*>(a2);
            return net::stack::socket_connect(fd, addr);
        }
        case SYS_NET_SEND: {
            int fd = static_cast<int>(a1);
            const void* data = reinterpret_cast<const void*>(a2);
            uint32_t len = static_cast<uint32_t>(a3);
            return zerocopy::zc_send(fd, data, len);
        }
        case SYS_NET_RECV: {
            int fd = static_cast<int>(a1);
            void* data = reinterpret_cast<void*>(a2);
            uint32_t len = static_cast<uint32_t>(a3);
            return zerocopy::zc_recv(fd, data, len);
        }
        case SYS_NET_CLOSE: {
            int fd = static_cast<int>(a1);
            return zerocopy::zc_destroy_channel(fd);
        }

        // Phase 7: AI-native OS syscalls
        case SYS_EBPF_LOAD: {
            const ebpf::ebpf_inst* insns = reinterpret_cast<const ebpf::ebpf_inst*>(a1);
            uint32_t cnt = static_cast<uint32_t>(a2);
            if (ebpf::ebpf_verify(insns, cnt)) return 0;
            return static_cast<uint64_t>(-1);
        }
        case SYS_EBPF_RUN: {
            ebpf::ebpf_program prog;
            prog.insns = reinterpret_cast<const ebpf::ebpf_inst*>(a1);
            prog.insn_cnt = static_cast<uint32_t>(a2);
            prog.result = 0;
            if (ebpf::ebpf_verify(prog.insns, prog.insn_cnt)) {
                uint64_t result = ebpf::ebpf_run_program(&prog);
                return prog.result;
            }
            return static_cast<uint64_t>(-1);
        }
        case SYS_ZC_CREATE: {
            zerocopy::channel_dir dir = static_cast<zerocopy::channel_dir>(a1);
            uint32_t slot_size = static_cast<uint32_t>(a2);
            uint32_t slot_count = static_cast<uint32_t>(a3);
            return zerocopy::zc_create_channel(dir, slot_size, slot_count);
        }
        case SYS_ZC_SEND: {
            int fd = static_cast<int>(a1);
            const void* data = reinterpret_cast<const void*>(a2);
            uint32_t len = static_cast<uint32_t>(a3);
            return zerocopy::zc_send(fd, data, len);
        }
        case SYS_ZC_RECV: {
            int fd = static_cast<int>(a1);
            void* data = reinterpret_cast<void*>(a2);
            uint32_t len = static_cast<uint32_t>(a3);
            return zerocopy::zc_recv(fd, data, len);
        }
        case SYS_ZC_DESTROY: {
            int fd = static_cast<int>(a1);
            return zerocopy::zc_destroy_channel(fd);
        }
        case SYS_SCHED_HINTS: {
            uint64_t task_id = a1;
            sched_hints::sched_hints* h = reinterpret_cast<sched_hints::sched_hints*>(a2);
            if (h) {
                sched_hints::sched_set_hints(task_id, h);
                return 0;
            }
            return static_cast<uint64_t>(-1);
        }
        case SYS_SANDBOX_CREATE: {
            sandbox::sandbox_domain* config = reinterpret_cast<sandbox::sandbox_domain*>(a1);
            return sandbox::sandbox_create(config);
        }
        case SYS_SANDBOX_RULE: {
            sandbox::sandbox_id_t id = static_cast<sandbox::sandbox_id_t>(a1);
            sandbox::sandbox_rule* rule = reinterpret_cast<sandbox::sandbox_rule*>(a2);
            return sandbox::sandbox_add_rule(id, rule);
        }

        default:
            lib::kprintf("unknown syscall %u\n", static_cast<unsigned>(num));
            return static_cast<uint64_t>(-1);
    }
}

} // namespace feather::kernel
