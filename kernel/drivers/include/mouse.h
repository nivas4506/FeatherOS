#pragma once


#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>

namespace feather::drivers {

struct mouse_state {
    int32_t x;
    int32_t y;
    bool left;
    bool right;
    bool middle;
};

void mouse_init(uint32_t screen_width, uint32_t screen_height);
void mouse_handler();
mouse_state mouse_get_state();

} // namespace feather::drivers

