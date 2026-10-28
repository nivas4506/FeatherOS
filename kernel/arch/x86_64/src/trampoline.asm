; FeatherOS Kernel — AP Trampoline (x86_64)
; Copied to low memory by the BSP; Application Processors start here in real mode
; after receiving a SIPI. Switches to long mode and calls ap_entrypoint.
;
; Build: nasm -f bin trampoline.asm -o trampoline.bin
; The binary is included as data in smp.cpp and copied to a low-memory address.

[bits 16]
org 0x7000                          ; AP starts here (SIPI vector 0x70)

section .text

global trampoline_start
global trampoline_end
global trampoline_gdt_ptr
global trampoline_gdt_code
global ap_entrypoint

trampoline_start:

    ; ─── Real mode: set up segments and stack ─────────────────────
    cli
    xor     ax, ax
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    mov     sp, 0x6FF0              ; stack just below trampoline

    ; ─── Load GDT for protected mode ──────────────────────────────
    lgdt    [gdt_desc_32]

    ; ─── Enable protected mode (CR0.PE) ──────────────────────────
    mov     eax, cr0
    or      eax, 1
    mov     cr0, eax

    ; ─── Far jump to 32-bit code (flush CS) ──────────────────────
    jmp     0x08:pm32_start

[bits 32]
pm32_start:
    mov     ax, 0x10                ; data segment
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax

    ; ─── Enable PAE (CR4.PAE) ────────────────────────────────────
    mov     eax, cr4
    or      eax, (1 << 5)           // PAE bit
    mov     cr4, eax

    ; ─── Set up a minimal identity-mapped PML4/PDPT ───────────────
    ; PML4 at 0x7F000, PDPT at 0x7F100, PD at 0x7F200
    ; Map first 4GB identity-style using one PDP entry pointing to one PD
    mov     eax, 0x7F000
    mov     cr3, eax                 ; CR3 = PML4 physical address

    ; PML4[0] -> PDPT (present, writable)
    mov     dword [0x7F000], 0x7F100 | 0x3
    ; PDPT[0] -> PD (present, writable)
    mov     dword [0x7F100], 0x7F200 | 0x3

    ; PD[0] -> 0x00000000 (2MB page, present, writable, huge page)
    mov     dword [0x7F200], 0x000000 | 0x83   // 0x83 = P | RW | PS(2MB)

    ; ─── Enable long mode (EFER.LME) ─────────────────────────────
    mov     ecx, 0xC0000080         ; EFER MSR
    rdmsr
    or      eax, (1 << 8)           ; LME bit
    wrmsr

    ; ─── Enable paging (CR0.PG) ──────────────────────────────────
    mov     eax, cr0
    or      eax, (1 << 31)          ; PG bit
    mov     cr0, eax

    ; ─── Far jump to 64-bit code ─────────────────────────────────
    jmp     0x08:long_mode_start

[bits 64]
long_mode_start:
    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax

    ; ─── Set up stack for this AP ────────────────────────────────
    ; Each AP gets its own stack below 1MB (simple scheme)
    mov     rsp, 0x6FF0

    ; ─── Get this CPU's APIC ID ───────────────────────────────────
    mov     ecx, 0x00000001         ; CPUID leaf 1
    xor     eax, eax
    xor     ebx, ebx
    xor     ecx2, ecx2              ; careful: clobber
    xor     edx, edx
    cpuid
    ; APIC ID is in EBX bits 24-31
    movzx   edi, bl                 ; use BL (apic id for single-core systems)
    ; Actually for multi-core, initial APIC ID is in EBX bits 31:24
    shr     ebx, 24
    movzx   edi, bl

    ; ─── Call ap_entrypoint(apic_id, cpu_id) ─────────────────────
    ; cpu_id: we'll determine from a global counter set by BSP
    ; For simplicity, pass cpu_id = 1 (first AP); BSP tracks this
    xor     esi, esi
    inc     esi                     ; cpu_id = 1 (will be overwritten properly)
    call    ap_entrypoint

    ; ─── AP idle loop ────────────────────────────────────────────
.halt:
    hlt
    jmp     .halt

;==============================================================================
; 32-bit GDT for the trampoline
;==============================================================================
align 8
gdt32_start:
    dq 0                                                        ; null
gdt32_code:                                                     ; 0x08 — code
    dw 0xFFFF
    dw 0
    db 0
    db 10011010b
    db 11001111b
    db 0
gdt32_data:                                                     ; 0x10 — data
    dw 0xFFFF
    dw 0
    db 0
    db 10010010b
    db 11001111b
    db 0
gdt32_end:

gdt_desc_32:
    dw gdt32_end - gdt32_start - 1
    dq gdt32_start

trampoline_end:
