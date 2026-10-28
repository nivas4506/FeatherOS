; FeatherOS Kernel — SYSRET assembly helper (x86_64)
; Sets up registers and executes SYSRET to return to userspace.
; MSVC does not support inline assembly, so this is a separate object file.
;
; MSVC x64 calling convention:
;   RCX = 1st arg (rip)
;   RDX = 2nd arg (rsp / user stack)
;   R8  = 3rd arg (rflags)
;   R9  = 4th arg (user_stack, unused)
;
; SYSRET requirements:
;   RCX = return RIP
;   R11 = return RFLAGS
;   RSP = user stack pointer

[bits 64]
section .text

global __sysret_setup

__sysret_setup:
    mov     r11, r8         ; R8 (3rd arg = rflags) -> R11 for SYSRET
    mov     rsp, rdx        ; RDX (2nd arg = user stack) -> RSP
    sysretq                 ; RCX already has RIP (1st arg)
    ret                     ; Should never reach here
