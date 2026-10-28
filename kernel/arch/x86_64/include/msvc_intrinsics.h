#pragma once
// FeatherOS Kernel — MSVC intrinsic compatibility layer (x86_64)
// Provides MSVC-intrinsic based implementations of the low-level CPU / I/O
// operations that are normally written with GNU __asm__ inline assembly.
// MSVC x64 does not support inline assembly (__asm), so privileged / control
// register operations that lack intrinsics are declared here (extern "C" at
// global scope) and implemented in arch/x86_64/src/msvc_helpers.asm
// (compiled with -f win64).
#ifdef _MSC_VER
#include <intrin.h>
#include <cstdint>

//---------------------------------------------------------------------------
// External assembly helpers (defined in msvc_helpers.asm, C linkage)
// Declared at global scope because MSVC requires extern "C" at global scope.
//---------------------------------------------------------------------------
extern "C" {

void __hlt();
void __cli();
void __sti();

uint64_t __read_cr3();
void __write_cr3(uint64_t);

uint64_t __read_cr4();
void __write_cr4(uint64_t);

uint64_t __read_cr2();

uint64_t __rdmsr(uint32_t msr);
void __wrmsr(uint32_t msr, uint64_t val);

void __lidt(void* idtp);
void __lgdt(void* gdtr);
void __invlpg(void* virt);
void __reload_segments();

} // extern "C"

namespace feather::arch::cpu {

//---------------------------------------------------------------------------
// Instructions that HAVE MSVC intrinsics
//---------------------------------------------------------------------------

inline void hlt() { __hlt(); }
inline void cli() { __cli(); }
inline void sti() { __sti(); }
inline void nop() { __nop(); }
inline void pause() { _mm_pause(); }
inline void cpu_pause() { pause(); }

// Read / write CR0 — MSVC provides intrinsics
inline uint64_t read_cr0() { return __readcr0(); }
inline void write_cr0(uint64_t v) { __writecr0(v); }

// Read / write CR3 — no MSVC intrinsic; uses asm helper
inline uint64_t read_cr3() { return __read_cr3(); }
inline void write_cr3(uint64_t v) { __write_cr3(v); }

// Read / write CR4 — no MSVC intrinsic; uses asm helper
inline uint64_t read_cr4() { return __read_cr4(); }
inline void write_cr4(uint64_t v) { __write_cr4(v); }

// Read RFLAGS — MSVC provides __readeflags()
inline uint64_t read_rflags() { return static_cast<uint64_t>(__readeflags()); }

// CPUID — MSVC provides __cpuidex()
inline void cpuid(uint32_t leaf, uint32_t& eax, uint32_t& ebx, uint32_t& ecx, uint32_t& edx) {
    int cpuInfo[4] = {0};
    __cpuidex(cpuInfo, static_cast<int>(leaf), 0);
    eax = static_cast<uint32_t>(cpuInfo[0]);
    ebx = static_cast<uint32_t>(cpuInfo[1]);
    ecx = static_cast<uint32_t>(cpuInfo[2]);
    edx = static_cast<uint32_t>(cpuInfo[3]);
}

} // namespace feather::arch::cpu

//---------------------------------------------------------------------------
// I/O port operations — MSVC provides __inbyte/__outbyte etc.
//---------------------------------------------------------------------------
namespace feather::arch::io {

inline void outb(uint16_t port, uint8_t val) {
    __outbyte(static_cast<unsigned short>(port), val);
}
inline uint8_t inb(uint16_t port) {
    return static_cast<uint8_t>(__inbyte(static_cast<unsigned short>(port)));
}
inline void outw(uint16_t port, uint16_t val) {
    __outword(static_cast<unsigned short>(port), val);
}
inline uint16_t inw(uint16_t port) {
    return static_cast<uint16_t>(__inword(static_cast<unsigned short>(port)));
}
inline void outl(uint16_t port, uint32_t val) {
    __outdword(static_cast<unsigned short>(port), val);
}
inline uint32_t inl(uint16_t port) {
    return __indword(static_cast<unsigned short>(port));
}

} // namespace feather::arch::io

//---------------------------------------------------------------------------
// Spinlock pause hint
//---------------------------------------------------------------------------
namespace feather::lib {

inline void spinlock_pause() {
    _mm_pause();
}

} // namespace feather::lib

#endif // _MSC_VER
