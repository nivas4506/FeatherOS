// FeatherOS Kernel — SMP boot
// Discovers processors via ACPI, initializes LAPIC/IOAPIC, and provides
// the framework for booting Application Processors (APs) via SIPI.

#include "smp.h"
#include "acpi.h"
#include "lapic.h"
#include "ioapic.h"
#include "percpu.h"
#include "cpu.h"

namespace feather::arch::smp {

// Flag set by each AP when it reaches 64-bit mode (indexed by cpu_id)
volatile int g_ap_ready[percpu::MAX_CPUS] = {0};

void init() {
    int cpu_count = acpi::cpu_count();
    if (cpu_count <= 1) return;  // Uniprocessor, nothing to do

    // Initialize BSP LAPIC
    lapic::init();

    // Initialize IOAPIC (redirects device interrupts)
    ioapic::init();

    // APs are detected via ACPI MADT (cpu_count > 1). Full AP boot via
    // SIPI requires the trampoline to be copied to a low-memory physical
    // address (<1MB) and identity-mapped. That integration with the
    // physical page allocator is the next integration step.
    (void)cpu_count;
}

int online_cpus() {
    int count = 1;  // BSP is always online
    for (int i = 1; i < percpu::MAX_CPUS; i++) {
        if (g_ap_ready[i]) count++;
    }
    return count;
}

void send_ipi(uint8_t apic_id, uint32_t vector) {
    lapic::send_ipi(apic_id, vector);
}

void broadcast_ipi(uint32_t vector, uint8_t self_apic_id) {
    (void)self_apic_id;
    volatile uint32_t* icr_hi =
        reinterpret_cast<volatile uint32_t*>(acpi::lapic_base_address() + 0x310);
    volatile uint32_t* icr_lo =
        reinterpret_cast<volatile uint32_t*>(acpi::lapic_base_address() + 0x300);

    while (*icr_lo & (1 << 12)) {}

    *icr_hi = 0;  // all excluding self
    *icr_lo = vector | (0 << 8) | (1 << 14) | (1 << 11);  // fixed, assert, level
}

// Called by each AP after trampoline switches to 64-bit mode
extern "C" void ap_entrypoint(uint8_t apic_id, int cpu_id) {
    percpu::init_ap(apic_id, cpu_id, 0);
    lapic::init();
    g_ap_ready[cpu_id] = 1;

    // AP idle loop — wait for scheduler work
    while (true) {
        cpu::pause();
    }
}

} // namespace feather::arch::smp
