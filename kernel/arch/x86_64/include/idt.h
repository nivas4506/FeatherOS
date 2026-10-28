#pragma once

#ifdef _MSC_VER
#include <intrin.h>
#endif

#include <cstdint>

namespace feather::arch::idt {

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
};
struct idt_ptr {
    uint16_t limit;
    uint64_t base;
};
#ifdef _MSC_VER
#pragma pack(pop)
#endif

void init();
void set_gate(uint8_t num, uint64_t handler, uint16_t sel, uint8_t ist, uint8_t type_attr);
void load();

} // namespace feather::arch::idt
