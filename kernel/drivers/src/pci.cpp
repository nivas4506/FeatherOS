// FeatherOS Kernel — PCIe enumeration
// Accesses PCI configuration space via IO ports 0xCF8 / 0xCFC.

#include "pci.h"
#include "../../arch/x86_64/include/io.h"

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace feather::drivers::pci {

namespace {

using feather::arch::io::outb;
using feather::arch::io::inb;
using feather::arch::io::outl;
using feather::arch::io::inl;

// Static storage for discovered devices
static pci_device g_devices[MAX_DEVICES];
static int g_device_count = 0;

// Build the PCI configuration address
inline uint32_t make_addr(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg) {
    return (1u << 31)               // enable bit
         | (static_cast<uint32_t>(bus) << 16)
         | (static_cast<uint32_t>(device) << 11)
         | (static_cast<uint32_t>(function) << 8)
         | (reg & 0xFC);
}

} // anonymous namespace

uint32_t config_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg) {
    outl(0xCF8, make_addr(bus, device, function, reg));
    return inl(0xCFC);
}

void config_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg, uint32_t value) {
    outl(0xCF8, make_addr(bus, device, function, reg));
    outl(0xCFC, value);
}

uint16_t config_read_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg) {
    outl(0xCF8, make_addr(bus, device, function, reg));
    return inb(0xCFC + (reg & 2));
}

uint8_t config_read_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg) {
    outl(0xCF8, make_addr(bus, device, function, reg));
    return inb(0xCFC + (reg & 3));
}

int enumerate(pci_device* out, int max) {
    g_device_count = 0;

    for (uint8_t bus = 0; bus < 255; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            // Read vendor ID to check if device exists
            uint16_t vendor = config_read_word(bus, device, 0, REG_VENDOR_ID);
            if (vendor == 0xFFFF || vendor == 0x0000) continue;

            // Read header type to check multi-function
            uint8_t header_type = config_read_byte(bus, device, 0, REG_HEADER_TYPE);
            bool multi_func = (header_type & MULTI_FUNCTION) != 0;

            uint8_t max_func = multi_func ? 7 : 0;

            for (uint8_t function = 0; function <= max_func; function++) {
                uint16_t vid = config_read_word(bus, device, function, REG_VENDOR_ID);
                if (vid == 0xFFFF || vid == 0x0000) continue;

                pci_device dev{};
                dev.bus       = bus;
                dev.device    = device;
                dev.function  = function;
                dev.vendor_id = vid;
                dev.device_id = config_read_word(bus, device, function, REG_DEVICE_ID);
                dev.revision_id = config_read_byte(bus, device, function, REG_REVISION_ID);
                dev.prog_if     = config_read_byte(bus, device, function, REG_PROG_IF);
                dev.subclass    = config_read_byte(bus, device, function, REG_SUBCLASS);
                dev.class_code  = config_read_byte(bus, device, function, REG_CLASS_CODE);
                dev.header_type = config_read_byte(bus, device, function, REG_HEADER_TYPE);

                // Read BARs (header type 0 has 6 BARs)
                if ((dev.header_type & HEADER_TYPE_MASK) == 0) {
                    for (int i = 0; i < 6; i++) {
                        dev.bar[i] = config_read_dword(bus, device, function,
                            static_cast<uint8_t>(REG_BAR0 + i * 4));
                    }
                }

                dev.interrupt_line = config_read_byte(bus, device, function, REG_INTERRUPT_LINE);

                if (out && g_device_count < max) {
                    out[g_device_count] = dev;
                }
                g_device_count++;
            }
        }
    }

    return g_device_count;
}

int device_count() {
    return g_device_count;
}

pci_device* find_nvme() {
    for (int i = 0; i < g_device_count; i++) {
        pci_device& d = g_devices[i];
        if (d.class_code == CLASS_STORAGE
            && d.subclass == SUBCLASS_NVME
            && d.prog_if == PROG_IF_NVM) {
            return &d;
        }
    }
    return nullptr;
}

} // namespace feather::drivers::pci
