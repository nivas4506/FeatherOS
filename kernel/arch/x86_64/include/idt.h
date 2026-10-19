#pragma once

#include <cstdint>

namespace feather::arch::idt {

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void init();
void set_gate(uint8_t num, uint64_t handler, uint16_t sel, uint8_t ist, uint8_t type_attr);
void load();

} // namespace feather::arch::idt
