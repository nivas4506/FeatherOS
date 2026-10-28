#pragma once
// FeatherOS Kernel — Serial Port Driver (16550 UART)


#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>
#include <cstddef>

namespace feather::drivers {

void serial_init(uint16_t port = 0x3F8);  // COM1
void serial_putc(char c);
void serial_puts(const char* str);
void serial_write(const char* data, std::size_t len);
bool serial_tx_ready(uint16_t port);
char serial_getc(uint16_t port);
bool serial_rx_ready(uint16_t port);

} // namespace feather::drivers
