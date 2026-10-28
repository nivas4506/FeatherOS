#pragma once


#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>

namespace feather::arch::gdt {

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
};
#ifdef _MSC_VER
#pragma pack(pop)
#endif

void init();

} // namespace feather::arch::gdt

