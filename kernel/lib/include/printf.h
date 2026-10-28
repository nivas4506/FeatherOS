#pragma once
// FeatherOS Kernel — formatted printing (freestanding printf)


#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstddef>

namespace feather::lib {

void kprintf(const char* fmt, ...);
void kputc(char c);
void kputs(const char* s);

} // namespace feather::lib

