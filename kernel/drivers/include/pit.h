#pragma once

#include <cstdint>

namespace feather::drivers {

void pit_init(uint32_t hz = 100);
uint64_t pit_ticks();
void pit_handler();

} // namespace feather::drivers
