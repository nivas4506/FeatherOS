// FeatherOS Kernel — Slab Allocator Implementation
// Cache-based allocator for kernel objects.

#include "slab.h"
#include "../../lib/include/string.h"
#include "../../lib/include/printf.h"
#include "../include/heap.h"

namespace feather::mm::slab {
namespace {

// Global cache list
static kmem_cache* g_cache_list = nullptr;

// Slab header (embedded at start of each slab page)
struct slab {
    kmem_cache* cache;
    void*       free;           // First free object in this slab
    uint32_t    free_count;     // Number of free objects
    uint32_t    used_count;     // Number of used objects
    slab*       next;           // Next slab in cache
    slab*       prev;           // Previous slab in cache
};

// Object header (embedded at start of each free object)
struct obj_header {
    obj_header* next;           // Next free object
};

// Round up to alignment
static size_t align_up(size_t val, size_t align) {
    return (val + align - 1) & ~(align - 1);
}

} // anonymous namespace

kmem_cache* kmem_cache_create(const char* name, size_t size, size_t align) {
    if (!name || size == 0) return nullptr;

    size = align_up(size, align ? align : 8);
    if (size < sizeof(obj_header)) size = sizeof(obj_header);

    auto* cache = static_cast<kmem_cache*>(mm::kmalloc(sizeof(kmem_cache)));
    if (!cache) return nullptr;

    cache->name = name;
    cache->object_size = size;
    cache->align = align ? align : 8;
    cache->slabs = nullptr;
    cache->free_objects = nullptr;
    cache->objects_per_slab = 4096 / size;
    if (cache->objects_per_slab == 0) cache->objects_per_slab = 1;
    cache->next = g_cache_list;
    g_cache_list = cache;

    return cache;
}

void kmem_cache_destroy(kmem_cache* cache) {
    if (!cache) return;

    // Remove from global list
    if (g_cache_list == cache) {
        g_cache_list = cache->next;
    } else {
        kmem_cache* prev = g_cache_list;
        while (prev && prev->next != cache) prev = prev->next;
        if (prev) prev->next = cache->next;
    }

    mm::kfree(cache);
}

void* kmem_cache_alloc(kmem_cache* cache) {
    if (!cache) return nullptr;

    // Try to find a free object in existing slabs
    slab* s = reinterpret_cast<slab*>(cache->slabs);
    while (s) {
        if (s->free_count > 0 && s->free) {
            obj_header* obj = reinterpret_cast<obj_header*>(s->free);
            s->free = obj->next;
            s->free_count--;
            s->used_count++;
            return obj;
        }
        s = s->next;
    }

    // No free object found, allocate a new slab
    void* slab_mem = mm::kmalloc(4096);
    if (!slab_mem) return nullptr;

    auto* new_slab = reinterpret_cast<slab*>(slab_mem);
    new_slab->cache = cache;
    new_slab->free = reinterpret_cast<uint8_t*>(slab_mem) + sizeof(slab);
    new_slab->free_count = cache->objects_per_slab;
    new_slab->used_count = 0;
    new_slab->next = reinterpret_cast<slab*>(cache->slabs);
    new_slab->prev = nullptr;
    if (new_slab->next) new_slab->next->prev = new_slab;
    cache->slabs = new_slab;

    // Initialize free object list within the slab
    uint8_t* obj_ptr = reinterpret_cast<uint8_t*>(new_slab->free);
    obj_header* prev = nullptr;
    for (uint32_t i = 0; i < cache->objects_per_slab; i++) {
        obj_header* cur = reinterpret_cast<obj_header*>(obj_ptr + i * cache->object_size);
        cur->next = prev;
        prev = cur;
    }
    new_slab->free = reinterpret_cast<void*>(prev);

    // Allocate first object
    obj_header* obj = reinterpret_cast<obj_header*>(new_slab->free);
    new_slab->free = obj->next;
    new_slab->free_count--;
    new_slab->used_count++;
    return obj;
}

void kmem_cache_free(kmem_cache* cache, void* obj) {
    if (!cache || !obj) return;

    // Find which slab this object belongs to
    slab* s = reinterpret_cast<slab*>(cache->slabs);
    while (s) {
        uintptr_t obj_ptr = reinterpret_cast<uintptr_t>(obj);
        uintptr_t slab_start = reinterpret_cast<uintptr_t>(s);
        uintptr_t slab_end = slab_start + 4096;
        if (obj_ptr >= slab_start && obj_ptr < slab_end) {
            // Return object to free list
            obj_header* oh = reinterpret_cast<obj_header*>(obj);
            oh->next = reinterpret_cast<obj_header*>(s->free);
            s->free = obj;
            s->free_count++;
            s->used_count--;
            return;
        }
        s = s->next;
    }
}

void* kmalloc_slab(size_t size) {
    // Find or create a cache for this size
    kmem_cache* cache = g_cache_list;
    while (cache) {
        if (cache->object_size >= size) break;
        cache = cache->next;
    }
    if (!cache) {
        cache = kmem_cache_create("kmalloc", size, 8);
        if (!cache) return nullptr;
    }
    return kmem_cache_alloc(cache);
}

void kfree_slab(void* ptr) {
    if (!ptr) return;
    // We need to find which cache this belongs to
    // Simplified: search all caches
    kmem_cache* cache = g_cache_list;
    while (cache) {
        // Check if ptr belongs to any slab in this cache
        slab* s = reinterpret_cast<slab*>(cache->slabs);
        while (s) {
            uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
            uintptr_t s_start = reinterpret_cast<uintptr_t>(s);
            uintptr_t s_end = s_start + 4096;
            if (p >= s_start && p < s_end) {
                kmem_cache_free(cache, ptr);
                return;
            }
            s = s->next;
        }
        cache = cache->next;
    }
}

void slab_init() {
    g_cache_list = nullptr;
}

} // namespace feather::mm::slab
