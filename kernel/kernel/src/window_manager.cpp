#include "window_manager.h"
#include "framebuffer.h"

namespace feather::kernel::gui {

namespace {
window windows[max_windows]{};
uint32_t screen_width = 1024;
uint32_t screen_height = 768;
uint32_t focused_id = 0;

uint32_t clamp_x(int32_t value, uint32_t width) {
    const int32_t max_x = static_cast<int32_t>(screen_width > width ? screen_width - width : 0);
    return static_cast<uint32_t>(value < 0 ? 0 : value > max_x ? max_x : value);
}

uint32_t clamp_y(int32_t value, uint32_t height) {
    const int32_t max_y = static_cast<int32_t>(screen_height > height + 44 ? screen_height - height - 44 : 0);
    return static_cast<uint32_t>(value < 44 ? 44 : value > max_y ? max_y : value);
}

} // namespace

void window_manager_init(uint32_t width, uint32_t height) {
    screen_width = width == 0 ? 1024 : width;
    screen_height = height == 0 ? 768 : height;
    for (auto& item : windows) item = {};
    focused_id = 0;
}

int window_create(const char* title, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t title_color) {
    for (uint32_t id = 0; id < max_windows; ++id) {
        if (windows[id].used) continue;
        windows[id] = { true, true, true, clamp_x(static_cast<int32_t>(x), width), clamp_y(static_cast<int32_t>(y), height), width, height, title_color, title };
        window_focus(id);
        return static_cast<int>(id);
    }
    return -1;
}

void window_close(uint32_t id) {
    if (id >= max_windows) return;
    windows[id].visible = false;
    if (focused_id == id) focused_id = 0;
}

void window_show(uint32_t id) {
    if (id >= max_windows || !windows[id].used) return;
    windows[id].visible = true;
    window_focus(id);
}

void window_focus(uint32_t id) {
    if (id >= max_windows || !windows[id].used) return;
    for (auto& item : windows) item.focused = false;
    windows[id].focused = true;
    focused_id = id;
}

void window_move(uint32_t id, int32_t dx, int32_t dy) {
    if (id >= max_windows || !windows[id].used) return;
    windows[id].x = clamp_x(static_cast<int32_t>(windows[id].x) + dx, windows[id].width);
    windows[id].y = clamp_y(static_cast<int32_t>(windows[id].y) + dy, windows[id].height);
    window_focus(id);
}

void window_resize(uint32_t id, int32_t dw, int32_t dh) {
    if (id >= max_windows || !windows[id].used) return;
    const uint32_t width = static_cast<uint32_t>(static_cast<int32_t>(windows[id].width) + dw < 220 ? 220 : static_cast<int32_t>(windows[id].width) + dw);
    const uint32_t height = static_cast<uint32_t>(static_cast<int32_t>(windows[id].height) + dh < 120 ? 120 : static_cast<int32_t>(windows[id].height) + dh);
    windows[id].width = width;
    windows[id].height = height;
    windows[id].x = clamp_x(static_cast<int32_t>(windows[id].x), width);
    windows[id].y = clamp_y(static_cast<int32_t>(windows[id].y), height);
    window_focus(id);
}

void window_manager_render() {
    for (uint32_t id = 0; id < max_windows; ++id) {
        const window& item = windows[id];
        if (!item.used || !item.visible) continue;
        const uint32_t body = item.focused ? 0x152B38 : 0x12212D;
        feather::drivers::framebuffer_fill_rect(item.x, item.y, item.width, item.height, body);
        feather::drivers::framebuffer_fill_rect(item.x, item.y, item.width, 28, item.focused ? item.title_color : 0x34434D);
        feather::drivers::framebuffer_draw_text(item.x + 10, item.y + 9, item.title, 0xFFFFFF, 1);
        feather::drivers::framebuffer_fill_rect(item.x + item.width - 54, item.y + 10, 8, 2, 0xB5C5CA);
        feather::drivers::framebuffer_fill_rect(item.x + item.width - 34, item.y + 7, 8, 8, 0xB5C5CA);
        feather::drivers::framebuffer_fill_rect(item.x + item.width - 14, item.y + 7, 8, 8, 0xE68A91);
    }
}

const window* window_get(uint32_t id) {
    return id < max_windows && windows[id].used ? &windows[id] : nullptr;
}

} // namespace feather::kernel::gui
