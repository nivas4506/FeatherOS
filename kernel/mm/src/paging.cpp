#include "paging.h"
#include "frame_allocator.h"
#include "../../arch/x86_64/include/cpu.h"
#include "../../lib/include/string.h"

namespace feather::mm {

static uint64_t kernel_cr3 = 0;

static constexpr uint64_t PTE_P  = 1ull << 0;
static constexpr uint64_t PTE_RW = 1ull << 1;
static constexpr uint64_t PTE_PS = 1ull << 7;

static uint64_t* table_at(uint64_t phys) {
    return reinterpret_cast<uint64_t*>(phys);
}

void paging_init() {
    kernel_cr3 = arch::cpu::read_cr3();
}

uint64_t paging_cr3() {
    return kernel_cr3;
}

void* alloc_page() {
    void* frame = frame_allocator_alloc();
    if (frame) lib::memset(frame, 0, 4096);
    return frame;
}

void map_page(void* virt, void* phys, bool writable) {
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

    // Bootstrap uses 2 MiB pages. If the PD entry is huge, leave it alone —
    // identity map already covers the first 1 GiB.
    uint64_t* pd = table_at(pdpt[pdpte] & ~0xFFFull);
    if (pd[pde] & PTE_PS) {
        (void)p;
        (void)writable;
        (void)pte;
        return;
    }

    if (!(pd[pde] & PTE_P)) {
        void* frame = alloc_page();
        if (!frame) return;
        pd[pde] = reinterpret_cast<uint64_t>(frame) | PTE_P | PTE_RW;
    }
    uint64_t* pt = table_at(pd[pde] & ~0xFFFull);
    uint64_t flags = PTE_P | (writable ? PTE_RW : 0);
    pt[pte] = (p & ~0xFFFull) | flags;

    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

void unmap_page(void* virt) {
    (void)virt;
}

} // namespace feather::mm
