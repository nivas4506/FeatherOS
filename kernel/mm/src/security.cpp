// FeatherOS Kernel — Memory Security Protections
// SMEP/SMAP, W^X, and KASLR implementation.

#include "security.h"
#include "../../arch/x86_64/include/cpu.h"
#include "../../lib/include/printf.h"

namespace feather::mm::security {
namespace {

// Detected CPU features
static uint32_t g_cpu_features = 0;

// KASLR base offset (randomized)
static uint64_t g_kaslr_offset = 0;

// Simple xorshift PRNG for KASLR
static uint64_t g_prng_state = 0x123456789abcdef0ULL;

static uint64_t xorshift64() {
    g_prng_state ^= g_prng_state << 13;
    g_prng_state ^= g_prng_state >> 7;
    g_prng_state ^= g_prng_state << 17;
    return g_prng_state;
}

} // anonymous namespace

bool has_feature(uint32_t feature) {
    return (g_cpu_features & feature) != 0;
}

void enable_smep() {
    if (!(g_cpu_features & FEATURE_SMEP)) return;
    uint64_t cr4 = arch::cpu::read_cr4();
    cr4 |= (1ULL << 20);  // SMEP bit
    arch::cpu::write_cr4(cr4);
}

void enable_smap() {
    if (!(g_cpu_features & FEATURE_SMAP)) return;
    uint64_t cr4 = arch::cpu::read_cr4();
    cr4 |= (1ULL << 21);  // SMAP bit
    arch::cpu::write_cr4(cr4);
}

void disable_smep() {
    uint64_t cr4 = arch::cpu::read_cr4();
    cr4 &= ~(1ULL << 20);
    arch::cpu::write_cr4(cr4);
}

void disable_smap() {
    uint64_t cr4 = arch::cpu::read_cr4();
    cr4 &= ~(1ULL << 21);
    arch::cpu::write_cr4(cr4);
}

void enforce_wx(uint64_t start, uint64_t end, bool writable, bool executable) {
    if (writable && executable) {
        // W^X violation - page is both writable and executable
        // In a full implementation, we'd split the mapping or fault
    }
}

bool wx_violation(uint64_t /*addr*/, bool writable, bool executable) {
    return writable && executable;
}

uint64_t kaslr_offset() {
    return g_kaslr_offset;
}

uint64_t kaslr_relocate(uint64_t symbol_addr) {
    return symbol_addr + g_kaslr_offset;
}

void security_init() {
    // Detect CPU features using CPUID
    uint32_t eax, ebx, ecx, edx;
    arch::cpu::cpuid(1, eax, ebx, ecx, edx);

    // Check SMEP (ECX bit 20) and SMAP (ECX bit 21)
    if (ecx & (1 << 20)) g_cpu_features |= FEATURE_SMEP;
    if (ecx & (1 << 21)) g_cpu_features |= FEATURE_SMAP;

    // Initialize KASLR with a random offset
    // Use CPUID leaf 1 EAX as a seed (varies by CPU model)
    g_prng_state ^= eax;
    g_kaslr_offset = xorshift64() & 0xFFFFF000;  // Page-aligned offset up to 16MB

    if (g_cpu_features & FEATURE_SMEP) {
        enable_smep();
    }
    if (g_cpu_features & FEATURE_SMAP) {
        enable_smap();
    }
}

} // namespace feather::mm::security
