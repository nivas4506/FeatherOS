// FeatherOS Kernel — IO APIC implementation

#include "ioapic.h"
#include "lapic.h"
#include "acpi.h"

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace feather::arch::ioapic {

namespace {

inline volatile uint32_t* sel_reg() {
    return reinterpret_cast<volatile uint32_t*>(acpi::ioapic_address(0) + 0x00);
}

inline volatile uint32_t* wr_reg() {
    return reinterpret_cast<volatile uint32_t*>(acpi::ioapic_address(0) + 0x10);
}

// Read a 32-bit IOAPIC register by writing the selector then reading the window
uint32_t read_reg(uint32_t reg) {
    *sel_reg() = reg;
    return *wr_reg();
}

void write_reg(uint32_t reg, uint32_t value) {
    *sel_reg() = reg;
    *wr_reg() = value;
}

} // anonymous namespace

void init() {
    // Only handle IOAPIC #0 for now
    if (acpi::ioapic_count() < 1) return;

    uint32_t version = read_reg(REG_IOAPIC_VERSION);
    uint32_t max_redir = ((version >> 16) & 0xFF);  // highest redirection entry index
    uint32_t entries = max_redir + 1;

    // Mask all redirection entries initially
    for (uint32_t i = 0; i < entries; i++) {
        set_redir_entry(static_cast<uint8_t>(i), 0, 0, true);
    }
}

void set_redir_entry(uint8_t gsi, uint8_t vector, uint8_t apic_id, bool masked) {
    uint32_t low_reg = REG_IOAPIC_RED_TBL + gsi * 2;
    uint32_t high_reg = low_reg + 1;

    uint32_t low = vector;
    if (masked) low |= (1 << 16);  // mask bit

    uint32_t high = static_cast<uint32_t>(apic_id) << 24;

    write_reg(low_reg, low);
    write_reg(high_reg, high);
}

void eoi(uint8_t gsi) {
    (void)gsi;
    // IO APIC EOI not required for edge-triggered; level-triggered needs EOI to IOAPIC
}

uintptr_t get_base() {
    if (acpi::ioapic_count() > 0) {
        return acpi::ioapic_address(0);
    }
    return 0;
}

} // namespace feather::arch::ioapic
