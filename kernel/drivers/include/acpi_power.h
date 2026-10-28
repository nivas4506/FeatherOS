#pragma once
// FeatherOS Kernel — ACPI Power Management
// S3 sleep, S5 shutdown, CPU frequency scaling (P-states).

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstddef>
#include <cstdint>

namespace feather::drivers::power {

// ACPI sleep states
enum sleep_state : uint8_t {
    S_SLEEP_S1 = 1,
    S_SLEEP_S2 = 2,
    S_SLEEP_S3 = 3,
    S_SLEEP_S4 = 4,
    S_SLEEP_S5 = 5,  // Soft off
};

// CPU P-state (performance state)
struct pstate {
    uint32_t frequency_mhz;
    uint32_t power_mw;
    uint32_t latency_us;
};

// Initialize ACPI power management
bool power_init();

// Enter sleep state (S1-S5)
// Returns false if the state is not supported
bool power_sleep(sleep_state state);

// Shutdown the system (S5)
void power_shutdown();

// Reboot the system
void power_reboot();

// Get number of available P-states
uint32_t pstate_count();

// Get P-state by index
const pstate* pstate_get(uint32_t index);

// Set CPU frequency to a P-state
bool pstate_set(uint32_t index);

// Get current CPU frequency in MHz
uint32_t pstate_current_freq();

// Initialize CPU frequency scaling
bool cpufreq_init();

// ACPI FADT (Fixed ACPI Description Table) register block addresses
struct acpi_fadt {
    uint32_t pm1a_evt_blk;
    uint32_t pm1b_evt_blk;
    uint32_t pm1a_ctrl_blk;
    uint32_t pm1b_ctrl_blk;
    uint8_t  pm1_cnt_len;
};

// Get FADT pointers
const acpi_fadt* power_get_fadt();

} // namespace feather::drivers::power
