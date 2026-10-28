// FeatherOS Kernel — Network Stack Implementation
// Ethernet, ARP, IPv4, ICMP, UDP, and socket API.

#include "net.h"
#include "netstack.h"
#include "../../lib/include/string.h"

namespace feather::drivers::net {
namespace {

// Registered network devices
static net_device* g_devices = nullptr;
static net_device* g_default_dev = nullptr;

// ARP cache: IP -> MAC mapping
static struct arp_entry {
    feather::net::stack::ipv4_addr ip;
    uint8_t   mac[6];
    bool      valid;
} g_arp_cache[16];

} // anonymous namespace

int net_register_device(net_device* dev) {
    if (!dev) return -1;
    dev->next = g_devices;
    g_devices = dev;
    if (!g_default_dev) g_default_dev = dev;
    return 0;
}

net_device* net_find_device(const char* name) {
    for (net_device* d = g_devices; d; d = d->next) {
        if (lib::strcmp(d->name, name) == 0) return d;
    }
    return nullptr;
}

int net_xmit(net_device* dev, const uint8_t* data, uint32_t len) {
    if (!dev || !dev->xmit) return -1;
    int ret = dev->xmit(dev, data, len);
    if (ret == 0) {
        dev->stats.tx_packets++;
        dev->stats.tx_bytes += len;
    } else {
        dev->stats.tx_errors++;
    }
    return ret;
}

void net_rx(net_device* dev, uint8_t* data, uint32_t len) {
    if (!dev) return;
    dev->stats.rx_packets++;
    dev->stats.rx_bytes += len;
    // Forward to network stack
    // net_stack::net_stack_rx(data, len, dev->driver_data);
}

net_device* net_default_device() {
    return g_default_dev;
}

void net_init() {
    // Network subsystem initialized
    g_devices = nullptr;
    g_default_dev = nullptr;
}

} // namespace feather::drivers::net
