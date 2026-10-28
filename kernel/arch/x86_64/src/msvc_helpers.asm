; FeatherOS Kernel — MSVC helper assembly (x86_64)
; Provides implementations for privileged instructions that lack MSVC intrinsics.
; Assembled with NASM -f win64 for consumption by the MSVC linker.

global __hlt
global __cli
global __sti
global __read_cr3
global __write_cr3
global __read_cr4
global __write_cr4
global __read_cr2
global __rdmsr
global __wrmsr
global __lidt
global __lgdt
global __invlpg
global __reload_segments

section .text

; void __hlt(void)
__hlt:
    hlt
    ret

; void __cli(void)
__cli:
    cli
    ret

; void __sti(void)
__sti:
    sti
    ret

; uint64_t __read_cr3(void)
__read_cr3:
    mov rax, cr3
    ret

; void __write_cr3(uint64_t value)  (RCX = first arg on Win64)
__write_cr3:
    mov cr3, rcx
    ret

; uint64_t __read_cr4(void)
__read_cr4:
    mov rax, cr4
    ret

; void __write_cr4(uint64_t value)
__write_cr4:
    mov cr4, rcx
    ret

; uint64_t __read_cr2(void)
__read_cr2:
    mov rax, cr2
    ret

; uint64_t __rdmsr(uint32_t msr)   (ECX = msr)
; Returns: EDX:EAX
__rdmsr:
    rdmsr
    ret

; void __wrmsr(uint32_t msr, uint64_t value)
; ECX = msr, EDX:EAX = value
__wrmsr:
    wrmsr
    ret

; void __lidt(void* idtp)          (RCX = pointer to idtr)
__lidt:
    lidt [rcx]
    ret

; void __lgdt(void* gdtr)          (RCX = pointer to gdtr)
__lgdt:
    lgdt [rcx]
    ret

; void __invlpg(void* virt)        (RCX = virtual address)
__invlpg:
    invlpg [rcx]
    ret

; void __reload_segments(void)
; Far return to reload CS register after GDT load.
__reload_segments:
    push 8
    mov rax, .return_addr
    push rax
    retf
.return_addr:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    ret
