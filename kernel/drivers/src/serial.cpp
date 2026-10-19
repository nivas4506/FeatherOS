#include "serial.h"
#include "../../arch/x86_64/include/io.h"
#include "../../arch/x86_64/include/cpu.h"

#include <cstddef>

namespace feather::drivers {

static uint16_t serial_port = 0x3F8;

void serial_init(uint16_t port) {
    serial_port = port;
    arch::io::outb(port + 1, 0x00);
    arch::io::outb(port + 3, 0x80);
    arch::io::outb(port + 0, 0x03);
    arch::io::outb(port + 1, 0x00);
    arch::io::outb(port + 3, 0x03);
    arch::io::outb(port + 2, 0xC7);
    arch::io::outb(port + 1, 0x00);
    arch::io::outb(port + 4, 0x0B);
}

bool serial_tx_ready(uint16_t port) {
    return (arch::io::inb(port + 5) & 0x20) != 0;
}

bool serial_rx_ready(uint16_t port) {
    return (arch::io::inb(port + 5) & 0x01) != 0;
}

void serial_putc(char c) {
    unsigned spins = 0;
    while (!serial_tx_ready(serial_port) && spins++ < 100000) {
        arch::cpu::pause();
    }
    arch::io::outb(serial_port, static_cast<uint8_t>(c));
}

char serial_getc(uint16_t port) {
    while (!serial_rx_ready(port)) {
        arch::cpu::pause();
    }
    return static_cast<char>(arch::io::inb(port));
}

void serial_puts(const char* str) {
    while (*str) {
        if (*str == '\n') serial_putc('\r');
        serial_putc(*str++);
    }
}

void serial_write(const char* data, std::size_t len) {
    for (std::size_t i = 0; i < len; i++) {
        serial_putc(data[i]);
    }
}

} // namespace feather::drivers
