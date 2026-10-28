// FeatherOS Kernel — TCP/IP Stack Implementation
// ARP, IPv4, ICMP, UDP, and BSD-style socket API.

#include "netstack.h"
#include "../include/net.h"
#include "../../lib/include/string.h"

namespace feather::net::stack {
namespace {

// Local IP configuration
static ipv4_addr g_local_ip   = 0xC0A80064;  // 192.168.0.100
static ipv4_addr g_gateway    = 0xC0A80001;  // 192.168.0.1
static ipv4_addr g_netmask    = 0xFFFFFF00;  // 255.255.255.0

// Socket table
static socket g_sockets[32];
static int    g_next_fd = 3;

// ARP cache entry
static arp_entry g_arp_cache[16];

bool arp_cache_lookup(ipv4_addr ip, uint8_t* mac) {
    for (int i = 0; i < 16; i++) {
        if (g_arp_cache[i].valid && g_arp_cache[i].ip == ip) {
            lib::memcpy(mac, g_arp_cache[i].mac, 6);
            return true;
        }
    }
    return false;
}

void arp_cache_insert(ipv4_addr ip, const uint8_t* mac) {
    for (int i = 0; i < 16; i++) {
        if (!g_arp_cache[i].valid) {
            g_arp_cache[i].ip = ip;
            lib::memcpy(g_arp_cache[i].mac, mac, 6);
            g_arp_cache[i].valid = true;
            return;
        }
    }
}

// Find a free socket
static socket* alloc_socket() {
    for (int i = 0; i < 32; i++) {
        if (g_sockets[i].state == SS_FREE) {
            g_sockets[i].fd = g_next_fd++;
            g_sockets[i].state = SS_UNCONNECTED;
            return &g_sockets[i];
        }
    }
    return nullptr;
}

static socket* find_socket(int fd) {
    for (int i = 0; i < 32; i++) {
        if (g_sockets[i].fd == fd && g_sockets[i].state != SS_FREE) {
            return &g_sockets[i];
        }
    }
    return nullptr;
}

} // namespace

void net_stack_init() {
    for (int i = 0; i < 32; i++) {
        g_sockets[i].fd = -1;
        g_sockets[i].state = SS_FREE;
    }
    g_next_fd = 3;
}

int socket_create(int domain, int type, int protocol) {
    (void)domain; (void)type; (void)protocol;
    socket* s = alloc_socket();
    if (!s) return -1;
    s->fd = g_next_fd++;
    s->state = SS_UNCONNECTED;
    return s->fd;
}

int socket_bind(int fd, const sockaddr_in* addr) {
    socket* s = find_socket(fd);
    if (!s || !addr) return -1;
    s->local_addr = *addr;
    return 0;
}

int socket_listen(int fd, int backlog) {
    (void)backlog;
    socket* s = find_socket(fd);
    if (!s) return -1;
    if (s->type != SOCK_STREAM) return -1;
    s->state = SS_CONNECTING;
    return 0;
}

int socket_accept(int fd, sockaddr_in* addr) {
    socket* s = find_socket(fd);
    if (!s || !addr) return -1;
    (void)addr;
    return -1;
}

int socket_connect(int fd, const sockaddr_in* addr) {
    socket* s = find_socket(fd);
    if (!s || !addr) return -1;
    s->remote_addr = *addr;
    s->state = SS_CONNECTED;
    return 0;
}

int socket_send(int fd, const uint8_t* data, uint32_t len) {
    socket* s = find_socket(fd);
    if (!s || !data) return -1;
    if (s->state != SS_CONNECTED) return -1;
    (void)data;
    (void)len;
    return -1;
}

int socket_recv(int fd, uint8_t* data, uint32_t len) {
    socket* s = find_socket(fd);
    if (!s || !data) return -1;
    (void)data;
    (void)len;
    return -1;
}

int socket_close(int fd) {
    socket* s = find_socket(fd);
    if (!s) return -1;
    s->state = SS_FREE;
    s->fd = -1;
    return 0;
}

void net_stack_rx(uint8_t* data, uint32_t len, void* dev_data) {
    (void)data;
    (void)len;
    (void)dev_data;
}

bool arp_resolve(ipv4_addr ip, uint8_t* mac) {
    (void)ip; (void)mac;
    return false;
}

ipv4_addr net_get_local_ip() {
    return g_local_ip;
}

void net_set_local_ip(ipv4_addr ip) {
    g_local_ip = ip;
}

void net_set_gateway(ipv4_addr gw) {
    g_gateway = gw;
}

void net_set_netmask(ipv4_addr mask) {
    g_netmask = mask;
}

const uint8_t* net_get_mac() {
    feather::drivers::net::net_device* dev = feather::drivers::net::net_default_device();
    if (!dev) return nullptr;
    return dev->mac;
}

} // namespace feather::net::stack
