#pragma once
// FeatherOS Kernel — Paging / Memory Management x86_64

#include <cstddef>
#include <cstdint>

namespace feather::mm {

void paging_init();

// Map a physical page at a virtual address.
// If `user` is true, set the U/S bit so userspace can access it.
void map_page(void* virt, void* phys, bool writable, bool user = false);

// Convenience: map a page for userspace access (U/S bit set)
void map_user_page(void* virt, void* phys, bool writable);

void unmap_page(void* virt);
void* alloc_page();
uint64_t paging_cr3();
uint64_t paging_user_cr3();

/// Switch CR3 to the kernel address space (called on syscall entry)
void switch_to_kernel_cr3();

/// Switch CR3 to the user address space (called on sysret)
void switch_to_user_cr3();

/// Set the user CR3 (called when creating a user address space)
void paging_set_user_cr3(uint64_t cr3);

/// Clone the kernel PML4 into a new user address space.
/// The clone maps all kernel pages (read-only from user) plus any user pages.
/// Returns physical address of new PML4, or 0 on failure.
uint64_t clone_kernel_pml4();

} // namespace feather::mm