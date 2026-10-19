#include "vga.h"
#include "../../arch/x86_64/include/io.h"

#include <cstddef>

namespace feather::drivers {

static volatile uint16_t* vga_buffer = reinterpret_cast<volatile uint16_t*>(0xB8000);
static uint8_t vga_row = 0;
static uint8_t vga_col = 0;
static uint8_t vga_color = 0x0F;

static uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return static_cast<uint16_t>(uc) | static_cast<uint16_t>(color) << 8;
}

static void update_cursor() {
    uint16_t pos = static_cast<uint16_t>(vga_row * 80 + vga_col);
    arch::io::outb(0x3D4, 0x0F);
    arch::io::outb(0x3D5, static_cast<uint8_t>(pos & 0xFF));
    arch::io::outb(0x3D4, 0x0E);
    arch::io::outb(0x3D5, static_cast<uint8_t>((pos >> 8) & 0xFF));
}

void vga_init() {
    vga_color = 0x0F;
    vga_clear();
}

void vga_clear() {
    for (std::size_t i = 0; i < 80 * 25; i++)
        vga_buffer[i] = vga_entry(' ', vga_color);
    vga_row = 0;
    vga_col = 0;
    update_cursor();
}

void vga_putc(char c) {
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\b') {
        if (vga_col > 0) {
            vga_col--;
            vga_buffer[vga_row * 80 + vga_col] = vga_entry(' ', vga_color);
        }
    } else if (c == '\t') {
        vga_col = static_cast<uint8_t>((vga_col + 4) & ~3);
    } else {
        vga_buffer[vga_row * 80 + vga_col] = vga_entry(static_cast<unsigned char>(c), vga_color);
        vga_col++;
    }

    if (vga_col >= 80) {
        vga_col = 0;
        vga_row++;
    }
    if (vga_row >= 25) {
        for (std::size_t i = 0; i < 24 * 80; i++)
            vga_buffer[i] = vga_buffer[i + 80];
        for (std::size_t i = 24 * 80; i < 25 * 80; i++)
            vga_buffer[i] = vga_entry(' ', vga_color);
        vga_row = 24;
    }
    update_cursor();
}

void vga_write(const char* str) {
    while (*str) vga_putc(*str++);
}

void vga_write_color(const char* str, uint8_t color) {
    uint8_t old = vga_color;
    vga_color = color;
    vga_write(str);
    vga_color = old;
}

void vga_set_cursor(uint8_t x, uint8_t y) {
    vga_col = x;
    vga_row = y;
    update_cursor();
}

} // namespace feather::drivers
