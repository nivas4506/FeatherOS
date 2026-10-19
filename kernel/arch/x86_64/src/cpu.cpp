// FeatherOS Kernel — CPU implementation x86_64
#include "cpu.h"

namespace feather::arch::cpu {

void cpuid(uint32_t leaf, uint32_t& eax, uint32_t& ebx, uint32_t& ecx, uint32_t& edx) {
    __asm__ volatile (
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(leaf), "c"(0)
    );
}

void enable_sse() {
    uint64_t cr0 = read_cr0();
    cr0 &= ~(1ULL << 2);   // clear EM
    cr0 |=  (1ULL << 1);   // set MP
    write_cr0(cr0);

    uint64_t cr4 = read_cr4();
    cr4 |= (1ULL << 9);  // OSFXSR
    cr4 |= (1ULL << 10); // OSXMMEXCPT
    write_cr4(cr4);
}

} // namespace feather::arch::cpu
