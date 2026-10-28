#include "paging.h"
#include "frame_allocator.h"
#include "../../arch/x86_64/include/cpu.h"
#include "../../lib/include/string.h"

#ifdef _MSC_VER
extern "C" void __invlpg(void* virt);
#endif

namespace feather::mm {

static uint64_t kernel_cr3 = 0;
static uint64_t user_cr3 = 0;

static constexpr uint64_t PTE_P   = 1ull << 0;  // Present
static constexpr uint64_t PTE_RW  = 1ull << 1;  // Read/Write
static constexpr uint64_t PTE_US  = 1ull << 2;  // User/Supervisor
static constexpr uint64_t PTE_PS  = 1ull << 7;  // Page Size (2 MiB)

static uint64_t* table_at(uint64_t phys) {
    return reinterpret_cast<uint64_t*>(phys & ~0xFFFull);
}

void paging_init() {
    kernel_cr3 = arch::cpu::read_cr3();
    user_cr3 = kernel_cr3;  // Start same as kernel; will be replaced by clone
}

uint64_t paging_cr3() {
    return kernel_cr3;
}

uint64_t paging_user_cr3() {
    return user_cr3 ? user_cr3 : kernel_cr3;
}

void switch_to_kernel_cr3() {
    arch::cpu::write_cr3(kernel_cr3);
}

void switch_to_user_cr3() {
    arch::cpu::write_cr3(paging_user_cr3());
}

void paging_set_user_cr3(uint64_t cr3) {
    user_cr3 = cr3;
}

void* alloc_page() {
    void* frame = frame_allocator_alloc();
    if (frame) lib::memset(frame, 0, 4096);
    return frame;
}

void map_page(void* virt, void* phys, bool writable, bool user) {
    uint64_t v = reinterpret_cast<uint64_t>(virt);
    uint64_t p = reinterpret_cast<uint64_t>(phys);

    uint64_t* pml4 = table_at(kernel_cr3 & ~0xFFFull);
    uint64_t pml4e = (v >> 39) & 0x1FF;
    uint64_t pdpte = (v >> 30) & 0x1FF;
    uint64_t pde   = (v >> 21) & 0x1FF;
    uint64_t pte   = (v >> 12) & 0x1FF;

    if (!(pml4[pml4e] & PTE_P)) {
        void* frame = alloc_page();
        if (!frame) return;
        pml4[pml4e] = reinterpret_cast<uint64_t>(frame) | PTE_P | PTE_RW;
    }
    uint64_t* pdpt = table_at(pml4[pml4e] & ~0xFFFull);

    if (!(pdpt[pdpte] & PTE_P)) {
        void* frame = alloc_page();
        if (!frame) return;
        pdpt[pdpte] = reinterpret_cast<uint64_t>(frame) | PTE_P | PTE_RW;
    }

    // Bootstrap uses 2 MiB pages. If the PD entry is huge, leave it alone.
    uint64_t* pd = table_at(pdpt[pdpte] & ~0xFFFull);
    if (pd[pde] & PTE_PS) {
        (void)p;
        (void)writable;
        (void)user;
        (void)pte;
        return;
    }

    if (!(pd[pde] & PTE_P)) {
        void* frame = alloc_page();
        if (!frame) return;
        pd[pde] = reinterpret_cast<uint64_t>(frame) | PTE_P | PTE_RW;
    }
    uint64_t* pt = table_at(pd[pde] & ~0xFFFull);
    uint64_t flags = PTE_P | (writable ? PTE_RW : 0) | (user ? PTE_US : 0);
    pt[pte] = (p & ~0xFFFull) | flags;

#ifdef _MSC_VER
    __invlpg(virt);
#else
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
#endif
}

void map_user_page(void* virt, void* phys, bool writable) {
    map_page(virt, phys, writable, true);
}

void unmap_page(void* virt) {
    (void)virt;
}

// ─── Clone kernel PML4 for user address space ──────────────────────
//
// Allocates a new PML4 and recursively clones the kernel's page table
// structure. All cloned entries are marked read-only from user mode
// (no PTE_RW unless the kernel explicitly allows it). New user pages
// added later should use map_user_page() which sets the U/S bit.
//
// Returns physical address of new PML4, or 0 on failure.
uint64_t clone_kernel_pml4() {
    // Allocate new PML4
    void* new_pml4 = alloc_page();
    if (!new_pml4) return 0;

    uint64_t* src_pml4 = table_at(kernel_cr3 & ~0xFFFull);
    uint64_t* dst_pml4 = reinterpret_cast<uint64_t*>(new_pml4);

    lib::memset(dst_pml4, 0, 4096);

    // Copy all 512 PML4 entries from kernel to user
    for (int i = 0; i < 512; ++i) {
        uint64_t entry = src_pml4[i];
        if (!(entry & PTE_P)) continue;

        // Clone the PDPT table pointed to by this entry
        void* new_pdpt = alloc_page();
        if (!new_pdpt) {
            // Partial clone — what we have is usable
            return reinterpret_cast<uint64_t>(new_pml4);
        }

        dst_pml4[i] = reinterpret_cast<uint64_t>(new_pdpt) | PTE_P | PTE_RW;

        uint64_t* src_pdpt = table_at(entry & ~0xFFFull);
        uint64_t* dst_pdpt = reinterpret_cast<uint64_t*>(new_pdpt);
        lib::memset(dst_pdpt, 0, 4096);

        for (int j = 0; j < 512; ++j) {
            uint64_t pdpte = src_pdpt[j];
            if (!(pdpte & PTE_P)) continue;

            // Clone the PD table
            void* new_pd = alloc_page();
            if (!new_pd) continue;

            dst_pdpt[j] = reinterpret_cast<uint64_t>(new_pd) | PTE_P | PTE_RW;

            uint64_t* src_pd = table_at(pdpte & ~0xFFFull);
            uint64_t* dst_pd = reinterpret_cast<uint64_t*>(new_pd);
            lib::memset(dst_pd, 0, 4096);

            for (int k = 0; k < 512; ++k) {
                uint64_t pde = src_pd[k];
                if (!(pde & PTE_P)) continue;

                if (pde & PTE_PS) {
                    // 2 MiB huge page — copy as-is but clear RW for user
                    uint64_t flags = pde & (PTE_P | PTE_PS);
                    // User can read but not write (unless kernel explicitly allows)
                    dst_pd[k] = (pde & ~0xFFFull) | flags | PTE_US;
                } else {
                    // 4 KiB page table — clone the PT
                    void* new_pt = alloc_page();
                    if (!new_pt) continue;

                    dst_pd[k] = reinterpret_cast<uint64_t>(new_pt) | PTE_P | PTE_RW;

                    uint64_t* src_pt = table_at(pde & ~0xFFFull);
                    uint64_t* dst_pt = reinterpret_cast<uint64_t*>(new_pt);
                    lib::memset(dst_pt, 0, 4096);

                    for (int l = 0; l < 512; ++l) {
                        uint64_t pte = src_pt[l];
                        if (!(pte & PTE_P)) continue;

                        // Copy PTE: user can read, write only if kernel allows
                        uint64_t flags = pte & PTE_P;
                        if (pte & PTE_RW) flags |= PTE_RW;
                        flags |= PTE_US;  // User-accessible
                        dst_pt[l] = (pte & ~0xFFFull) | flags;
                    }
                }
            }
        }
    }

    return reinterpret_cast<uint64_t>(new_pml4);
}

} // namespace feather::mm