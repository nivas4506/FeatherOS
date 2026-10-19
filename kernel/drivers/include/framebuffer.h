#pragma once

#include <cstdint>

namespace feather::drivers {

struct framebuffer_info {
    uint8_t* address;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bits_per_pixel;
    uint8_t red_position;
    uint8_t red_mask;
    uint8_t green_position;
    uint8_t green_mask;
    uint8_t blue_position;
    uint8_t blue_mask;
};

bool framebuffer_init(uint32_t multiboot_magic, uint32_t multiboot_information);
bool framebuffer_ready();
const framebuffer_info& framebuffer_get_info();
void framebuffer_clear(uint32_t color);
void framebuffer_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void framebuffer_draw_text(uint32_t x, uint32_t y, const char* text, uint32_t color, uint32_t scale = 2);
void framebuffer_draw_cursor(uint32_t x, uint32_t y);
void framebuffer_draw_desktop();

} // namespace feather::drivers
