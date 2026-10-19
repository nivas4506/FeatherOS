#include "gui.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "mouse.h"
#include "window_manager.h"
#include "../../arch/x86_64/include/cpu.h"
#include "../../drivers/include/pit.h"

namespace feather::kernel {

void gui_run() {
    using namespace feather::drivers;
    const framebuffer_info& screen = framebuffer_get_info();
    gui::window_manager_init(screen.width, screen.height);
    const int terminal_id = gui::window_create("TERMINAL", 300, 280, screen.width - 600, 240, 0x1D3944);
    gui::window_close(static_cast<uint32_t>(terminal_id));
    framebuffer_draw_desktop();
    mouse_state previous = mouse_get_state();
    bool terminal_open = false;
    framebuffer_draw_cursor(static_cast<uint32_t>(previous.x), static_cast<uint32_t>(previous.y));

    for (;;) {
        bool redraw = false;
        if (keyboard_has_char()) {
            const char key = keyboard_try_getc();
            if (key == 'r' || key == 'R') redraw = true;
            if (key == 't' || key == 'T') {
                terminal_open = !terminal_open;
                if (terminal_open) gui::window_show(static_cast<uint32_t>(terminal_id));
                else gui::window_close(static_cast<uint32_t>(terminal_id));
                redraw = true;
            }
            if (key != 't' && key != 'T') redraw = true;
        }

        const mouse_state current = mouse_get_state();
        if (current.x != previous.x || current.y != previous.y || current.left != previous.left) {
            redraw = true;
        }
        if (redraw) {
            framebuffer_draw_desktop();
            if (terminal_open) {
                gui::window_manager_render();
                framebuffer_fill_rect(320, 320, screen.width - 640, 180, 0x0B1418);
                framebuffer_draw_text(320, 340, "FEATHEROS NATIVE SHELL", 0xDCEFF2, 2);
                framebuffer_draw_text(320, 380, "PRESS T TO CLOSE", 0x9DB5C2, 1);
            }
            if (current.left) {
                framebuffer_fill_rect(static_cast<uint32_t>(current.x > 80 ? current.x - 80 : 0), static_cast<uint32_t>(current.y > 18 ? current.y - 18 : 0), 160, 36, 0x275D5A);
                framebuffer_draw_text(static_cast<uint32_t>(current.x > 70 ? current.x - 70 : 0), static_cast<uint32_t>(current.y > 10 ? current.y - 10 : 0), "CLICK", 0xFFFFFF, 1);
            }
            framebuffer_draw_cursor(static_cast<uint32_t>(current.x), static_cast<uint32_t>(current.y));
        }
        previous = current;
        arch::cpu::hlt();
    }
}

} // namespace feather::kernel
