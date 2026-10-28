#pragma once
// FeatherOS Kernel — VGA text mode driver


#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>

namespace feather::drivers {

void vga_init();
void vga_putc(char c);
void vga_clear();
void vga_write(const char* str);
void vga_write_color(const char* str, uint8_t color);
void vga_set_cursor(uint8_t x, uint8_t y);

} // namespace feather::drivers

