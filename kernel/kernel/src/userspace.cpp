// FeatherOS Kernel — Userspace mode switching
// Sets up CPU state for SYSRET to userspace and manages user task context.

#include "userspace.h"
#include "../../arch/x86_64/include/cpu.h"
#include "../../mm/include/paging.h"
#include "../../lib/include/printf.h"

#ifdef _MSC_VER
extern "C" void __sysret_setup(uint64_t rip, uint64_t rsp, uint64_t rflags,
                               uint64_t user_stack);
#endif

namespace feather::kernel::userspace {

namespace {

// Current user context (per-CPU in a full implementation)
static user_context_t* g_user_ctx = nullptr;

// User stack for the current task
static uint64_t g_user_stack = 0;

} // anonymous namespace

void init() {
    lib::kputs("[userspace] userspace subsystem initialized\n");
}

void prepare_user_context(void* /*task*/, uint64_t entry_point,
                          uint64_t user_stack) {
    g_user_stack = user_stack;

    if (g_user_ctx) {
        g_user_ctx->rip   = entry_point;
        g_user_ctx->rsp   = user_stack;
        g_user_ctx->rflags = 0x202; // IF=1, reserved bit 1
        g_user_ctx->cs    = 0x1B;   // User code segment (ring 3)
        g_user_ctx->ss    = 0x23;   // User data segment (ring 3)
        g_user_ctx->ds    = 0x23;
        g_user_ctx->es    = 0x23;
        g_user_ctx->fs    = 0;
        g_user_ctx->gs    = 0;
        g_user_ctx->rax   = 0;
        g_user_ctx->rcx   = 0;
        g_user_ctx->rdx   = 0;
        g_user_ctx->rbx   = 0;
        g_user_ctx->rsi   = 0;
        g_user_ctx->rdi   = 0;
        g_user_ctx->rbp   = 0;
        g_user_ctx->r8    = 0;
        g_user_ctx->r9    = 0;
        g_user_ctx->r10   = 0;
        g_user_ctx->r11   = 0;
        g_user_ctx->r12   = 0;
        g_user_ctx->r13   = 0;
        g_user_ctx->r14   = 0;
        g_user_ctx->r15   = 0;
    }
}

[[noreturn]] void switch_to_user(const user_context_t* ctx) {
    if (!ctx) {
        arch::cpu::hlt();
        for (;;) {}
    }

    // Switch to user CR3
    mm::switch_to_user_cr3();

    // Use assembly helper to execute SYSRET
#ifdef _MSC_VER
    __sysret_setup(ctx->rip, ctx->rsp, ctx->rflags, g_user_stack);
#else
    // For GNU: inline assembly SYSRET
    __asm__ volatile (
        "mov %0, %%rcx\n"    // return RIP
        "mov %1, %%r11\n"    // return RFLAGS
        "mov %2, %%rsp\n"    // user stack
        "sysretq\n"
        :
        : "r"(ctx->rip), "r"(ctx->rflags), "r"(g_user_stack)
        : "rcx", "r11", "rsp", "memory"
    );
#endif

    // Should never return
    for (;;) arch::cpu::hlt();
}

void switch_to_kernel(uint64_t syscall_num,
                      uint64_t a1, uint64_t a2, uint64_t a3,
                      uint64_t a4, uint64_t a5,
                      user_context_t* save_ctx) {
    // Switch back to kernel CR3
    mm::switch_to_kernel_cr3();

    // Save user context
    if (save_ctx) {
        g_user_ctx = save_ctx;
        // The caller (syscall_entry.asm) should have already saved registers
        // We just need to record the context pointer
    }

    (void)syscall_num;
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
}

user_context_t* current_user_context() {
    return g_user_ctx;
}

} // namespace feather::kernel::userspace
