#pragma once
// FeatherOS Kernel — Heap Allocator (kmalloc/kfree)

#include <cstddef>
#include <cstdint>

namespace feather::mm {

// Initialize the heap (call after paging_init)
void heap_init();

// Allocate `size` bytes, returns nullptr on failure
void* kmalloc(std::size_t size);

// Free memory allocated by kmalloc
void kfree(void* ptr);

// Allocate zeroed memory
void* kcalloc(std::size_t count, std::size_t size);

// Reallocate memory (like realloc)
void* krealloc(void* ptr, std::size_t new_size);

// Get heap statistics
struct heap_stats {
    std::size_t total_bytes;
    std::size_t used_bytes;
    std::size_t free_bytes;
    std::size_t allocation_count;
};
heap_stats heap_get_stats();

} // namespace feather::mm