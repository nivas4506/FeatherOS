// FeatherOS Kernel — ACPI table parsing
// Scans memory for the RSDP signature, validates checksums, and parses MADT
// to discover LAPIC/IOAPIC addresses and CPU count.

#include "acpi.h"
#include "io.h"

#include <cstdint>

namespace feather::arch::acpi {

namespace {

// Simple byte comparison (avoids <cstring> MSVC header issues)
static bool mem_equal(const void* a, const void* b, size_t n) {
    const uint8_t* pa = static_cast<const uint8_t*>(a);
    const uint8_t* pb = static_cast<const uint8_t*>(b);
    for (size_t i = 0; i < n; i++) {
        if (pa[i] != pb[i]) return false;
    }
    return true;
}

// Search for RSDP signature in a memory range
const uint8_t* find_rsdp_in_range(uintptr_t start, uintptr_t end) {
    const char* sig = "RSD PTR ";
    for (uintptr_t addr = start; addr <= end - 8; addr += 16) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(addr);
        if (mem_equal(p, sig, 8)) {
            // Validate checksum (first 20 bytes for ACPI 1.0)
            uint8_t sum = 0;
            for (int i = 0; i < 20; i++) sum += p[i];
            if (sum == 0) return p;
        }
    }
    return nullptr;
}

uint8_t sum_bytes(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) sum += data[i];
    return sum;
}

} // anonymous namespace

static const rsdp_t* g_rsdp = nullptr;
static const madt_t* g_madt = nullptr;
static int g_cpu_count = 1;
static uintptr_t g_lapic_base = 0xFEE00000; // default fallback
static int g_ioapic_count = 0;
static uintptr_t g_ioapic_addresses[8] = {0};
static uint32_t g_ioapic_gsi_base = 0;

void init(uint32_t multiboot_magic, uint32_t multiboot_information) {
    (void)multiboot_magic;
    (void)multiboot_information;

    // 1. Search EBDA (Extended BIOS Data Area)
    uintptr_t ebda_ptr = *reinterpret_cast<uint16_t*>(0x40E);
    uintptr_t ebda = ebda_ptr << 4;
    if (ebda) {
        g_rsdp = reinterpret_cast<const rsdp_t*>(find_rsdp_in_range(ebda, ebda + 1024));
    }

    // 2. Search BIOS ROM area (0xE0000 - 0xFFFFF)
    if (!g_rsdp) {
        g_rsdp = reinterpret_cast<const rsdp_t*>(find_rsdp_in_range(0xE0000, 0xFFFFF));
    }

    if (!g_rsdp) {
        // No ACPI found — use defaults
        g_lapic_base = 0xFEE00000;
        g_cpu_count = 1;
        return;
    }

    // Use XSDT if available (ACPI 2.0+), otherwise RSDT
    const uint8_t* xsdt_ptr = nullptr;
    if (g_rsdp->revision >= 2 && g_rsdp->xsdt_address) {
        xsdt_ptr = reinterpret_cast<const uint8_t*>(g_rsdp->xsdt_address);
    } else if (g_rsdp->rsdt_address) {
        xsdt_ptr = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(g_rsdp->rsdt_address));
    }

    if (!xsdt_ptr) {
        g_lapic_base = 0xFEE00000;
        g_cpu_count = 1;
        return;
    }

    const sdt_header_t* sdt_hdr = reinterpret_cast<const sdt_header_t*>(xsdt_ptr);
    if (sum_bytes(reinterpret_cast<const uint8_t*>(sdt_hdr), sdt_hdr->length) != 0) {
        // Invalid checksum
        g_lapic_base = 0xFEE00000;
        g_cpu_count = 1;
        return;
    }

    // Determine entry size: XSDT uses 8-byte pointers, RSDT uses 4-byte
    bool is_xsdt = (g_rsdp->revision >= 2 && g_rsdp->xsdt_address);
    size_t entry_size = is_xsdt ? 8 : 4;
    size_t entries = (sdt_hdr->length - sizeof(sdt_header_t)) / entry_size;

    // 3. Scan for MADT (signature "APIC")
    for (size_t i = 0; i < entries; i++) {
        uintptr_t table_addr;
        if (is_xsdt) {
            table_addr = *reinterpret_cast<const uint64_t*>(xsdt_ptr + sizeof(sdt_header_t) + i * 8);
        } else {
            table_addr = *reinterpret_cast<const uint32_t*>(xsdt_ptr + sizeof(sdt_header_t) + i * 4);
            table_addr = static_cast<uintptr_t>(table_addr);
        }

        const sdt_header_t* hdr = reinterpret_cast<const sdt_header_t*>(table_addr);
        if (mem_equal(hdr->signature, "APIC", 4)) {
            g_madt = reinterpret_cast<const madt_t*>(hdr);
            break;
        }
    }

    if (!g_madt) {
        g_lapic_base = 0xFEE00000;
        g_cpu_count = 1;
        return;
    }

    // Parse MADT entries
    g_lapic_base = g_madt->lapic_address;

    const uint8_t* entry_ptr =
        reinterpret_cast<const uint8_t*>(g_madt) + sizeof(madt_t);
    const uint8_t* end =
        reinterpret_cast<const uint8_t*>(g_madt) + g_madt->header.length;

    while (entry_ptr + sizeof(madt_entry_header_t) <= end) {
        const madt_entry_header_t* entry_hdr =
            reinterpret_cast<const madt_entry_header_t*>(entry_ptr);
        uint8_t type = entry_hdr->type;
        uint8_t len = entry_hdr->length;

        if (len < sizeof(madt_entry_header_t)) break;

        switch (type) {
        case MADT_TYPE_LOCAL_APIC: {
            const madt_lapic_t* lapic_entry =
                reinterpret_cast<const madt_lapic_t*>(entry_ptr);
            // processor_id == 0xFF means this LAPIC entry is unusable
            if (lapic_entry->processor_id != 0xFF) {
                g_cpu_count++;
            }
            break;
        }
        case MADT_TYPE_IO_APIC: {
            const madt_ioapic_t* ioapic_entry =
                reinterpret_cast<const madt_ioapic_t*>(entry_ptr);
            if (g_ioapic_count < 8) {
                g_ioapic_addresses[g_ioapic_count] = ioapic_entry->ioapic_address;
                g_ioapic_gsi_base =
                    ioapic_entry->global_system_interrupt_base;
                g_ioapic_count++;
            }
            break;
        }
        case MADT_TYPE_INTERRUPT_OVR:
            // Could store IRQ override mappings here
            break;
        default:
            break;
        }

        entry_ptr += len;
    }

    // g_cpu_count started at 0 for BSP; we incremented for each usable LAPIC entry
    // The BSP itself counts as one CPU
    if (g_cpu_count == 0) g_cpu_count = 1;
}

int cpu_count() {
    return g_cpu_count;
}

uintptr_t lapic_base_address() {
    return g_lapic_base;
}

int ioapic_count() {
    return g_ioapic_count;
}

uintptr_t ioapic_address(int index) {
    if (index >= 0 && index < g_ioapic_count) {
        return g_ioapic_addresses[index];
    }
    return 0;
}

uint32_t ioapic_gsi_base() {
    return g_ioapic_gsi_base;
}

} // namespace feather::arch::acpi
