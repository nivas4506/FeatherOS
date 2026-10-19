; FeatherOS — Multiboot2 bootstrap: 32-bit protected mode -> 64-bit long mode
; GRUB loads this ELF at 1 MiB and jumps to _start with EAX = 0x36D76289.

BITS 32

section .multiboot align=8
mb_start:
    dd 0xE85250D6                    ; magic
    dd 0                             ; i386 protected mode
    dd mb_end - mb_start             ; header length
    dd -(0xE85250D6 + 0 + (mb_end - mb_start))
    dw 0                             ; end tag
    dw 0
    dd 8
mb_end:

section .bss
align 4096
p4_table:       resb 4096
p3_table:       resb 4096
p2_table:       resb 4096
align 16
stack_bottom:   resb 32768
stack_top:

section .rodata
align 16
gdt64:
    dq 0
.code: equ $ - gdt64
    ; kernel code: present, ring0, executable, readable, long-mode
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)
.data: equ $ - gdt64
    ; kernel data: present, ring0, writable
    dq (1 << 44) | (1 << 47) | (1 << 41)
.pointer:
    dw $ - gdt64 - 1
    dd gdt64

section .text
global _start
extern kernel_main

_start:
    cli
    mov     esp, stack_top
    xor     ebp, ebp

    ; Identity-map the first 1 GiB with 2 MiB huge pages.
    mov     eax, p3_table
    or      eax, 0x03                ; present | writable
    mov     [p4_table], eax

    mov     eax, p2_table
    or      eax, 0x03
    mov     [p3_table], eax

    xor     ecx, ecx
.map_p2:
    mov     eax, 0x200000
    mul     ecx                      ; EAX = ecx * 2 MiB
    or      eax, 0x83                ; present | writable | huge
    mov     [p2_table + ecx * 8], eax
    inc     ecx
    cmp     ecx, 512
    jne     .map_p2

    ; CR4.PAE
    mov     eax, cr4
    or      eax, 1 << 5
    mov     cr4, eax

    mov     eax, p4_table
    mov     cr3, eax

    ; EFER.LME
    mov     ecx, 0xC0000080
    rdmsr
    or      eax, 1 << 8
    wrmsr

    ; CR0.PG | CR0.PE
    mov     eax, cr0
    or      eax, (1 << 31) | (1 << 0)
    mov     cr0, eax

    lgdt    [gdt64.pointer]
    jmp     gdt64.code:long_mode_start

BITS 64
long_mode_start:
    mov     ax, gdt64.data
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    mov     fs, ax
    mov     gs, ax
    mov     rsp, stack_top
    xor     rbp, rbp
    call    kernel_main
.hang:
    hlt
    jmp     .hang
