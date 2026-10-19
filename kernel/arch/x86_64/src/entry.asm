; FeatherOS Kernel Entry Point — x86_64
; Sets up a stack, loads the GDT, and jumps into C++ kernel_main.
; This is a freestanding kernel — no BIOS, no multiboot, loaded at 1MiB.

[bits 64]
section .text
global _start
extern kernel_main

_start:
    ; ─── Set up a temporary stack ──────────────────────────────
    mov     rsp, stack_top

    ; ─── Clear registers ──────────────────────────────────────
    xor     ax, ax
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax

    ; ─── Load GDT ─────────────────────────────────────────────
    lgdt    [gdt_descriptor]

    ; ─── Reload CS by far jump to set up instruction pipeline ─
    jmp     0x08:.flush_cs
.flush_cs:

    ; ─── Call the C++ kernel entry point ──────────────────────
    extern kernel_main
    call    kernel_main

.halt:
    hlt
    jmp     .halt

section .rodata
gdt_start:
    dq 0                                                ; null descriptor
gdt_code:                                               ; 0x08 — kernel code
    dw 0xFFFF                                           ; limit low
    dw 0                                                ; base low
    db 0                                                ; base middle
    db 10011010b                                        ; access (present, ring0, code, executable, readable)
    db 11001111b                                        ; flags + limit high (4KB granularity, 64-bit)
    db 0                                                ; base high
gdt_data:                                               ; 0x10 — kernel data
    dw 0xFFFF
    dw 0
    db 0
    db 10010010b                                        ; access (present, ring0, data, writable)
    db 11001111b
    db 0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1                          ; limit
    dq gdt_start                                         ; base

section .bss
align 16
stack_bottom:
    resb 16384                                          ; 16 KiB stack
stack_top:
