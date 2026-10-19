file(REMOVE_RECURSE
  "CMakeFiles/kernel_elf"
  "boot.o"
  "context_switch.o"
  "cpu.o"
  "elf.o"
  "frame_allocator.o"
  "framebuffer.o"
  "gdt.o"
  "gui.o"
  "heap.o"
  "idt.o"
  "interrupt_dispatch.o"
  "interrupts.o"
  "io.o"
  "isr.o"
  "kernel.elf"
  "keyboard.o"
  "main.o"
  "mouse.o"
  "paging.o"
  "panic.o"
  "pit.o"
  "portal.o"
  "printf.o"
  "serial.o"
  "shell.o"
  "string.o"
  "syscall_entry.o"
  "syscalls.o"
  "task.o"
  "vfs.o"
  "vga.o"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/kernel_elf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
