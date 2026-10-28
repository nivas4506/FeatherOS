// FeatherOS Kernel — USB xHCI Host Controller Driver
// eXtensible Host Controller Interface implementation (stub).

#include "xhci.h"

namespace feather::drivers::usb {

xhci_controller g_xhci = {};

bool xhci_init(uintptr_t mmio_base) {
    g_xhci.cap = reinterpret_cast<volatile xhci_cap_regs*>(mmio_base);
    g_xhci.op = reinterpret_cast<volatile xhci_op_regs*>(mmio_base + 0x100); // Offset varies
    g_xhci.ports = nullptr;
    g_xhci.num_ports = 0;
    g_xhci.dcbaa_addr = 0;
    g_xhci.initialized = false;

    // Read number of ports from HCSPARAMS1
    if (g_xhci.cap) {
        uint32_t hcs_params1 = g_xhci.cap->hcs_params1;
        g_xhci.num_ports = (hcs_params1 >> 0) & 0xFF;  // Max device slots
    }

    // Stop the controller (run/stop bit = 0)
    if (g_xhci.op) {
        g_xhci.op->usb_cmd &= ~0x1;
        // Wait for controller to stop (HCHalted bit)
        for (volatile int i = 0; i < 100000; i++) {}
        if (g_xhci.op->usb_sts & 0x1) {
            g_xhci.initialized = true;
        }
    }

    return g_xhci.initialized;
}

void xhci_shutdown() {
    if (g_xhci.initialized && g_xhci.op) {
        g_xhci.op->usb_cmd &= ~0x1;
    }
    g_xhci.initialized = false;
}

bool xhci_port_reset(uint8_t port_num) {
    if (!g_xhci.initialized || !g_xhci.ports) return false;
    if (port_num >= g_xhci.num_ports) return false;

    volatile uint32_t* portsc = &g_xhci.ports[port_num].portsc;
    *portsc |= (1 << 9);  // Port Reset bit

    // Wait for reset to complete
    for (volatile int i = 0; i < 1000000; i++) {
        if (!(*portsc & (1 << 9))) break;
    }

    return true;
}

uint32_t xhci_port_status(uint8_t port_num) {
    if (!g_xhci.initialized || !g_xhci.ports) return 0;
    if (port_num >= g_xhci.num_ports) return 0;
    return g_xhci.ports[port_num].portsc;
}

void xhci_enumerate_devices() {
    if (!g_xhci.initialized) return;

    // Enumerate all ports
    for (uint32_t i = 0; i < g_xhci.num_ports; i++) {
        uint32_t status = xhci_port_status(static_cast<uint8_t>(i));
        bool connected = (status & 0x1) != 0;  // Current Connect Status
        bool enabled   = (status & 0x2) != 0;  // Port Enabled

        if (connected && !enabled) {
            xhci_port_reset(static_cast<uint8_t>(i));
        }
    }
}

} // namespace feather::drivers::usb
