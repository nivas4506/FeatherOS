#include "interrupt_dispatch.h"
#include "interrupts.h"
#include "cpu.h"
#include "../../kernel/include/panic.h"
#include "../../lib/include/printf.h"

extern "C" void interrupt_dispatch(uint64_t vector, uint64_t error_code) {
    using namespace feather;

    if (vector == 0x80) {
        // Syscall ABI: rax=num, rdi,rsi,rdx,r10,r8,r9 — we only have the vector here.
        // The C syscall wrapper is invoked from software int with args already in regs
        // is not forwarded; shell uses native kernel APIs. Keep a safe no-op.
        (void)error_code;
        return;
    }

    if (vector >= 32 && vector <= 47) {
        arch::interrupts::dispatch_irq(static_cast<uint8_t>(vector - 32));
        return;
    }

    uint64_t cr2 = 0;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));

    const char* name = "unknown";
    switch (vector) {
        case 0:  name = "#DE divide error"; break;
        case 3:  name = "#BP breakpoint"; break;
        case 6:  name = "#UD invalid opcode"; break;
        case 8:  name = "#DF double fault"; break;
        case 13: name = "#GP general protection"; break;
        case 14: name = "#PF page fault"; break;
        default: break;
    }

    lib::kprintf("\nCPU exception %s vec=%u err=0x%x cr2=0x%p\n",
                 name,
                 static_cast<unsigned>(vector),
                 static_cast<unsigned>(error_code),
                 reinterpret_cast<void*>(cr2));

    if (vector == 8 || vector == 13 || vector == 14) {
        kernel::panic("fatal CPU exception");
    }
}
