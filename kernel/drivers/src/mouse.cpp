#include "mouse.h"
#include "../../arch/x86_64/include/io.h"

namespace feather::drivers {

namespace {
constexpr uint16_t data_port = 0x60;
constexpr uint16_t command_port = 0x64;
mouse_state state{};
uint8_t packet[3]{};
uint8_t packet_index = 0;
uint32_t width = 1024;
uint32_t height = 768;

bool can_write() {
    return (arch::io::inb(command_port) & 2) == 0;
}

bool can_read() {
    return (arch::io::inb(command_port) & 1) != 0;
}

void wait_write() {
    for (uint32_t i = 0; i < 100000 && !can_write(); ++i) arch::io::io_wait();
}

void wait_read() {
    for (uint32_t i = 0; i < 100000 && !can_read(); ++i) arch::io::io_wait();
}

void write_command(uint8_t command) {
    wait_write();
    arch::io::outb(command_port, command);
}

void write_data(uint8_t data) {
    wait_write();
    arch::io::outb(data_port, data);
}

uint8_t read_data() {
    wait_read();
    return arch::io::inb(data_port);
}

} // namespace

void mouse_init(uint32_t screen_width, uint32_t screen_height) {
    width = screen_width == 0 ? 1024 : screen_width;
    height = screen_height == 0 ? 768 : screen_height;
    state = { static_cast<int32_t>(width / 2), static_cast<int32_t>(height / 2), false, false, false };
    packet_index = 0;

    write_command(0xA8);
    write_command(0x20);
    const uint8_t status = read_data();
    write_command(0x60);
    write_data(static_cast<uint8_t>(status | 2));
    write_command(0xD4);
    write_data(0xF6);
    read_data();
    write_command(0xD4);
    write_data(0xF4);
    read_data();
}

void mouse_handler() {
    const uint8_t value = arch::io::inb(data_port);
    if (packet_index == 0 && (value & 0x08) == 0) return;
    packet[packet_index++] = value;
    if (packet_index < 3) return;
    packet_index = 0;

    state.x += static_cast<int8_t>(packet[1]);
    state.y -= static_cast<int8_t>(packet[2]);
    if (state.x < 0) state.x = 0;
    if (state.y < 0) state.y = 0;
    if (state.x >= static_cast<int32_t>(width)) state.x = static_cast<int32_t>(width - 1);
    if (state.y >= static_cast<int32_t>(height)) state.y = static_cast<int32_t>(height - 1);
    state.left = (packet[0] & 1) != 0;
    state.right = (packet[0] & 2) != 0;
    state.middle = (packet[0] & 4) != 0;
}

mouse_state mouse_get_state() {
    return state;
}

} // namespace feather::drivers
