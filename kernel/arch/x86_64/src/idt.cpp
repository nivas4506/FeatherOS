#include "idt.h"

extern "C" uint64_t isr_stub_addresses[256];

namespace feather::arch::idt {

alignas(16) idt_entry idt[256];
static idt_ptr idtp;

void set_gate(uint8_t num, uint64_t handler, uint16_t sel, uint8_t ist, uint8_t type_attr) {
    idt[num].offset_low  = static_cast<uint16_t>(handler & 0xFFFF);
    idt[num].selector    = sel;
    idt[num].ist         = ist;
    idt[num].type_attr   = type_attr;
    idt[num].offset_mid  = static_cast<uint16_t>((handler >> 16) & 0xFFFF);
    idt[num].offset_high = static_cast<uint32_t>(handler >> 32);
    idt[num].zero        = 0;
}

void load() {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = reinterpret_cast<uint64_t>(&idt[0]);
    __asm__ volatile ("lidt %0" : : "m"(idtp));
}

void init() {
    for (int i = 0; i < 256; i++) {
        uint8_t attr = (i == 0x80) ? 0xEE : 0x8E; // syscall trap gate DPL=3
        set_gate(static_cast<uint8_t>(i), isr_stub_addresses[i], 0x08, 0, attr);
    }
    load();
}

} // namespace feather::arch::idt
