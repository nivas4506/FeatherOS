#pragma once

#include <cstddef>
#include <cstdint>

namespace feather::mm {

void paging_init();
void map_page(void* virt, void* phys, bool writable);
void unmap_page(void* virt);
void* alloc_page();
uint64_t paging_cr3();

} // namespace feather::mm
