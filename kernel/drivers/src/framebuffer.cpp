#include "framebuffer.h"

#include <cstddef>

namespace feather::drivers {

namespace {

constexpr uint32_t multiboot2_magic = 0x36D76289;
constexpr uint32_t tag_end = 0;
constexpr uint32_t tag_framebuffer = 8;

struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot_framebuffer_tag {
    uint32_t type;
    uint32_t size;
    uint64_t address;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bits_per_pixel;
    uint8_t framebuffer_type;
    uint16_t reserved;
    uint8_t red_position;
    uint8_t red_mask;
    uint8_t green_position;
    uint8_t green_mask;
    uint8_t blue_position;
    uint8_t blue_mask;
};

framebuffer_info info{};
bool ready = false;

const uint8_t* glyph(char value) {
    static const uint8_t blank[7] = {};
    static const uint8_t letters[26][7] = {
        {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
        {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
        {14,17,16,23,17,17,14},{17,17,17,31,17,17,17},{31,4,4,4,4,4,31},
        {1,1,1,1,17,17,14},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
        {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
        {17,17,17,17,17,10,4},{17,17,17,21,21,27,17},{17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
    };
    static const uint8_t digits[10][7] = {
        {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
        {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
        {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14}
    };
    if (value >= 'A' && value <= 'Z') return letters[value - 'A'];
    if (value >= 'a' && value <= 'z') return letters[value - 'a'];
    if (value >= '0' && value <= '9') return digits[value - '0'];
    return blank;
}

uint32_t pixel(uint32_t color) {
    if (info.bits_per_pixel == 32) return color;
    return color & 0x00FFFFFFu;
}

} // namespace

bool framebuffer_init(uint32_t multiboot_magic, uint32_t multiboot_information) {
    ready = false;
    if (multiboot_magic != multiboot2_magic || multiboot_information == 0) return false;

    const auto* total_size = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(multiboot_information));
    const auto* tag = reinterpret_cast<const multiboot_tag*>(total_size + 2);
    const auto* end = reinterpret_cast<const uint8_t*>(total_size) + *total_size;

    while (reinterpret_cast<const uint8_t*>(tag) < end && tag->type != tag_end) {
        if (tag->type == tag_framebuffer && tag->size >= sizeof(multiboot_framebuffer_tag)) {
            const auto* framebuffer = reinterpret_cast<const multiboot_framebuffer_tag*>(tag);
            if (framebuffer->framebuffer_type != 1 || framebuffer->address == 0 || framebuffer->bits_per_pixel < 24) return false;
            info.address = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(framebuffer->address));
            info.pitch = framebuffer->pitch;
            info.width = framebuffer->width;
            info.height = framebuffer->height;
            info.bits_per_pixel = framebuffer->bits_per_pixel;
            info.red_position = framebuffer->red_position;
            info.red_mask = framebuffer->red_mask;
            info.green_position = framebuffer->green_position;
            info.green_mask = framebuffer->green_mask;
            info.blue_position = framebuffer->blue_position;
            info.blue_mask = framebuffer->blue_mask;
            ready = true;
            return true;
        }
        tag = reinterpret_cast<const multiboot_tag*>(
            (reinterpret_cast<uintptr_t>(tag) + tag->size + 7u) & ~static_cast<uintptr_t>(7u));
    }
    return false;
}

bool framebuffer_ready() {
    return ready;
}

const framebuffer_info& framebuffer_get_info() {
    return info;
}

void framebuffer_clear(uint32_t color) {
    if (!ready) return;
    framebuffer_fill_rect(0, 0, info.width, info.height, color);
}

void framebuffer_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    if (!ready || x >= info.width || y >= info.height) return;
    const uint32_t right = x + width > info.width ? info.width : x + width;
    const uint32_t bottom = y + height > info.height ? info.height : y + height;
    const uint32_t bytes_per_pixel = info.bits_per_pixel / 8;
    const uint32_t packed = pixel(color);

    for (uint32_t row = y; row < bottom; ++row) {
        auto* destination = reinterpret_cast<uint32_t*>(info.address + row * info.pitch + x * bytes_per_pixel);
        for (uint32_t column = x; column < right; ++column) destination[column - x] = packed;
    }
}

void framebuffer_draw_text(uint32_t x, uint32_t y, const char* text, uint32_t color, uint32_t scale) {
    if (!ready || !text || scale == 0) return;
    for (const char* cursor = text; *cursor; ++cursor) {
        if (*cursor == '\n') {
            y += 8 * scale;
            x = 0;
            continue;
        }
        const uint8_t* bitmap = glyph(*cursor);
        for (uint32_t row = 0; row < 7; ++row) {
            for (uint32_t column = 0; column < 5; ++column) {
                if ((bitmap[row] & (1u << (4u - column))) != 0)
                    framebuffer_fill_rect(x + column * scale, y + row * scale, scale, scale, color);
            }
        }
        x += 6 * scale;
    }
}

void framebuffer_draw_cursor(uint32_t x, uint32_t y) {
    framebuffer_fill_rect(x, y, 3, 22, 0xFFFFFF);
    framebuffer_fill_rect(x, y, 13, 3, 0xFFFFFF);
    framebuffer_fill_rect(x + 3, y + 3, 3, 13, 0xFFFFFF);
    framebuffer_fill_rect(x + 6, y + 13, 4, 7, 0xFFFFFF);
    framebuffer_fill_rect(x + 10, y + 19, 4, 4, 0xFFFFFF);
}

void framebuffer_draw_desktop() {
    if (!ready) return;
    framebuffer_clear(0x101C2B);
    framebuffer_fill_rect(0, 0, info.width, 42, 0x152334);
    framebuffer_fill_rect(0, info.height - 74, info.width, 74, 0x0D1724);
    framebuffer_fill_rect(0, 42, 5, info.height - 116, 0x55C7B2);
    framebuffer_fill_rect(28, 82, 180, 126, 0x1C3142);
    framebuffer_fill_rect(40, 94, 156, 4, 0x79E0CA);
    framebuffer_fill_rect(40, 112, 100, 4, 0x718B9C);
    framebuffer_fill_rect(40, 130, 130, 4, 0x718B9C);
    framebuffer_fill_rect(info.width - 268, 82, 240, 170, 0x172B3A);
    framebuffer_fill_rect(info.width - 248, 103, 180, 5, 0x79E0CA);
    framebuffer_fill_rect(info.width - 248, 128, 150, 5, 0x718B9C);
    framebuffer_fill_rect(info.width - 248, 153, 190, 5, 0x718B9C);
    framebuffer_fill_rect(info.width - 248, 178, 120, 5, 0x718B9C);
    framebuffer_draw_text(28, 14, "FEATHEROS", 0xA9F1E1, 2);
    framebuffer_draw_text(50, 104, "WELCOME", 0xDCEFF2, 2);
    framebuffer_draw_text(50, 146, "NATIVE DESKTOP", 0x9DB5C2, 1);
    framebuffer_draw_text(info.width - 248, 80, "SYSTEM", 0xA9F1E1, 2);
    framebuffer_draw_text(42, info.height - 52, "APPLICATIONS", 0x9DB5C2, 1);
}

} // namespace feather::drivers
