#pragma once
// FeatherOS Kernel — ACPI (Advanced Configuration and Power Interface) parsing
// Discovers system topology: LAPIC, IOAPIC, CPU count, and interrupt routing.

#ifdef _MSC_VER
#include <intrin.h>
#pragma pack(push, 1)
#endif
#include <cstdint>
#include <cstddef>

namespace feather::arch::acpi {

// ACPI RSDP (Root System Description Pointer) — searched for in EBDA/BIOS area
struct rsdp_t {
    char     signature[8];   // "RSD PTR "
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_address;   // 32-bit RSDT address (ACPI 1.0)
    uint32_t length;
    uint64_t xsdt_address;   // 64-bit XSDT address (ACPI 2.0+)
    uint8_t  extended_checksum;
    char     reserved[3];
};

// ACPI SDT (System Description Table) header
struct sdt_header_t {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
};

// MADT (Multiple APIC Description Table)
struct madt_t {
    sdt_header_t header;
    uint32_t     lapic_address;
    uint32_t     flags;
};

// MADT entry types
enum madt_type_t {
    MADT_TYPE_LOCAL_APIC    = 0,
    MADT_TYPE_IO_APIC       = 1,
    MADT_TYPE_INTERRUPT_OVR = 2,
    MADT_TYPE_NMI           = 4,
    MADT_TYPE_LAPIC_NMI     = 5,
    MADT_TYPE_LOCAL_X2APIC  = 9,
};

// MADT entry header
struct madt_entry_header_t {
    uint8_t type;
    uint8_t length;
};

// Processor Local APIC entry
struct madt_lapic_t {
    madt_entry_header_t header;
    uint8_t             processor_id;
    uint8_t             lapic_id;
    uint32_t            flags;
};

// IO APIC entry
struct madt_ioapic_t {
    madt_entry_header_t header;
    uint8_t             ioapic_id;
    uint8_t             reserved;
    uint32_t            ioapic_address;
    uint32_t            global_system_interrupt_base;
};

// Interrupt override entry
struct madt_int_override_t {
    madt_entry_header_t header;
    uint8_t             bus;
    uint8_t             source;
    uint32_t            global_system_interrupt;
    uint16_t            flags;
};

// IO APIC redirection table entry
struct ioapic_redir_t {
    uint64_t raw;
};

#ifdef _MSC_VER
#pragma pack(pop)
#endif

// Discover ACPI tables from multiboot info or memory scan
void init(uint32_t multiboot_magic, uint32_t multiboot_information);

// Number of CPUs detected via MADT
int cpu_count();

// Local APIC base address
uintptr_t lapic_base_address();

// IO APIC count and addresses
int ioapic_count();
uintptr_t ioapic_address(int index);

// Global system interrupt base for IOAPIC #0
uint32_t ioapic_gsi_base();

} // namespace feather::arch::acpi
