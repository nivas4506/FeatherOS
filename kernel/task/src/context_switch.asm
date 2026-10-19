; FeatherOS Kernel — Context Switch Assembly (x86_64)
; void context_switch(cpu_context* old_ctx, cpu_context* new_ctx);

[bits 64]
section .text
global context_switch

context_switch:
    ; rdi = old_ctx, rsi = new_ctx
    ; Save old context
    mov [rdi + 0x00], rax
    mov [rdi + 0x08], rcx
    mov [rdi + 0x10], rdx
    mov [rdi + 0x18], rbx
    mov [rdi + 0x20], rsp
    mov [rdi + 0x28], rbp
    mov [rdi + 0x30], rsi
    mov [rdi + 0x38], rdi
    mov [rdi + 0x40], r8
    mov [rdi + 0x48], r9
    mov [rdi + 0x50], r10
    mov [rdi + 0x58], r11
    mov [rdi + 0x60], r12
    mov [rdi + 0x68], r13
    mov [rdi + 0x70], r14
    mov [rdi + 0x78], r15
    
    pushfq
    pop rax
    mov [rdi + 0x80], rax        ; rflags
    
    ; Save RIP - we can't directly, but we can use a trick
    lea rax, [rip + 1f]
    mov [rdi + 0x88], rax        ; rip (return address)
1:
    
    ; Load new context
    mov rax, [rsi + 0x80]
    push rax
    popfq                        ; rflags
    
    mov rax, [rsi + 0x00]
    mov rcx, [rsi + 0x08]
    mov rdx, [rsi + 0x10]
    mov rbx, [rsi + 0x18]
    mov rsp, [rsi + 0x20]
    mov rbp, [rsi + 0x28]
    mov rsi, [rsi + 0x30]
    mov rdi, [rsi + 0x38]
    mov r8,  [rsi + 0x40]
    mov r9,  [rsi + 0x48]
    mov r10, [rsi + 0x50]
    mov r11, [rsi + 0x58]
    mov r12, [rsi + 0x60]
    mov r13, [rsi + 0x68]
    mov r14, [rsi + 0x70]
    mov r15, [rsi + 0x78]
    
    ; Jump to new RIP
    jmp [rsi + 0x88]
    ret