#pragma once
// FeatherOS Kernel — Per-CPU data
// Each CPU has its own stack, GDT/IDT pointers, and scheduler state.

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>
#include <cstddef>

namespace feather::arch::percpu {

// Per-CPU state. One instance per logical processor.
struct cpu_state_t {
    uint8_t   apic_id;           // Local APIC ID of this core
    uint8_t   cpu_id;            // Logical CPU index (0 = BSP)
    uint64_t  kernel_stack_top;  // Top of this core's kernel stack
    uintptr_t gdt_ptr;           // GDT descriptor pointer (for this core)
    uintptr_t idt_ptr;           // IDT descriptor pointer (for this core)
    uintptr_t current_task;      // Currently running task on this core
    uint64_t  tsc_freq;          // TSC frequency (if calibrated)
};

// Maximum number of CPUs supported
constexpr int MAX_CPUS = 64;

// Array of per-CPU states (indexed by cpu_id)
extern cpu_state_t g_cpu_states[MAX_CPUS];

// Number of CPUs currently online
extern int g_cpu_online;

// Get the current CPU's state (reads from GS or via a global pointer)
// For BSP, we use a global pointer; for APs, set during AP init.
cpu_state_t* current();

// Initialize per-CPU state for the BSP (CPU 0)
void init_bsp();

// Initialize per-CPU state for an AP (called from trampoline)
void init_ap(uint8_t apic_id, int cpu_id, uint64_t stack_top);

// Switch to this CPU's kernel stack
void switch_stack(uint64_t stack_top);

} // namespace feather::arch::percpu
