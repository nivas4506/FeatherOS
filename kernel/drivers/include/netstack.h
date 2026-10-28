// FeatherOS Kernel — TCP/IP Network Stack
// Ethernet, ARP, IPv4, ICMP, UDP implementation with socket API.

#pragma once
#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstddef>
#include <cstdint>

namespace feather::net::stack {

// IPv4 address
using ipv4_addr = uint32_t;

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

// ARP cache entry
struct arp_entry {
    ipv4_addr ip;
    uint8_t   mac[6];
    bool      valid;
};

// Socket address structure (sockaddr_in compatible)
struct sockaddr_in {
    uint16_t sin_family;    // AF_INET = 2
    uint16_t sin_port;      // Port in network byte order
    ipv4_addr  sin_addr;    // IP address in network byte order
    char     sin_zero[8];   // Padding
};

// Socket types
enum sock_type : int {
    SOCK_STREAM = 1,  // TCP
    SOCK_DGRAM  = 2,  // UDP
};

// Socket state
enum sock_state : int {
    SS_FREE     = 0,
    SS_UNCONNECTED = 1,
    SS_CONNECTING = 2,
    SS_CONNECTED  = 3,
    SS_DISCONNECTING = 4,
};

// Socket structure
struct socket {
    int          fd;            // File descriptor
    sock_type    type;          // SOCK_STREAM or SOCK_DGRAM
    sock_state   state;         // Connection state
    sockaddr_in  local_addr;    // Local address/port
    sockaddr_in  remote_addr;   // Remote address/port
    void*        driver_data;   // Driver-specific state
    socket*      next;          // Linked list of sockets
};

// Socket domain
enum sock_domain : int {
    AF_INET = 2,
};

// Initialize network layer
void net_stack_init();

// Socket API
int  socket_create(int domain, int type, int protocol);
int  socket_bind(int fd, const sockaddr_in* addr);
int  socket_listen(int fd, int backlog);
int  socket_accept(int fd, sockaddr_in* addr);
int  socket_connect(int fd, const sockaddr_in* addr);
int  socket_send(int fd, const uint8_t* data, uint32_t len);
int  socket_recv(int fd, uint8_t* data, uint32_t len);
int  socket_close(int fd);

// Process incoming network packets (called by net_rx)
void net_stack_rx(uint8_t* data, uint32_t len, void* dev_data);

// ARP: resolve IP to MAC
bool arp_resolve(ipv4_addr ip, uint8_t* mac);

// IPv4: send a packet
int ipv4_send(ipv4_addr dst, uint8_t protocol, const uint8_t* data, uint32_t len);

// UDP: send a datagram
int udp_send(ipv4_addr dst_ip, uint16_t dst_port, ipv4_addr src_ip, uint16_t src_port,
             const uint8_t* data, uint32_t len);

// Get local IP address
ipv4_addr net_get_local_ip();

// Set local IP address (for static config)
void net_set_local_ip(ipv4_addr ip);

// Set gateway
void net_set_gateway(ipv4_addr gw);

// Set netmask
void net_set_netmask(ipv4_addr mask);

// Get MAC address of default device
const uint8_t* net_get_mac();

} // namespace feather::net::stack
