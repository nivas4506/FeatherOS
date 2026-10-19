#pragma once
// FeatherOS Kernel — raw port I/O helpers (freestanding)

namespace feather::lib {

void outb(unsigned short port, unsigned char val);
unsigned char inb(unsigned short port);

} // namespace feather::lib
