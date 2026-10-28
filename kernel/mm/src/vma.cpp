// FeatherOS Kernel — Virtual Memory Areas (VMA) Implementation

#include "vma.h"
#include "../../lib/include/string.h"
#include "../../mm/include/heap.h"

namespace feather::mm::vma {
namespace {

static vma* g_vma_list = nullptr;

} // anonymous namespace

vma* vma_create(uint64_t start, uint64_t end, uint32_t flags) {
    auto* area = static_cast<vma*>(mm::kmalloc(sizeof(vma)));
    if (!area) return nullptr;
    area->vm_start = start;
    area->vm_end = end;
    area->vm_flags = flags;
    area->vm_pgoff = 0;
    area->vm_file = nullptr;
    area->vm_file_offset = 0;
    area->vm_next = nullptr;
    return area;
}

void vma_destroy(vma* area) {
    if (!area) return;
    mm::kfree(area);
}

int vma_insert(vma* head, vma* area) {
    if (!head || !area) return -1;

    // Insert in order by start address
    vma* prev = head;
    vma* cur = head->vm_next;

    while (cur && cur->vm_start < area->vm_start) {
        prev = cur;
        cur = cur->vm_next;
    }

    area->vm_next = cur;
    prev->vm_next = area;
    return 0;
}

void vma_remove(vma* head, vma* area) {
    if (!head || !area) return;

    vma* prev = head;
    vma* cur = head->vm_next;

    while (cur) {
        if (cur == area) {
            prev->vm_next = cur->vm_next;
            return;
        }
        prev = cur;
        cur = cur->vm_next;
    }
}

vma* vma_find(vma* head, uint64_t addr) {
    vma* cur = head ? head->vm_next : nullptr;
    while (cur) {
        if (addr >= cur->vm_start && addr < cur->vm_end) {
            return cur;
        }
        cur = cur->vm_next;
    }
    return nullptr;
}

void vma_merge(vma* head) {
    if (!head) return;

    vma* cur = head->vm_next;
    while (cur && cur->vm_next) {
        if (cur->vm_end == cur->vm_next->vm_start &&
            cur->vm_flags == cur->vm_next->vm_flags) {
            cur->vm_end = cur->vm_next->vm_end;
            vma* to_remove = cur->vm_next;
            cur->vm_next = to_remove->vm_next;
            vma_destroy(to_remove);
        } else {
            cur = cur->vm_next;
        }
    }
}

void vma_init() {
    g_vma_list = static_cast<vma*>(mm::kmalloc(sizeof(vma)));
    if (g_vma_list) {
        g_vma_list->vm_start = 0;
        g_vma_list->vm_end = 0;
        g_vma_list->vm_flags = 0;
        g_vma_list->vm_next = nullptr;
    }
}

} // namespace feather::mm::vma
