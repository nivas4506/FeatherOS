#pragma once
// FeatherOS Kernel — USB xHCI Host Controller Driver
// eXtensible Host Controller Interface for USB 3.0 / 2.0 / 1.1 devices.

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstddef>
#include <cstdint>

namespace feather::drivers::usb {

// xHCI capability registers
struct xhci_cap_regs {
    uint32_t caplength_hciversion;
    uint32_t hcs_params1;
    uint32_t hcs_params2;
    uint32_t hcs_params3;
    uint32_t hcc_params1;
    uint32_t db_off;
    uint32_t runtime_reg_space_offset;
};

// xHCI operational registers
struct xhci_op_regs {
    uint32_t usb_cmd;
    uint32_t usb_sts;
    uint32_t page_size;
    uint32_t dn_ctrl;
    uint32_t crcr_low;
    uint32_t crcr_high;
};

// xHCI port register set
struct xhci_port_regs {
    uint32_t portsc;
    uint32_t porpmsc;
    uint32_t pli;
    uint32_t reserved;
};

// Device slot state
enum xhci_slot_state : uint8_t {
    XHCI_SLOT_DISABLED = 0,
    XHCI_SLOT_ENABLED  = 1,
    XHCI_SLOT_ADDRESSED = 2,
    XHCI_SLOT_CONFIGURED = 3,
};

// xHCI device context base address array
struct xhci_dcbaa {
    uint64_t device_context[255];
};

// Initialize xHCI controller
bool xhci_init(uintptr_t mmio_base);

// Shutdown xHCI controller
void xhci_shutdown();

// Reset USB port
bool xhci_port_reset(uint8_t port_num);

// Get port status
uint32_t xhci_port_status(uint8_t port_num);

// Enumerate USB devices
void xhci_enumerate_devices();

// xHCI controller instance
struct xhci_controller {
    volatile xhci_cap_regs* cap;
    volatile xhci_op_regs* op;
    volatile xhci_port_regs* ports;
    uint32_t num_ports;
    uint64_t dcbaa_addr;
    bool     initialized;
};

extern xhci_controller g_xhci;

} // namespace feather::drivers::usb
