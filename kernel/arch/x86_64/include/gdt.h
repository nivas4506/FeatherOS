#pragma once

#include <cstdint>

namespace feather::arch::gdt {

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void init();

} // namespace feather::arch::gdt
