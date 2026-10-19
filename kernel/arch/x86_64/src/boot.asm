; FeatherOS Kernel Entry Point — x86_64
; Multiboot2 compliant boot loader entry
; Sets up a stack, loads the GDT, and jumps into C++ kernel_main.

[bits 64]
section .text
global _start
extern kernel_main

;==============================================================================
; Multiboot2 Header (must be within first 8 KiB of kernel, 8-byte aligned)
;==============================================================================
section .multiboot
align 8
multiboot_header:
    ; Multiboot2 magic number
    dd 0xe85250d6
    ; Architecture: 0 = i386, 1 = x86_64
    dd 0
    ; Header length (total bytes from magic to end)
    dd multiboot_header_end - multiboot_header
    ; Checksum (magic + arch + length + checksum = 0)
    dd 0x100000000 - (0xe85250d6 + 0 + (multiboot_header_end - multiboot_header))

    ; Tags
    ; Request a 1024x768x32 framebuffer.
    dw 5
    dw 0
    dd 20
    dd 1024
    dd 768
    dd 32
    dd 0                      ; align the next header tag to 8 bytes
    ; Tag type 0 = end of multiboot2 header
    dw 0                      ; type
    dw 0                      ; flags (optional)
    dd 8                      ; size

multiboot_header_end:

;==============================================================================
; Kernel Entry Point
;==============================================================================
_start:
    mov     r12, rax
    mov     r13, rbx
    ; ─── Set up a temporary stack ──────────────────────────────
    mov     rsp, stack_top

    ; ─── In 64-bit mode, DS/ES/SS are flat and ignored ──
    ; Only FS and GS are meaningful in 64-bit mode
    xor     ax, ax
    mov     fs, ax
    mov     gs, ax

    ; ─── Load GDT ─────────────────────────────────────────────
    lgdt    [gdt_descriptor]

    ; ─── CS already set by GRUB multiboot2 (long mode active) ──
    ; No far jump needed in 64-bit mode

    ; ─── Call the C++ kernel entry point ──────────────────────
    mov     rdi, r12
    mov     rsi, r13
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