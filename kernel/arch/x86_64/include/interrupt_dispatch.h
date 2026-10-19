#pragma once

#include <cstdint>

extern "C" void interrupt_dispatch(uint64_t vector, uint64_t error_code);
