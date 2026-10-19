#pragma once
// FeatherOS Kernel — Panic

#include <cstddef>

namespace feather::kernel {

[[noreturn]] void panic(const char* msg);

} // namespace feather::kernel
