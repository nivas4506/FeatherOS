#pragma once
// FeatherOS Kernel — ELF Loader for Userspace Programs
// Loads ELF64 executables into user address spaces


#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>
#include <cstddef>

namespace feather::kernel::elf {

// ELF64 header
struct Elf64_Ehdr {
    unsigned char e_ident[16];    // Magic and other info
    uint16_t      e_type;         // Object file type
    uint16_t      e_machine;      // Architecture
    uint32_t      e_version;      // Object file version
    uint64_t      e_entry;        // Entry point virtual address
    uint64_t      e_phoff;        // Program header table file offset
    uint64_t      e_shoff;        // Section header table file offset
    uint32_t      e_flags;        // Processor-specific flags
    uint16_t      e_ehsize;       // ELF header size in bytes
    uint16_t      e_phentsize;    // Program header table entry size
    uint16_t      e_phnum;        // Program header table entry count
    uint16_t      e_shentsize;    // Section header table entry size
    uint16_t      e_shnum;        // Section header table entry count
    uint16_t      e_shstrndx;     // Section header string table index
};

// ELF64 program header
struct Elf64_Phdr {
    uint32_t p_type;   // Segment type
    uint32_t p_flags;  // Segment flags
    uint64_t p_offset; // Segment file offset
    uint64_t p_vaddr;  // Segment virtual address
    uint64_t p_paddr;  // Segment physical address
    uint64_t p_filesz; // Segment size in file
    uint64_t p_memsz;  // Segment size in memory
    uint64_t p_align;  // Segment alignment
};

// ELF segment types
enum : uint32_t {
    PT_NULL    = 0,
    PT_LOAD    = 1,
    PT_DYNAMIC = 2,
    PT_INTERP  = 3,
    PT_NOTE    = 4,
    PT_SHLIB   = 5,
    PT_PHDR    = 6,
};

// Segment flags
enum : uint32_t {
    PF_X = 1, // Execute
    PF_W = 2, // Write
    PF_R = 4, // Read
};

// Magic number
constexpr uint32_t ELF_MAGIC = 0x464c457f; // "\x7fELF"

// User process address space
struct UserAddressSpace {
    uint64_t  pml4_page;       // Physical address of PML4 table
    uint64_t  entry_point;     // Userspace entry point
    uint64_t  stack_top;       // User stack top
    uint64_t  stack_size;      // User stack size in bytes
    uint64_t  brk;             // Current program break (heap end)
    bool      active;          // Whether this address space is active
};

// Load ELF from memory buffer into user address space
// Returns 0 on success, negative errno on failure
int elf_load(const void* elf_data, size_t elf_size, UserAddressSpace* space);

// Create a new user address space
UserAddressSpace* user_as_create();

// Destroy a user address space
void user_as_destroy(UserAddressSpace* space);

// Switch to a user address space (load CR3)
void user_as_switch(UserAddressSpace* space);

// Allocate user stack
uint64_t user_stack_alloc(UserAddressSpace* space, size_t size);

// Free user stack
void user_stack_free(UserAddressSpace* space, uint64_t stack_addr, size_t size);

// Create a userspace task from ELF
// Returns task ID on success, 0 on failure
uint64_t userspace_task_create(const void* elf_data, size_t elf_size,
                               const char* name);

// Initialize ELF loader subsystem
void elf_init();

} // namespace feather::kernel::elf
