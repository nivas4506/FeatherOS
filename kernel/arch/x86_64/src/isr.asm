; FeatherOS — 64-bit ISR stubs + common trampoline
BITS 64
section .text

%macro ISR_NOERR 1
global isr_stub_%1
isr_stub_%1:
    push    qword 0
    push    qword %1
    jmp     isr_common
%endmacro

%macro ISR_ERR 1
global isr_stub_%1
isr_stub_%1:
    push    qword %1
    jmp     isr_common
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19

%assign i 20
%rep 12
    ISR_NOERR i
    %assign i i+1
%endrep

; IRQ 0-15 -> vectors 32-47
%assign i 32
%rep 16
    ISR_NOERR i
    %assign i i+1
%endrep

; remaining 48-255 (includes 0x80 syscall)
%assign i 48
%rep 208
    ISR_NOERR i
    %assign i i+1
%endrep

extern interrupt_dispatch

global isr_common
isr_common:
    push    rax
    push    rbx
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    push    rbp
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15

    mov     rdi, [rsp + 15 * 8]      ; vector
    mov     rsi, [rsp + 16 * 8]      ; error code
    call    interrupt_dispatch

    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax

    add     rsp, 16                  ; drop vector + error
    iretq

section .rodata
global isr_stub_addresses
isr_stub_addresses:
%assign i 0
%rep 256
    dq isr_stub_%+i
    %assign i i+1
%endrep
