// FeatherOS Kernel — raw port I/O implementation
#include "io.h"
#include "../../arch/x86_64/include/io.h"

namespace feather::lib {

void outb(unsigned short port, unsigned char val) {
    feather::arch::io::outb(port, val);
}

unsigned char inb(unsigned short port) {
    return feather::arch::io::inb(port);
}

} // namespace feather::lib
