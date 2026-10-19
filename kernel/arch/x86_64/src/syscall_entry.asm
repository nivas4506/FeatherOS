; FeatherOS Syscall Entry — x86_64
; Handles syscall instruction from userspace, saves state, calls C handler,
; then restores state and returns via sysretq to ring 3.
;
; Page table isolation:
;   On entry:  save user CR3, switch to kernel CR3
;   On exit:   switch back to user CR3, then sysretq

[bits 64]
section .text
global syscall_entry
extern feather_kernel_syscall_handler
extern user_rsp_save
extern kernel_stack_top
extern user_cr3_save

;==============================================================================
; SYSCALL_ENTRY — Entry point from userspace syscall instruction
;==============================================================================
syscall_entry:
    ; CPU state on syscall entry (from ring 3):
    ;   RCX = return RIP
    ;   R11 = return RFLAGS (IF cleared)
    ;   RSP = user stack pointer (syscall does NOT change RSP)
    ;   RAX = syscall number
    ;   RDI = arg1, RSI = arg2, RDX = arg3, R10 = arg4, R8 = arg5, R9 = arg6

    ; ─── Save user CR3 and switch to kernel CR3 ─────────────────
    mov     rax, cr3
    mov     [user_cr3_save], rax
    mov     rax, [rel kernel_cr3_addr]
    mov     cr3, rax

    ; ─── Save user RSP and switch to kernel stack ───────────────
    mov     [user_rsp_save], rsp
    lea     rsp, [rel kernel_stack_top]

    ; ─── Save all general-purpose registers ─────────────────────
    push    rax                 ; syscall number
    push    rcx                 ; return RIP
    push    r11                 ; return RFLAGS
    push    rdi
    push    rsi
    push    rdx
    push    r10                 ; 4th arg (syscall uses r10, not rcx)
    push    r8
    push    r9
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp

    ; ─── Set up arguments for C handler ─────────────────────────
    ; C: uint64_t handler(uint64_t num, uint64_t a1, uint64_t a2,
    ;                     uint64_t a3, uint64_t a4, uint64_t a5)
    ; ABI: rdi=num, rsi=a1, rdx=a2, rcx=a3, r8=a4, r9=a5
    ;
    ; Stack after pushes (top to bottom):
    ;   [rsp+0]  = rbp
    ;   [rsp+8]  = r15
    ;   [rsp+16] = r14
    ;   [rsp+24] = r13
    ;   [rsp+32] = r12
    ;   [rsp+40] = r9
    ;   [rsp+48] = r8
    ;   [rsp+56] = r10  (syscall arg4)
    ;   [rsp+64] = rdx  (syscall arg3)
    ;   [rsp+72] = rsi  (syscall arg2)
    ;   [rsp+80] = rdi  (syscall arg1)
    ;   [rsp+88] = r11  (return RFLAGS)
    ;   [rsp+96] = rcx  (return RIP)
    ;   [rsp+104]= rax  (syscall number)

    pop     rax                 ; rax = syscall number
    mov     rdi, rax            ; arg1 = num
    mov     rsi, [rsp+80]       ; arg2 = syscall rdi
    mov     rdx, [rsp+72]       ; arg3 = syscall rsi
    mov     rcx, [rsp+64]       ; arg4 = syscall rdx
    mov     r8,  [rsp+56]       ; arg5 = syscall r10
    mov     r9,  [rsp+48]       ; arg6 = syscall r8

    call    feather_kernel_syscall_handler

    ; ─── Restore all registers (reverse order) ──────────────────
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r9
    pop     r8
    pop     r10
    pop     rdx
    pop     rsi
    pop     rdi
    pop     r11                 ; return RFLAGS
    pop     rcx                 ; return RIP

    ; rax = return value (set by handler)
    ; rcx = return RIP
    ; r11 = return RFLAGS
    ; rsp now points to original rax position (syscall number we popped)
    add     rsp, 8              ; skip the syscall number slot

    ; ─── Restore user RSP ───────────────────────────────────────
    mov     rsp, [user_rsp_save]

    ; ─── Switch back to user CR3 ────────────────────────────────
    mov     rax, [user_cr3_save]
    mov     cr3, rax

    ; sysretq: CS = STAR[63:48] | 3, SS = STAR[63:48] + 8 | 3? 
    ; We set up STAR so that user CS = 0x18 | 3 = 0x1B, user SS = 0x20 | 3 = 0x23
    o64 sysret

section .data
align 8
global kernel_cr3_addr
kernel_cr3_addr:
    dq 0                        ; Filled in by syscalls_init()

section .bss
align 8
user_cr3_save:
    resq 1                      ; Saved user CR3 during syscall