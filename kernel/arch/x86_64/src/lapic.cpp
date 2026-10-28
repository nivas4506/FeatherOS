// FeatherOS Kernel — Local APIC implementation
// Configures xAPIC registers, timer, and IPIs.

#include "lapic.h"
#include "acpi.h"

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace feather::arch::lapic {

namespace {

inline volatile uint32_t* lapic_reg_ptr(lapic_reg reg) {
    uintptr_t base = acpi::lapic_base_address();
    return reinterpret_cast<volatile uint32_t*>(
        base + static_cast<uintptr_t>(static_cast<uint32_t>(reg)));
}

} // anonymous namespace

void init() {
    volatile uint32_t* siv = lapic_reg_ptr(REG_SIV);
    volatile uint32_t* lvt_timer = lapic_reg_ptr(REG_LVT_TIMER);
    volatile uint32_t* lvt_lint0 = lapic_reg_ptr(REG_LVT_LINT0);
    volatile uint32_t* lvt_lint1 = lapic_reg_ptr(REG_LVT_LINT1);
    volatile uint32_t* lvt_error = lapic_reg_ptr(REG_LVT_ERROR);
    volatile uint32_t* tpr = lapic_reg_ptr(REG_TPR);
    volatile uint32_t* dfr = lapic_reg_ptr(REG_DFR);
    volatile uint32_t* ldr = lapic_reg_ptr(REG_LDR);

    // Enable LAPIC: set bit 8 of Spurious Interrupt Vector Register
    *siv = 0xFF | 0x100;

    // Set Task Priority to 0 (accept all interrupts)
    *tpr = 0;

    // Flat destination format
    *dfr = 0xFFFFFFFF;

    // Logical destination = APIC ID
    *ldr = (*lapic_reg_ptr(REG_ID) & 0xFF) << 24;

    // Mask LVT timer, LINT0, LINT1, error (unmask when handlers installed)
    *lvt_timer  = 0x10000;  // mask bit
    *lvt_lint0  = 0x10000;
    *lvt_lint1  = 0x10000;
    *lvt_error  = 0x10000;
}

uint8_t get_apic_id() {
    return static_cast<uint8_t>((*lapic_reg_ptr(REG_ID) >> 24) & 0xFF);
}

uintptr_t get_base() {
    return acpi::lapic_base_address();
}

void init_timer(uint8_t vector, bool periodic) {
    volatile uint32_t* lvt_timer = lapic_reg_ptr(REG_LVT_TIMER);
    volatile uint32_t* tmr_div   = lapic_reg_ptr(REG_TMR_DIV);
    volatile uint32_t* tmr_init  = lapic_reg_ptr(REG_TMR_INITIAL);

    // Divide configuration = 1:16 (value 0x3)
    *tmr_div = 0x3;

    // LVT timer: vector | mode (periodic if requested) | mask bit cleared => enabled
    uint32_t mode = periodic ? (1 << 17) : 0;  // bit 17 = timer mode
    *lvt_timer = vector | mode;  // mask bit cleared => enabled

    // Initial count (arbitrary, can be programmed later)
    *tmr_init = 0xFFFFFFFF;
}

} // namespace feather::arch::lapic
