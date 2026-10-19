#include "keyboard.h"
#include "../../arch/x86_64/include/io.h"
#include "../../arch/x86_64/include/cpu.h"
#include "../../task/include/task.h"

namespace feather::drivers {

static constexpr uint32_t BUF_SIZE = 128;
static char buf[BUF_SIZE];
static volatile uint32_t head = 0;
static volatile uint32_t tail = 0;
static bool shift = false;
static bool extended = false;

static const char keymap[58] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    0, 0, ' '
};

static const char keymap_shift[58] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    0, 0, ' '
};

static void push_char(char c) {
    uint32_t next = (head + 1) % BUF_SIZE;
    if (next == tail) return; // drop on overflow
    buf[head] = c;
    head = next;
}

void keyboard_init() {
    tail = 0;
    head = 0;
    shift = false;
    extended = false;
}

void keyboard_handler() {
    uint8_t sc = arch::io::inb(0x60);

    if (sc == 0xE0) {
        extended = true;
        return;
    }

    bool release = (sc & 0x80) != 0;
    uint8_t code = sc & 0x7F;

    if (extended) {
        extended = false;
        return;
    }

    if (code == 0x2A || code == 0x36) {
        shift = !release;
        return;
    }

    if (release) return;
    if (code >= 58) return;

    char c = shift ? keymap_shift[code] : keymap[code];
    if (c) push_char(c);
}

bool keyboard_has_char() {
    return head != tail;
}

char keyboard_try_getc() {
    if (head == tail) return 0;
    char c = buf[tail];
    tail = (tail + 1) % BUF_SIZE;
    return c;
}

char keyboard_getc() {
    while (!keyboard_has_char()) {
        task::task_yield();
        arch::cpu::hlt();
    }
    return keyboard_try_getc();
}

} // namespace feather::drivers
