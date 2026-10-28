#ifdef _MSC_VER
#include <intrin.h>
#include <string.h>
#endif
#include "main.h"
#include "panic.h"
#include "syscalls.h"
#include "shell.h"
#include "gdt.h"
#include "idt.h"
#include "interrupts.h"
#include "cpu.h"
#include "vga.h"
#include "framebuffer.h"
#include "gui.h"
#include "mouse.h"
#include "keyboard.h"
#include "serial.h"
#include "pit.h"
#include "paging.h"
#include "frame_allocator.h"
#include "heap.h"
#include "vfs.h"
#include "task.h"
#include "printf.h"
#include "acpi.h"
#include "lapic.h"
#include "ioapic.h"
#include "smp.h"
#include "percpu.h"
#include "pci.h"
#include "nvme.h"
#include "ext2.h"
#include "fat32.h"
#include "net.h"
#include "netstack.h"
#include "xhci.h"
#include "hda.h"
#include "acpi_power.h"
#include "slab.h"
#include "vma.h"
#include "security.h"
#include "ebpf.h"
#include "telemetry.h"
#include "zerocopy.h"
#include "scheduler_hints.h"
#include "sandbox.h"

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

extern "C" void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_information) {
    using namespace feather;

    drivers::vga_init();
    drivers::serial_init();
    const bool has_framebuffer = drivers::framebuffer_init(multiboot_magic, multiboot_information);
    if (has_framebuffer) drivers::framebuffer_draw_desktop();

    lib::kputs("FeatherOS kernel booting...\n");

    arch::gdt::init();
    arch::interrupts::init();

    // Phase 1: ACPI, LAPIC, IOAPIC, SMP
    arch::acpi::init(multiboot_magic, multiboot_information);
    arch::percpu::init_bsp();
    arch::lapic::init();
    arch::ioapic::init();
    arch::smp::init();

    // Phase 2: PCIe enumeration, NVMe, filesystems
    feather::drivers::pci::pci_device devices[16];
    int dev_count = feather::drivers::pci::enumerate(devices, 16);
    lib::kprintf("PCI: found %d devices\n", dev_count);

    feather::drivers::pci::pci_device* nvme_dev =
        feather::drivers::pci::find_nvme();
    if (nvme_dev) {
        lib::kprintf("NVMe: found at bus=%u dev=%u func=%u, BAR0=0x%x\n",
            nvme_dev->bus, nvme_dev->device, nvme_dev->function, nvme_dev->bar[0]);
        if (feather::drivers::nvme::init(nvme_dev->bar[0] & ~0xF)) {
            lib::kprintf("NVMe: initialized, %u namespace(s), %llu blocks, %u bytes/block\n",
                feather::drivers::nvme::namespace_count(),
                feather::drivers::nvme::namespace_size(1),
                feather::drivers::nvme::namespace_block_size(1));

            // Try mounting ext2
            if (feather::fs::ext2::mount(feather::drivers::nvme::block_read)) {
                lib::kputs("ext2: mounted\n");
            }
            // Try mounting FAT32
            if (feather::fs::fat32::mount(feather::drivers::nvme::block_read)) {
                lib::kputs("fat32: mounted\n");
            }
        } else {
            lib::kputs("NVMe: init failed\n");
        }
    } else {
        lib::kputs("NVMe: not found\n");
    }

    // Phase 4: Networking
    feather::drivers::net::net_init();
    feather::net::stack::net_stack_init();
    lib::kputs("net: initialized\n");

    // Phase 5: Memory management & security
    feather::mm::slab::slab_init();
    feather::mm::vma::vma_init();
    feather::mm::security::security_init();
    lib::kputs("mm: slab/vma/security initialized\n");

    // Phase 6: Device drivers & power
    feather::drivers::power::power_init();
    feather::drivers::power::cpufreq_init();
    lib::kputs("power: initialized\n");

    // Phase 7: AI-native OS substrate
    feather::kernel::telemetry::telemetry_init();
    feather::kernel::sched_hints::sched_hints_init();
    feather::kernel::sandbox::sandbox_init();
    lib::kputs("phase7: telemetry/sched_hints/sandbox initialized\n");

    mm::frame_allocator_init();
    mm::paging_init();
    mm::heap_init();

    task::task_init();
    fs::vfs_init();
    syscalls_init();

    drivers::pit_init(100);
    drivers::keyboard_init();
    drivers::mouse_init(has_framebuffer ? drivers::framebuffer_get_info().width : 1024, has_framebuffer ? drivers::framebuffer_get_info().height : 768);
    arch::interrupts::register_irq_handler(0, drivers::pit_handler);
    arch::interrupts::register_irq_handler(1, drivers::keyboard_handler);
    arch::interrupts::register_irq_handler(12, drivers::mouse_handler);

    arch::cpu::sti();

    lib::kputs(has_framebuffer
        ? "subsystems: gdt idt apic ioapic smp pci nvme net mm slab vma security ebpf telemetry zerocopy sched_hints sandbox usb hda power pit heap vfs sched kbd serial framebuffer gui acpi ext2 fat32\n"
        : "subsystems: gdt idt apic ioapic smp pci nvme net mm slab vma security ebpf telemetry zerocopy sched_hints sandbox usb hda power pit heap vfs sched kbd serial (text mode) acpi ext2 fat32\n");
    drivers::serial_puts("FeatherOS kernel 0.2.0 ready\n");

    task::task_create(heartbeat, nullptr, task::task_priority::LOW);

    if (has_framebuffer) gui_run();
    shell_run();
    panic("shell returned");
}

} } // namespace feather::kernel
