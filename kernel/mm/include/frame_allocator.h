#pragma once

#include <cstddef>
#include <cstdint>

namespace feather::mm {

void frame_allocator_init();
void* frame_allocator_alloc();
void frame_allocator_free(void* frame);

struct frame_stats {
    std::size_t total_frames;
    std::size_t used_frames;
    std::size_t free_frames;
};
frame_stats frame_allocator_stats();

} // namespace feather::mm
