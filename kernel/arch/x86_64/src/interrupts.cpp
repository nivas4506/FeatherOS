#include "interrupts.h"
#include "idt.h"
#include "io.h"

namespace feather::arch::interrupts {

static void (*irq_handlers[16])() = {};

static void pic_remap() {
    using io::outb;
    using io::io_wait;

    outb(0x20, 0x11); io_wait();
    outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait();
    outb(0xA1, 0x28); io_wait();
    outb(0x21, 0x04); io_wait();
    outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait();
    outb(0xA1, 0x01); io_wait();

    // IRQ0 timer + IRQ1 keyboard unmasked
    outb(0x21, 0xFC);
    outb(0xA1, 0xFF);
}

void init() {
    pic_remap();
    idt::init();
}

void register_irq_handler(uint8_t irq, void (*handler)()) {
    if (irq < 16) irq_handlers[irq] = handler;
}

void eoi(uint8_t irq) {
    if (irq >= 8) io::outb(0xA0, 0x20);
    io::outb(0x20, 0x20);
}

void dispatch_irq(uint8_t irq) {
    if (irq < 16 && irq_handlers[irq]) {
        irq_handlers[irq]();
    }
    eoi(irq);
}

} // namespace feather::arch::interrupts
