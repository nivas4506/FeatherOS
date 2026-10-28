#include "gdt.h"

#ifdef _MSC_VER
extern "C" void __reload_segments();
extern "C" void __lgdt(void*);
#endif

namespace feather::arch::gdt {

struct gdt_entry {
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#ifndef _MSC_VER
} __attribute__((packed));
#else
};
#endif

// GDT entries:
//  0x00 - Null
//  0x08 - Kernel code (ring 0, 64-bit)
//  0x10 - Kernel data (ring 0)
//  0x18 - User code (ring 3, 64-bit)
//  0x20 - User data (ring 3)
alignas(8) static gdt_entry gdt[5];
static gdt_ptr gdtr;

static void reload_segments() {
#ifdef _MSC_VER
    __reload_segments();
#else
    __asm__ volatile (
        "pushq $0x08\n"
        "leaq  1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "movw  $0x10, %%ax\n"
        "movw  %%ax, %%ds\n"
        "movw  %%ax, %%es\n"
        "movw  %%ax, %%ss\n"
        "movw  %%ax, %%fs\n"
        "movw  %%ax, %%gs\n"
        :
        :
        : "rax", "memory"
    );
#endif
}

void init() {
    // Null descriptor
    gdt[0] = {};
    // Kernel code 64-bit: P=1 DPL=0 S=1 E=1 RW=1, L=1 (64-bit)
    gdt[1] = {0, 0, 0, 0x9A, 0x20, 0};
    // Kernel data: P=1 DPL=0 S=1 RW=1
    gdt[2] = {0, 0, 0, 0x92, 0x00, 0};
    // User code 64-bit: P=1 DPL=3 S=1 E=1 RW=1, L=1 (64-bit)
    gdt[3] = {0, 0, 0, 0xFA, 0x20, 0};
    // User data: P=1 DPL=3 S=1 RW=1
    gdt[4] = {0, 0, 0, 0xF2, 0x00, 0};

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base  = reinterpret_cast<uint64_t>(&gdt[0]);
#ifdef _MSC_VER
    __lgdt(&gdtr);
#else
    __asm__ volatile ("lgdt %0" : : "m"(gdtr));
#endif
    reload_segments();
}

} // namespace feather::arch::gdt
