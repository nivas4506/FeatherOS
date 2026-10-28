#pragma once


#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>

namespace feather::drivers {

void keyboard_init();
void keyboard_handler();
bool keyboard_has_char();
char keyboard_getc();          // blocking
char keyboard_try_getc();      // 0 if empty

} // namespace feather::drivers

