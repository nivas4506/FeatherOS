#include "main.h"
#include "panic.h"
#include "syscalls.h"
#include "shell.h"
#include "gdt.h"
#include "idt.h"
#include "interrupts.h"
#include "cpu.h"
#include "vga.h"
#include "keyboard.h"
#include "serial.h"
#include "pit.h"
#include "paging.h"
#include "frame_allocator.h"
#include "heap.h"
#include "vfs.h"
#include "task.h"
#include "printf.h"

namespace feather { namespace kernel {

static void heartbeat(void*) {
    using namespace feather::drivers;
    for (int i = 0; i < 3; i++) {
        serial_puts("[task] heartbeat\n");
        task::task_sleep(50);
    }
    serial_puts("[task] done\n");
    task::task_exit(0);
}

extern "C" void kernel_main() {
    using namespace feather;

    drivers::vga_init();
    drivers::serial_init();

    lib::kputs("FeatherOS kernel booting...\n");

    arch::gdt::init();
    arch::interrupts::init();

    mm::frame_allocator_init();
    mm::paging_init();
    mm::heap_init();

    task::task_init();
    fs::vfs_init();
    syscalls_init();

    drivers::pit_init(100);
    drivers::keyboard_init();
    arch::interrupts::register_irq_handler(0, drivers::pit_handler);
    arch::interrupts::register_irq_handler(1, drivers::keyboard_handler);

    arch::cpu::sti();

    lib::kputs("subsystems: gdt idt pic pit heap vfs sched kbd serial\n");
    drivers::serial_puts("FeatherOS kernel 0.2.0 ready\n");

    task::task_create(heartbeat, nullptr, task::task_priority::LOW);

    shell_run();
    panic("shell returned");
}

} } // namespace feather::kernel
