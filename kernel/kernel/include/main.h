#pragma once


#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>
// FeatherOS Kernel — Main entry

namespace feather::kernel {

extern "C" void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_information);

} // namespace feather::kernel

