#pragma once
// FeatherOS Kernel — Userspace mode switching
// Sets up CPU state for SYSRET to userspace and manages user task context.

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>
#include <cstddef>

namespace feather::kernel::userspace {

// User mode register state (saved on syscall entry, restored on sysret)
struct alignas(16) user_context_t {
    uint64_t rip;      // User instruction pointer
    uint64_t rsp;      // User stack pointer
    uint64_t rflags;   // User RFLAGS
    uint64_t rax, rcx, rdx, rbx;
    uint64_t rsi, rdi, rbp;
    uint64_t r8,  r9,  r10, r11, r12, r13, r14, r15;
    uint64_t cs, ss, ds, es, fs, gs;
};

// Initialize userspace subsystem
void init();

// Prepare a task for userspace execution
// Sets up the user_context_t with entry point, stack, and segment selectors
void prepare_user_context(void* task, uint64_t entry_point, uint64_t user_stack);

// Switch from kernel to user mode (via SYSRET)
// This is called from the syscall exit path
[[noreturn]] void switch_to_user(const user_context_t* ctx);

// Switch from user to kernel mode (syscall entry)
// Called from syscall_entry.asm
void switch_to_kernel(uint64_t syscall_num,
                      uint64_t a1, uint64_t a2, uint64_t a3,
                      uint64_t a4, uint64_t a5,
                      user_context_t* save_ctx);

// Get the current user context pointer
user_context_t* current_user_context();

} // namespace feather::kernel::userspace
