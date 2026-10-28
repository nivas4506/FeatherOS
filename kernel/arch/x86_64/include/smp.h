#pragma once
// FeatherOS Kernel — Symmetric Multi-Processing (SMP)
// Boots Application Processors (APs) via SIPI and manages per-CPU initialization.

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>

namespace feather::arch::smp {

// Initialize SMP: parse ACPI, enumerate CPUs, boot all APs
void init();

// Called by the AP trampoline after switching to 64-bit mode
// apic_id: Local APIC ID of this processor
// cpu_id: logical CPU index
extern "C" void ap_entrypoint(uint8_t apic_id, int cpu_id);

// Number of CPUs that have completed initialization
int online_cpus();

// Send an IPI to a specific APIC ID
void send_ipi(uint8_t apic_id, uint32_t vector);

// Broadcast an IPI to all other processors (excluding self)
void broadcast_ipi(uint32_t vector, uint8_t self_apic_id);

} // namespace feather::arch::smp
