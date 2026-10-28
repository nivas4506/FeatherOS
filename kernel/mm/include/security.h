#pragma once
// FeatherOS Kernel — Memory Security Protections
// SMEP/SMAP, W^X (Write XOR Execute), and KASLR support.

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstddef>
#include <cstdint>

namespace feather::mm::security {

// CPU feature flags
enum cpu_features : uint32_t {
    FEATURE_SMEP    = 0x00000001,  // Supervisor Mode Execution Prevention
    FEATURE_SMAP    = 0x00000002,  // Supervisor Mode Access Prevention
    FEATURE_SMEP_MAP= 0x00000004,  // SMEP and MAP (combined)
};

// Check if a CPU feature is supported
bool has_feature(uint32_t feature);

// Enable SMEP (set CR4.SMEP bit 20)
void enable_smep();

// Enable SMAP (set CR4.SMAP bit 21)
void enable_smap();

// Disable SMEP
void disable_smep();

// Disable SMAP
void disable_smap();

// W^X (Write XOR Execute) enforcement
// Mark a page range as W^X compliant (either writable OR executable, not both)
void enforce_wx(uint64_t start, uint64_t end, bool writable, bool executable);

// Check if a page mapping violates W^X
bool wx_violation(uint64_t addr, bool writable, bool executable);

// KASLR (Kernel Address Space Layout Randomization)
// Get the kernel base offset (randomized at boot)
uint64_t kaslr_offset();

// Apply KASLR relocation to a symbol address
uint64_t kaslr_relocate(uint64_t symbol_addr);

// Initialize security features (detect CPU, enable SMEP/SMAP, apply KASLR)
void security_init();

} // namespace feather::mm::security
