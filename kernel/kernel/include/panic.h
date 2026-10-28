#pragma once
// FeatherOS Kernel — Panic


#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstddef>

namespace feather::kernel {

[[noreturn]] void panic(const char* msg);

} // namespace feather::kernel

