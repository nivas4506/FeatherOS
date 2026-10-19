#include "pit.h"
#include "../../arch/x86_64/include/io.h"
#include "../../task/include/task.h"

namespace feather::drivers {

static volatile uint64_t ticks = 0;

void pit_init(uint32_t hz) {
    if (hz == 0) hz = 100;
    uint32_t divisor = 1193182 / hz;
    if (divisor == 0) divisor = 1;
    if (divisor > 65535) divisor = 65535;

    arch::io::outb(0x43, 0x36);
    arch::io::outb(0x40, static_cast<uint8_t>(divisor & 0xFF));
    arch::io::outb(0x40, static_cast<uint8_t>((divisor >> 8) & 0xFF));
}

uint64_t pit_ticks() {
    return ticks;
}

void pit_handler() {
    ticks++;
    task::scheduler_tick();
}

} // namespace feather::drivers
