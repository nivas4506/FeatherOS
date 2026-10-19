#include "heap.h"
#include "../../lib/include/string.h"
#include "../../arch/x86_64/include/cpu.h"

namespace feather::mm {

struct block_header {
    std::size_t   size;
    bool          free;
    block_header* next;
    block_header* prev;
    uint32_t      magic;
};

static constexpr uint32_t BLOCK_MAGIC = 0xFEEDBEEF;
static constexpr std::size_t HEADER_SIZE = sizeof(block_header);
static constexpr std::size_t HEAP_BYTES = 256 * 1024;

alignas(16) static uint8_t heap_arena[HEAP_BYTES];

static block_header* heap_start = nullptr;
static std::size_t heap_used_bytes = 0;
static std::size_t allocation_count = 0;
static volatile int heap_lock = 0;

static void lock() {
    while (__sync_lock_test_and_set(&heap_lock, 1)) {
        while (heap_lock) arch::cpu::pause();
    }
}

static void unlock() {
    __sync_lock_release(&heap_lock);
}

static void split_block(block_header* block, std::size_t size) {
    if (block->size < size + HEADER_SIZE + 16) return;
    auto* extra = reinterpret_cast<block_header*>(
        reinterpret_cast<uint8_t*>(block) + HEADER_SIZE + size);
    extra->size = block->size - size - HEADER_SIZE;
    extra->free = true;
    extra->magic = BLOCK_MAGIC;
    extra->next = block->next;
    extra->prev = block;
    if (block->next) block->next->prev = extra;
    block->next = extra;
    block->size = size;
}

static void merge(block_header* block) {
    if (block->next && block->next->free) {
        block->size += HEADER_SIZE + block->next->size;
        block->next = block->next->next;
        if (block->next) block->next->prev = block;
    }
    if (block->prev && block->prev->free) {
        block->prev->size += HEADER_SIZE + block->size;
        block->prev->next = block->next;
        if (block->next) block->next->prev = block->prev;
    }
}

void heap_init() {
    heap_start = reinterpret_cast<block_header*>(heap_arena);
    heap_start->size = HEAP_BYTES - HEADER_SIZE;
    heap_start->free = true;
    heap_start->next = nullptr;
    heap_start->prev = nullptr;
    heap_start->magic = BLOCK_MAGIC;
    heap_used_bytes = 0;
    allocation_count = 0;
}

void* kmalloc(std::size_t size) {
    if (!heap_start) heap_init();
    if (size == 0) return nullptr;
    size = (size + 7) & ~static_cast<std::size_t>(7);

    lock();
    block_header* cur = heap_start;
    while (cur) {
        if (cur->free && cur->size >= size) {
            cur->free = false;
            split_block(cur, size);
            heap_used_bytes += cur->size + HEADER_SIZE;
            allocation_count++;
            void* payload = reinterpret_cast<uint8_t*>(cur) + HEADER_SIZE;
            unlock();
            return payload;
        }
        cur = cur->next;
    }
    unlock();
    return nullptr;
}

void kfree(void* ptr) {
    if (!ptr) return;
    auto* block = reinterpret_cast<block_header*>(
        reinterpret_cast<uint8_t*>(ptr) - HEADER_SIZE);
    if (block->magic != BLOCK_MAGIC) return;

    lock();
    block->free = true;
    if (heap_used_bytes >= block->size + HEADER_SIZE) {
        heap_used_bytes -= block->size + HEADER_SIZE;
    }
    if (allocation_count) allocation_count--;
    merge(block);
    unlock();
}

void* kcalloc(std::size_t count, std::size_t size) {
    std::size_t total = count * size;
    void* ptr = kmalloc(total);
    if (ptr) lib::memset(ptr, 0, total);
    return ptr;
}

void* krealloc(void* ptr, std::size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) { kfree(ptr); return nullptr; }

    auto* block = reinterpret_cast<block_header*>(
        reinterpret_cast<uint8_t*>(ptr) - HEADER_SIZE);
    if (block->size >= new_size) return ptr;

    void* neu = kmalloc(new_size);
    if (neu) {
        lib::memcpy(neu, ptr, block->size);
        kfree(ptr);
    }
    return neu;
}

heap_stats heap_get_stats() {
    return {
        .total_bytes = HEAP_BYTES,
        .used_bytes = heap_used_bytes,
        .free_bytes = HEAP_BYTES - heap_used_bytes,
        .allocation_count = allocation_count
    };
}

} // namespace feather::mm
