#pragma once
// FeatherOS Kernel — Local APIC (xAPIC mode, x64)
// Per-core interrupt controller: timer, software IPI, spurious, error handling.

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>

#include "acpi.h"

namespace feather::arch::lapic {

// xAPIC register offsets (relative to LAPIC base)
enum lapic_reg : uint32_t {
    REG_ID               = 0x0020,  // Local APIC ID
    REG_VERSION          = 0x0030,  // LAPIC version
    REG_TPR              = 0x0080,  // Task Priority Register
    REG_APR              = 0x0090,  // Arbitration Priority
    REG_PPR              = 0x00A0,  // Processor Priority
    REG_EOI              = 0x00B0,  // End Of Interrupt
    REG_RRD              = 0x00C0,  // Remote Read
    REG_LDR              = 0x00D0,  // Logical Destination
    REG_DFR              = 0x00E0,  // Destination Format
    REG_SIV              = 0x00F0,  // Spurious Interrupt Vector
    REG_ISR0             = 0x0100,  // In-Service Register (8 x 32-bit)
    REG_TMR0             = 0x0180,  // Trigger Mode Register
    REG_IRR0             = 0x0200,  // Interrupt Request Register
    REG_ESR              = 0x0280,  // Error Status
    REG_ICR_LO           = 0x0300,  // Interrupt Command Register (low)
    REG_ICR_HI           = 0x0310,  // Interrupt Command Register (high)
    REG_LVT_TIMER        = 0x0320,  // Local Vector Table — Timer
    REG_LVT_THERMAL      = 0x0330,  // LVT — Thermal Sensor
    REG_LVT_PERF         = 0x0340,  // LVT — Performance Counter
    REG_LVT_LINT0        = 0x0350,  // LVT — LINT0
    REG_LVT_LINT1        = 0x0360,  // LVT — LINT1
    REG_LVT_ERROR        = 0x0370,  // LVT — Error
    REG_TMR_INITIAL      = 0x0380,  // Timer Initial Count
    REG_TMR_CURRENT      = 0x0390,  // Timer Current Count
    REG_TMR_DIV          = 0x03E0,  // Timer Divide Configuration
};

// Initialize LAPIC for the current processor (BSP or AP)
void init();

// Send EOI (End Of Interrupt)
inline void eoi() {
    volatile uint32_t* reg =
        reinterpret_cast<volatile uint32_t*>(
            acpi::lapic_base_address() + static_cast<uint32_t>(REG_EOI));
    *reg = 0;
}

// Send an IPI to a specific APIC ID
void send_ipi(uint8_t apic_id, uint32_t vector);

// Setup the LAPIC timer (one-shot or periodic) with a given vector
void init_timer(uint8_t vector, bool periodic);

// Get the local APIC ID of the current processor
uint8_t get_apic_id();

// Get the LAPIC base address (from ACPI or default)
uintptr_t get_base();

} // namespace feather::arch::lapic
