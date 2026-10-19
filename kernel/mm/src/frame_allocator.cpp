#include "frame_allocator.h"
#include "../../lib/include/string.h"

extern "C" char _kernel_start[];
extern "C" char _kernel_end[];

namespace feather::mm {

// Track the first 128 MiB of physical memory (32768 * 4 KiB).
static constexpr std::size_t FRAME_SIZE = 4096;
static constexpr std::size_t MAX_FRAMES = 32768;
static uint8_t bitmap[MAX_FRAMES / 8];
static std::size_t used_frames = 0;
static std::size_t search_from = 0;

static void set_used(std::size_t frame, bool used) {
    std::size_t byte = frame / 8;
    std::size_t bit  = frame % 8;
    bool was_used = (bitmap[byte] & (1u << bit)) != 0;
    if (used) {
        bitmap[byte] |= static_cast<uint8_t>(1u << bit);
        if (!was_used) used_frames++;
    } else {
        bitmap[byte] = static_cast<uint8_t>(bitmap[byte] & ~(1u << bit));
        if (was_used) used_frames--;
    }
}

static bool is_used(std::size_t frame) {
    return (bitmap[frame / 8] & (1u << (frame % 8))) != 0;
}

void frame_allocator_init() {
    lib::memset(bitmap, 0, sizeof(bitmap));
    used_frames = 0;

    // Reserve the first 1 MiB (IVT, BIOS, VGA) plus the kernel image.
    std::size_t kernel_end = (reinterpret_cast<uintptr_t>(_kernel_end) + FRAME_SIZE - 1) / FRAME_SIZE;
    if (kernel_end < 256) kernel_end = 256; // 1 MiB

    for (std::size_t f = 0; f < kernel_end && f < MAX_FRAMES; f++) {
        set_used(f, true);
    }
    search_from = kernel_end;
}

void* frame_allocator_alloc() {
    for (std::size_t f = search_from; f < MAX_FRAMES; f++) {
        if (!is_used(f)) {
            set_used(f, true);
            search_from = f + 1;
            return reinterpret_cast<void*>(f * FRAME_SIZE);
        }
    }
    for (std::size_t f = 0; f < search_from; f++) {
        if (!is_used(f)) {
            set_used(f, true);
            search_from = f + 1;
            return reinterpret_cast<void*>(f * FRAME_SIZE);
        }
    }
    return nullptr;
}

void frame_allocator_free(void* frame) {
    if (!frame) return;
    std::size_t f = reinterpret_cast<uintptr_t>(frame) / FRAME_SIZE;
    if (f >= MAX_FRAMES) return;
    set_used(f, false);
    if (f < search_from) search_from = f;
}

frame_stats frame_allocator_stats() {
    return {
        .total_frames = MAX_FRAMES,
        .used_frames = used_frames,
        .free_frames = MAX_FRAMES - used_frames
    };
}

} // namespace feather::mm
