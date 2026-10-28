#pragma once
// FeatherOS Kernel — CPU / system helpers x86_64

#ifdef _MSC_VER
#include <intrin.h>
#endif

#include <cstdint>

namespace feather::arch::cpu {

#ifdef _MSC_VER
// MSVC x64 does not support GNU __asm__ inline assembly. Use intrinsics
// (<intrin.h>) where available, and external assembly helpers
// (arch/x86_64/src/msvc_helpers.asm, -f win64) for the rest.

extern "C" void __hlt();
extern "C" void __cli();
extern "C" void __sti();
extern "C" uint64_t __read_cr3();
extern "C" void __write_cr3(uint64_t);
extern "C" uint64_t __read_cr4();
extern "C" void __write_cr4(uint64_t);
extern "C" uint64_t __read_cr2();

inline void hlt() { __hlt(); }
inline void cli() { __cli(); }
inline void sti() { __sti(); }
inline void nop() { __nop(); }
inline void pause() { _mm_pause(); }
inline void cpu_pause() { pause(); }
inline void asm_pause() { _mm_pause(); }

inline uint64_t read_cr0() { return __readcr0(); }
inline void write_cr0(uint64_t v) { __writecr0(v); }
inline uint64_t read_cr3() { return __read_cr3(); }
inline void write_cr3(uint64_t v) { __write_cr3(v); }
inline uint64_t read_cr4() { return __read_cr4(); }
inline void write_cr4(uint64_t v) { __write_cr4(v); }
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

// Enable SSE
inline void enable_sse() {
    uint64_t cr0 = read_cr0();
    cr0 &= ~(1ULL << 2);   // clear EM
    cr0 |=  (1ULL << 1);   // set MP
    write_cr0(cr0);
    uint64_t cr4 = read_cr4();
    cr4 |= (1ULL << 9);  // OSFXSR
    cr4 |= (1ULL << 10); // OSXMMEXCPT
    write_cr4(cr4);
}
#else
inline void hlt() {
    __asm__ volatile ("hlt");
}

inline void cli() {
    __asm__ volatile ("cli");
}

inline void sti() {
    __asm__ volatile ("sti");
}

inline void nop() {
    __asm__ volatile ("nop");
}

inline void pause() {
    __asm__ volatile ("pause");
}

inline void cpu_pause() { pause(); }

inline void asm_pause() {
    __asm__ volatile ("pause");
}

// Read CR0
inline uint64_t read_cr0() {
    uint64_t v;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(v));
    return v;
}

// Write CR0
inline void write_cr0(uint64_t v) {
    __asm__ volatile ("mov %0, %%cr0" : : "r"(v));
}

// Read CR3
inline uint64_t read_cr3() {
    uint64_t v;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(v));
    return v;
}

// Write CR3
inline void write_cr3(uint64_t v) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(v));
}

// Read CR4
inline uint64_t read_cr4() {
    uint64_t v;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(v));
    return v;
}

// Write CR4
inline void write_cr4(uint64_t v) {
    __asm__ volatile ("mov %0, %%cr4" : : "r"(v));
}

// Read RFLAGS
inline uint64_t read_rflags() {
    uint64_t v;
    __asm__ volatile ("pushfq; pop %0" : "=r"(v));
    return v;
}

// CPUID
void cpuid(uint32_t leaf, uint32_t& eax, uint32_t& ebx, uint32_t& ecx, uint32_t& edx);

// Enable SSE
void enable_sse();
#endif

} // namespace feather::arch::cpu
