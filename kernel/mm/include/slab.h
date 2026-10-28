#pragma once
// FeatherOS Kernel — Slab Allocator
// Cache-based allocator for kernel objects of fixed sizes.

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstddef>
#include <cstdint>

namespace feather::mm::slab {

// Slab cache for a specific object size
struct kmem_cache {
    const char* name;           // Cache name (for debugging)
    size_t      object_size;    // Size of each object
    size_t      align;          // Alignment requirement
    void*       slabs;          // Linked list of slabs
    void*       free_objects;   // Linked list of free objects within slabs
    size_t      objects_per_slab;
    kmem_cache* next;           // Next cache in global list
};

// Create a slab cache for objects of a given size
kmem_cache* kmem_cache_create(const char* name, size_t size, size_t align);

// Destroy a slab cache
void kmem_cache_destroy(kmem_cache* cache);

// Allocate an object from a cache
void* kmem_cache_alloc(kmem_cache* cache);

// Free an object back to a cache
void kmem_cache_free(kmem_cache* cache, void* obj);

// Allocate raw memory (like kmalloc)
void* kmalloc_slab(size_t size);

// Free raw memory (like kfree)
void kfree_slab(void* ptr);

// Initialize the slab subsystem
void slab_init();

} // namespace feather::mm::slab
