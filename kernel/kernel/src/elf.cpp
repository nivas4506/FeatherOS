// FeatherOS Kernel — ELF Loader
// Loads ELF64 executables into user address spaces

#include "elf.h"
#include "../../lib/include/string.h"
#include "../../lib/include/printf.h"
#include "../../mm/include/paging.h"
#include "../../mm/include/heap.h"
#include "../../task/include/task.h"

namespace feather::kernel::elf {

// ──────────────────────────────────────────────────────────────
// User address space management
// ──────────────────────────────────────────────────────────────

static UserAddressSpace* g_user_spaces[256] = {nullptr};
static uint64_t g_next_as_id = 1;

// Kernel page table (clone base for user spaces)
// In a real implementation, we'd copy the kernel's PML4 and add user pages
static uint64_t g_kernel_pml4 = 0;

UserAddressSpace* user_as_create() {
    // Allocate address space structure
    auto* space = static_cast<UserAddressSpace*>(mm::kmalloc(sizeof(UserAddressSpace)));
    if (!space) return nullptr;
    lib::memset(space, 0, sizeof(UserAddressSpace));
    
    // Clone kernel PML4 (so user space has access to kernel mappings)
    // For now, use the current CR3 as base
    g_kernel_pml4 = mm::paging_cr3();
    
    // Allocate a new PML4 for this user space (copy of kernel's)
    // Simplified: just use kernel PML4 for now (no isolation yet)
    space->pml4_page = g_kernel_pml4;
    
    // Allocate user stack (4 pages = 16 KiB)
    space->stack_size = 4 * 4096;
    space->stack_top = 0x700000000000; // High memory for user stack
    
    // Map user stack pages (with U/S bit for userspace access)
    for (uint64_t i = 0; i < space->stack_size; i += 4096) {
        void* phys = mm::alloc_page();
        if (phys) {
            mm::map_user_page(
                reinterpret_cast<void*>(space->stack_top - space->stack_size + i),
                phys, true);
        }
    }
    
    space->brk = 0x600000000000; // Program break starts below stack
    space->active = false;
    
    // Register address space
    for (size_t i = 0; i < 256; ++i) {
        if (!g_user_spaces[i]) {
            g_user_spaces[i] = space;
            break;
        }
    }
    
    return space;
}

void user_as_destroy(UserAddressSpace* space) {
    if (!space) return;
    
    // Unmap stack pages
    for (uint64_t i = 0; i < space->stack_size; i += 4096) {
        mm::unmap_page(reinterpret_cast<void*>(space->stack_top - space->stack_size + i));
    }
    
    mm::kfree(space);
    
    // Unregister
    for (size_t i = 0; i < 256; ++i) {
        if (g_user_spaces[i] == space) {
            g_user_spaces[i] = nullptr;
            break;
        }
    }
}

void user_as_switch(UserAddressSpace* space) {
    if (!space) return;
    // In real implementation: load CR3 with space->pml4_page
    // For now: just mark active
    space->active = true;
}

uint64_t user_stack_alloc(UserAddressSpace* space, size_t size) {
    // For MVP: return fixed stack top
    return space->stack_top;
}

void user_stack_free(UserAddressSpace* space, uint64_t stack_addr, size_t size) {
    // No-op for MVP
}

// ──────────────────────────────────────────────────────────────
// ELF loading
// ──────────────────────────────────────────────────────────────

static bool is_valid_elf(const Elf64_Ehdr* hdr) {
    return hdr->e_ident[0] == 0x7f &&
           hdr->e_ident[1] == 'E' &&
           hdr->e_ident[2] == 'L' &&
           hdr->e_ident[3] == 'F';
}

int elf_load(const void* elf_data, size_t elf_size, UserAddressSpace* space) {
    if (!elf_data || !elf_size || !space) return -1;
    
    const auto* ehdr = static_cast<const Elf64_Ehdr*>(elf_data);
    
    if (!is_valid_elf(ehdr)) return -1; // Not a valid ELF
    if (ehdr->e_ident[4] != 2) return -1; // Not ELF64
    if (ehdr->e_type != 2) return -1; // Not ET_EXEC
    if (ehdr->e_machine != 0x3e) return -1; // Not x86_64
    if (ehdr->e_phoff == 0 || ehdr->e_phnum == 0) return -1; // No program headers
    
    // Iterate program headers and load PT_LOAD segments
    const auto* phdr = reinterpret_cast<const Elf64_Phdr*>(
        reinterpret_cast<const uint8_t*>(elf_data) + ehdr->e_phoff);
    
    for (uint16_t i = 0; i < ehdr->e_phnum; ++i) {
        if (phdr[i].p_type != PT_LOAD) continue;
        
        uint64_t vaddr = phdr[i].p_vaddr;
        uint64_t memsz = phdr[i].p_memsz;
        uint64_t filesz = phdr[i].p_filesz;
        uint64_t offset = phdr[i].p_offset;
        bool writable = (phdr[i].p_flags & PF_W) != 0;
        bool executable = (phdr[i].p_flags & PF_X) != 0;
        
        // Align to page boundary
        uint64_t page_start = vaddr & ~4095ULL;
        uint64_t page_end = (vaddr + memsz + 4095) & ~4095ULL;
        
        lib::kprintf("[elf] loading segment %u: vaddr=0x%lx memsz=0x%lx filesz=0x%lx\n",
                     i, vaddr, memsz, filesz);
        
        // Map pages for this segment
        for (uint64_t addr = page_start; addr < page_end; addr += 4096) {
            void* phys = mm::alloc_page();
            if (!phys) return -1; // Out of memory
            
            // Copy file data for pages within filesz
            if (addr >= vaddr && addr < vaddr + filesz) {
                uint64_t file_offset = offset + (addr - vaddr);
                uint64_t copy_size = 4096;
                if (file_offset + copy_size > elf_size) copy_size = elf_size - file_offset;
                
                const uint8_t* src = reinterpret_cast<const uint8_t*>(elf_data) + file_offset;
                uint8_t* dst = reinterpret_cast<uint8_t*>(addr);
                lib::memcpy(dst, src, copy_size);
                
                // Zero rest of page
                if (copy_size < 4096) {
                    lib::memset(dst + copy_size, 0, 4096 - copy_size);
                }
            } else if (addr >= vaddr + filesz && addr < vaddr + memsz) {
                // Zero-fill BSS
                uint8_t* dst = reinterpret_cast<uint8_t*>(addr);
                lib::memset(dst, 0, 4096);
            }
            
            mm::map_user_page(reinterpret_cast<void*>(addr), phys, writable);
        }
    }
    
    space->entry_point = ehdr->e_entry;
    lib::kprintf("[elf] entry point: 0x%lx\n", ehdr->e_entry);
    
    return 0;
}

// ──────────────────────────────────────────────────────────────
// Userspace task creation
// ──────────────────────────────────────────────────────────────

uint64_t userspace_task_create(const void* elf_data, size_t elf_size,
                               const char* name) {
    (void)name;
    
    // Create address space
    auto* space = user_as_create();
    if (!space) return 0;
    
    // Clone kernel PML4 for user address space (page table isolation)
    uint64_t user_pml4 = mm::clone_kernel_pml4();
    if (user_pml4) {
        space->pml4_page = user_pml4;
        mm::paging_set_user_cr3(user_pml4);
    } else {
        // Fallback: same CR3 as kernel (no isolation yet)
        mm::paging_set_user_cr3(mm::paging_cr3());
    }
    
    // Load ELF into user address space
    if (elf_load(elf_data, elf_size, space) < 0) {
        user_as_destroy(space);
        return 0;
    }
    
    // Map user stack pages with U/S bit
    for (uint64_t i = 0; i < space->stack_size; i += 4096) {
        void* phys = mm::alloc_page();
        if (phys) {
            mm::map_user_page(
                reinterpret_cast<void*>(space->stack_top - space->stack_size + i),
                phys, true);
        }
    }
    
    // Create kernel task that will switch to userspace
    auto* task_ptr = reinterpret_cast<task::task*>(task::task_create(
        [] (void* arg) {
            auto* space = static_cast<UserAddressSpace*>(arg);
            
            // Switch to user address space
            mm::switch_to_user_cr3();
            
            lib::kprintf("[userspace] switching to user mode at 0x%lx\n", space->entry_point);
            
            // For now: yield back to kernel (full sysret to user mode
            // requires a userspace binary with proper entry point)
            task::task_yield();
            
            // Switch back to kernel CR3
            mm::switch_to_kernel_cr3();
        },
        space,
        task::task_priority::NORMAL,
        32768
    ));
    
    if (!task_ptr) {
        user_as_destroy(space);
        return 0;
    }
    
    return task_ptr->id;
}

// ──────────────────────────────────────────────────────────────
// Init
// ──────────────────────────────────────────────────────────────

void elf_init() {
    lib::kputs("[elf] userspace ELF loader initialized\n");
}

} // namespace feather::kernel::elf