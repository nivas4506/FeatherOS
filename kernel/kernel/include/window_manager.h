#pragma once

#include <cstdint>

namespace feather::kernel::gui {

constexpr uint32_t max_windows = 8;

struct window {
    bool used;
    bool visible;
    bool focused;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t title_color;
    const char* title;
};

void window_manager_init(uint32_t screen_width, uint32_t screen_height);
int window_create(const char* title, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t title_color);
void window_close(uint32_t id);
void window_show(uint32_t id);
void window_focus(uint32_t id);
void window_move(uint32_t id, int32_t dx, int32_t dy);
void window_resize(uint32_t id, int32_t dw, int32_t dh);
void window_manager_render();
const window* window_get(uint32_t id);

} // namespace feather::kernel::gui
