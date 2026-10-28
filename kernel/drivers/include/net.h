#pragma once
// FeatherOS Kernel — Network Interface Abstraction
// Generic network device interface that multiple drivers (VirtIO-Net, e1000, etc.)
// can implement. Provides packet TX/RX and MAC address management.

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstddef>
#include <cstdint>

namespace feather::drivers::net {

// Ethernet frame types
enum ethertype : uint16_t {
    ETH_IPV4   = 0x0800,
    ETH_ARP    = 0x0806,
    ETH_IPV6   = 0x86DD,
};

// Ethernet header (14 bytes)
#pragma pack(push, 1)
struct eth_header {
    uint8_t  dst[6];
    uint8_t  src[6];
    uint16_t type;
};
#pragma pack(pop)

// Network device statistics
struct net_stats {
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_errors;
    uint64_t tx_errors;
    uint64_t dropped;
};

// Network device interface
struct net_device {
    char     name[16];          // "eth0", "eth1", etc.
    uint8_t  mac[6];            // MAC address
    uint16_t mtu;               // Maximum transmission unit
    ethertype default_type;     // Default ethertype

    // Driver callbacks
    int  (*start)(net_device* dev);
    void (*stop)(net_device* dev);
    int  (*xmit)(net_device* dev, const uint8_t* data, uint32_t len);
    void (*set_multicast_list)(net_device* dev);
    int  (*set_mac)(net_device* dev, const uint8_t* mac);

    net_stats stats;
    void*     driver_data;      // Driver-specific state
    net_device* next;           // Linked list of devices
};

// Register a network device
int net_register_device(net_device* dev);

// Find a device by name
net_device* net_find_device(const char* name);

// Transmit a packet on a device
int net_xmit(net_device* dev, const uint8_t* data, uint32_t len);

// Receive a packet (called by driver interrupt handler)
void net_rx(net_device* dev, uint8_t* data, uint32_t len);

// Initialize the network subsystem
void net_init();

// Default network device (for socket operations)
net_device* net_default_device();

} // namespace feather::drivers::net
