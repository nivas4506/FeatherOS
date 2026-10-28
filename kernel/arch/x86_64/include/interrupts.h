#pragma once


#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>

namespace feather::arch::interrupts {

void init();
void register_irq_handler(uint8_t irq, void (*handler)());
void dispatch_irq(uint8_t irq);
void eoi(uint8_t irq);

} // namespace feather::arch::interrupts

