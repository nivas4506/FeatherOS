// FeatherOS Kernel — Per-CPU state implementation

#include "percpu.h"
#include "lapic.h"

namespace feather::arch::percpu {

cpu_state_t g_cpu_states[MAX_CPUS] = {};
int g_cpu_online = 1;  // BSP is online

static cpu_state_t* g_current = &g_cpu_states[0];

cpu_state_t* current() {
    return g_current;
}

void init_bsp() {
    g_cpu_states[0].apic_id   = lapic::get_apic_id();
    g_cpu_states[0].cpu_id    = 0;
    g_cpu_states[0].kernel_stack_top = 0;  // set by boot.asm stack setup
    g_cpu_states[0].current_task = 0;
    g_current = &g_cpu_states[0];
}

void init_ap(uint8_t apic_id, int cpu_id, uint64_t stack_top) {
    g_cpu_states[cpu_id].apic_id      = apic_id;
    g_cpu_states[cpu_id].cpu_id       = cpu_id;
    g_cpu_states[cpu_id].kernel_stack_top = stack_top;
    g_cpu_states[cpu_id].current_task = 0;
    g_cpu_online++;
}

void switch_stack(uint64_t stack_top) {
    // In a real implementation this would load RSP. For now, the trampoline
    // assembly sets RSP before jumping into C++.
    (void)stack_top;
}

} // namespace feather::arch::percpu
