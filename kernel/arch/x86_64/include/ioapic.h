#pragma once
// FeatherOS Kernel — IO APIC (x64)
// Routes device interrupts to local APICs. Replaces legacy PIC when available.

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>

namespace feather::arch::ioapic {

// IO APIC register offsets (memory-mapped)
enum ioapic_reg : uint32_t {
    REG_IOAPIC_ID      = 0x00,
    REG_IOAPIC_VERSION = 0x01,
    REG_IOAPIC_ARB     = 0x02,
    REG_IOAPIC_RED_TBL = 0x10,  // Redirection table base (64 entries, 2x32-bit each)
};

// Initialize IO APIC (redirect interrupts to BSP LAPIC)
void init();

// Configure a redirection table entry: map GSI to a vector on a target LAPIC
void set_redir_entry(uint8_t gsi, uint8_t vector, uint8_t apic_id, bool masked);

// EOI to IO APIC (for level-triggered interrupts)
void eoi(uint8_t gsi);

// Get the IO APIC base address
uintptr_t get_base();

} // namespace feather::arch::ioapic
