#pragma once
// FeatherOS Kernel — CPU / system helpers x86_64

#include <cstdint>

namespace feather::arch::cpu {

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

} // namespace feather::arch::cpu
