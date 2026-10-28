#pragma once
// FeatherOS Kernel — Virtual Memory Areas (VMA)
// Per-process virtual memory region tracking for mmap/munmap/brk.

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstddef>
#include <cstdint>

namespace feather::mm::vma {

// VMA flags
enum vma_flags : uint32_t {
    VMA_READ     = 0x01,
    VMA_WRITE    = 0x02,
    VMA_EXEC     = 0x04,
    VMA_SHARED   = 0x08,
    VMA_PRIVATE  = 0x10,
    VMA_GROWSDOWN = 0x20,
    VMA_GROWSUP   = 0x40,
    VMA_ANONYMOUS = 0x80,
    VMA_FILE      = 0x100,
};

// A virtual memory area
struct vma {
    uint64_t  vm_start;         // Start address (inclusive)
    uint64_t  vm_end;           // End address (exclusive)
    uint32_t  vm_flags;         // Protection flags
    uint64_t  vm_pgoff;         // File/page offset
    void*     vm_file;          // Backing file (if file-backed)
    uint64_t  vm_file_offset;   // Offset within file
    vma*      vm_next;          // Next VMA in process list
};

// Create a new VMA
vma* vma_create(uint64_t start, uint64_t end, uint32_t flags);

// Destroy a VMA
void vma_destroy(vma* area);

// Insert a VMA into a process's VMA list
int vma_insert(vma* head, vma* area);

// Remove a VMA from a process's VMA list
void vma_remove(vma* head, vma* area);

// Find a VMA containing a given address
vma* vma_find(vma* head, uint64_t addr);

// Merge adjacent VMAs with same flags
void vma_merge(vma* head);

// Initialize the VMA subsystem
void vma_init();

} // namespace feather::mm::vma
