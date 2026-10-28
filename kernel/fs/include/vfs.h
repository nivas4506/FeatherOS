#pragma once


#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstddef>
#include <cstdint>

namespace feather::fs {

enum class node_type : uint32_t {
    FILE = 0,
    DIR  = 1,
    DEV  = 2,
};

struct vfs_node {
    const char* name;
    node_type   type;
    uint32_t    size;
    const char* data;
    vfs_node*   parent;
    vfs_node*   child;
    vfs_node*   sibling;
};

void vfs_init();
vfs_node* vfs_root();
vfs_node* vfs_lookup(const char* path);
void vfs_list(const char* path);
const char* vfs_read_all(const char* path, uint32_t* out_size);

} // namespace feather::fs

