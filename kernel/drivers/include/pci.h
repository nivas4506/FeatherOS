#pragma once
// FeatherOS Kernel — PCIe bus enumeration
// Accesses PCI configuration space via IO ports 0xCF8 / 0xCFC.

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>

namespace feather::drivers::pci {

// PCI configuration space registers
enum pci_reg : uint8_t {
    REG_VENDOR_ID      = 0x00,
    REG_DEVICE_ID      = 0x02,
    REG_COMMAND        = 0x04,
    REG_STATUS         = 0x06,
    REG_REVISION_ID    = 0x08,
    REG_PROG_IF        = 0x09,
    REG_SUBCLASS       = 0x0A,
    REG_CLASS_CODE     = 0x0B,
    REG_CACHE_LINE     = 0x0C,
    REG_LATENCY_TIMER  = 0x0D,
    REG_HEADER_TYPE    = 0x0E,
    REG_BIST           = 0x0F,
    REG_BAR0           = 0x10,
    REG_BAR1           = 0x14,
    REG_BAR2           = 0x18,
    REG_BAR3           = 0x1C,
    REG_BAR4           = 0x20,
    REG_BAR5           = 0x24,
    REG_INTERRUPT_LINE = 0x3C,
};

// PCI class codes
enum pci_class : uint8_t {
    CLASS_UNCLASSIFIED  = 0x00,
    CLASS_STORAGE       = 0x01,
    CLASS_NETWORK       = 0x02,
    CLASS_DISPLAY       = 0x03,
    CLASS_MULTIMEDIA    = 0x04,
    CLASS_MEMORY        = 0x05,
    CLASS_BRIDGE        = 0x06,
    CLASS_COMMUNICATION = 0x07,
    CLASS_SYSTEM        = 0x08,
    CLASS_INPUT         = 0x09,
    CLASS_DOCKING       = 0x0A,
    CLASS_PROCESSOR     = 0x0B,
    CLASS_SERIAL_BUS    = 0x0C,
    CLASS_WIRELESS      = 0x0D,
    CLASS_INTELLIGENT   = 0x0E,
};

// PCI subclass codes for storage (0x01)
enum pci_subclass_storage : uint8_t {
    SUBCLASS_SCSI       = 0x00,
    SUBCLASS_IDE        = 0x01,
    SUBCLASS_FLOPPY     = 0x02,
    SUBCLASS_IPI        = 0x03,
    SUBCLASS_RAID       = 0x04,
    SUBCLASS_NVME       = 0x08,   // NVM Express
    SUBCLASS_OTHER      = 0x80,
};

// PCI programming interface for NVMe (0x08)
enum pci_prog_if_nvme : uint8_t {
    PROG_IF_NVM         = 0x02,   // NVM Express
};

// Header type flags
enum header_type_flags : uint8_t {
    HEADER_TYPE_MASK    = 0x7F,
    MULTI_FUNCTION      = 0x80,
};

// A discovered PCI device
struct pci_device {
    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  revision_id;
    uint8_t  header_type;
    uint32_t bar[6];
    uint8_t  interrupt_line;
};

// Maximum devices found
constexpr int MAX_DEVICES = 256;

// Read a 32-bit value from PCI config space
uint32_t config_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg);

// Write a 32-bit value to PCI config space
void config_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg, uint32_t value);

// Read a 16-bit value from PCI config space
uint16_t config_read_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg);

// Read an 8-bit value from PCI config space
uint8_t config_read_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg);

// Enumerate all PCI buses/devices/functions. Fills the provided array.
// Returns the number of devices found.
int enumerate(pci_device* out, int max);

// Find the first NVMe controller
pci_device* find_nvme();

// Get the number of devices found by the last enumerate() call
int device_count();

} // namespace feather::drivers::pci
