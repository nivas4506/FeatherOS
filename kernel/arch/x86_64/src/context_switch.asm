; void context_switch(cpu_context* old_ctx, cpu_context* new_ctx);
; cpu_context: rip, rsp, rbp, rbx, r12, r13, r14, r15, rflags
BITS 64
section .text
global context_switch

context_switch:
    ; Save callee-saved state of the outgoing task.
    lea     rax, [rel .resume]
    mov     [rdi + 0], rax           ; rip
    mov     [rdi + 8], rsp
    mov     [rdi + 16], rbp
    mov     [rdi + 24], rbx
    mov     [rdi + 32], r12
    mov     [rdi + 40], r13
    mov     [rdi + 48], r14
    mov     [rdi + 56], r15
    pushfq
    pop     rax
    mov     [rdi + 64], rax          ; rflags

    ; Restore incoming task.
    mov     rax, [rsi + 64]
    push    rax
    popfq
    mov     rsp, [rsi + 8]
    mov     rbp, [rsi + 16]
    mov     rbx, [rsi + 24]
    mov     r12, [rsi + 32]
    mov     r13, [rsi + 40]
    mov     r14, [rsi + 48]
    mov     r15, [rsi + 56]
    jmp     [rsi]                    ; rip

.resume:
    ret
