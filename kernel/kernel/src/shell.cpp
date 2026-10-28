#include "shell.h"
#include "panic.h"
#include "../../lib/include/printf.h"
#include "../../lib/include/string.h"
#include "../../drivers/include/keyboard.h"
#include "../../drivers/include/vga.h"
#include "../../drivers/include/serial.h"
#include "../../drivers/include/pit.h"
#include "../../mm/include/heap.h"
#include "../../mm/include/frame_allocator.h"
#include "../../fs/include/vfs.h"
#include "../../task/include/task.h"
#include "../../arch/x86_64/include/io.h"
#include "../../arch/x86_64/include/cpu.h"
#include "../../kernel/include/elf.h"

namespace feather::kernel {

static void print_banner() {
    lib::kputs("\n");
    lib::kputs("  FeatherOS kernel 0.2.0  (x86_64, freestanding C++20)\n");
    lib::kputs("  Type 'help' for commands.\n\n");
}

static void cmd_help() {
    lib::kputs("Commands:\n");
    lib::kputs("  help       this list\n");
    lib::kputs("  uname      kernel identity\n");
    lib::kputs("  clear      clear VGA console\n");
    lib::kputs("  mem        heap + frame allocator stats\n");
    lib::kputs("  ps         scheduler stats\n");
    lib::kputs("  ticks      PIT tick count\n");
    lib::kputs("  ls [path]  list ramfs directory\n");
    lib::kputs("  cat <path> print a ramfs file\n");
    lib::kputs("  echo ...   print arguments\n");
    lib::kputs("  reboot     reset via keyboard controller\n");
lib::kputs("  uspspawn   spawn a userspace task from ramfs ELF\n");
}

static void cmd_uname() {
    lib::kputs("FeatherOS 0.2.0 x86_64 native kernel\n");
}

static void cmd_mem() {
    auto h = mm::heap_get_stats();
    auto f = mm::frame_allocator_stats();
    lib::kprintf("heap  total=%u used=%u free=%u allocs=%u\n",
                 static_cast<unsigned>(h.total_bytes),
                 static_cast<unsigned>(h.used_bytes),
                 static_cast<unsigned>(h.free_bytes),
                 static_cast<unsigned>(h.allocation_count));
    lib::kprintf("phys  frames total=%u used=%u free=%u (4KiB each, first 128MiB)\n",
                 static_cast<unsigned>(f.total_frames),
                 static_cast<unsigned>(f.used_frames),
                 static_cast<unsigned>(f.free_frames));
}

static void cmd_ps() {
    auto s = task::scheduler_get_stats();
    auto* cur = task::task_current();
    lib::kprintf("tasks=%u ready=%u switches=%u idle_ticks=%u current_id=%u\n",
                 static_cast<unsigned>(s.total_tasks),
                 static_cast<unsigned>(s.running_tasks),
                 static_cast<unsigned>(s.context_switches),
                 static_cast<unsigned>(s.idle_ticks),
                 cur ? static_cast<unsigned>(cur->id) : 0);
}

static void cmd_ls(const char* path) {
    if (!path || !*path) path = "/";
    fs::vfs_node* n = fs::vfs_lookup(path);
    if (!n) {
        lib::kprintf("ls: %s: not found\n", path);
        return;
    }
    if (n->type != fs::node_type::DIR) {
        lib::kprintf("%s\n", n->name);
        return;
    }
    for (fs::vfs_node* c = n->child; c; c = c->sibling) {
        char mark = (c->type == fs::node_type::DIR) ? '/' :
                    (c->type == fs::node_type::DEV) ? '*' : ' ';
        lib::kprintf("  %s%c\n", c->name, mark);
    }
}

static void cmd_cat(const char* path) {
    if (!path || !*path) {
        lib::kputs("cat: missing path\n");
        return;
    }
    uint32_t size = 0;
    const char* data = fs::vfs_read_all(path, &size);
    if (!data) {
        lib::kprintf("cat: %s: not a file\n", path);
        return;
    }
    lib::kputs(data);
}

static void cmd_reboot() {
    lib::kputs("rebooting...\n");
    arch::io::outb(0x64, 0xFE);
    for (;;) {
#ifdef _MSC_VER
        arch::cpu::cli();
        arch::cpu::hlt();
#else
        __asm__ volatile ("cli; hlt");
#endif
    }
}

static const char* next_token(char*& p) {
    while (*p == ' ' || *p == '\t') p++;
    if (*p == 0) return nullptr;
    const char* start = p;
    while (*p && *p != ' ' && *p != '\t') p++;
    if (*p) {
        *p = 0;
        p++;
    }
    return start;
}

static void cmd_uspspawn(const char* path) {
    if (!path || !*path) {
        lib::kputs("uspspawn: missing path\n");
        return;
    }
    
    uint32_t size = 0;
    const char* data = fs::vfs_read_all(path, &size);
    if (!data) {
        lib::kprintf("uspspawn: %s: not found\n", path);
        return;
    }
    
    uint64_t tid = elf::userspace_task_create(data, size, path);
    if (tid == 0) {
        lib::kprintf("uspspawn: failed to spawn %s\n", path);
    } else {
        lib::kprintf("uspspawn: spawned task %lu\n", tid);
    }
}

static void handle_line(char* line) {
    char* p = line;
    const char* cmd = next_token(p);
    if (!cmd) return;

    if (lib::strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (lib::strcmp(cmd, "uname") == 0) {
        cmd_uname();
    } else if (lib::strcmp(cmd, "clear") == 0) {
        drivers::vga_clear();
    } else if (lib::strcmp(cmd, "mem") == 0) {
        cmd_mem();
    } else if (lib::strcmp(cmd, "ps") == 0) {
        cmd_ps();
    } else if (lib::strcmp(cmd, "ticks") == 0) {
        lib::kprintf("%u\n", static_cast<unsigned>(drivers::pit_ticks()));
    } else if (lib::strcmp(cmd, "ls") == 0) {
        const char* path = next_token(p);
        cmd_ls(path);
    } else if (lib::strcmp(cmd, "cat") == 0) {
        cmd_cat(next_token(p));
    } else if (lib::strcmp(cmd, "echo") == 0) {
        const char* arg = next_token(p);
        while (arg) {
            lib::kputs(arg);
            arg = next_token(p);
            if (arg) lib::kputc(' ');
        }
        lib::kputc('\n');
    } else if (lib::strcmp(cmd, "reboot") == 0) {
        cmd_reboot();
    } else if (lib::strcmp(cmd, "uspspawn") == 0) {
        const char* path = next_token(p);
        cmd_uspspawn(path);
    } else if (lib::strcmp(cmd, "panic") == 0) {
        panic("shell requested panic");
    } else {
        lib::kprintf("unknown command: %s\n", cmd);
    }
}

void shell_run() {
    print_banner();
    char line[128];
    unsigned len = 0;

    for (;;) {
        lib::kputs("feather> ");
        len = 0;
        for (;;) {
            char c = drivers::keyboard_getc();
            if (c == '\n' || c == '\r') {
                lib::kputc('\n');
                line[len] = 0;
                break;
            }
            if (c == '\b') {
                if (len > 0) {
                    len--;
                    lib::kputs("\b \b");
                }
                continue;
            }
            if (c == 27) continue; // escape
            if (c >= 32 && len + 1 < sizeof(line)) {
                line[len++] = c;
                lib::kputc(c);
            }
        }
        handle_line(line);
    }
}

} // namespace feather::kernel
