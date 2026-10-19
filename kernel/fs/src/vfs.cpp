#include "vfs.h"
#include "../../lib/include/string.h"

namespace feather::fs {

static vfs_node nodes[32];
static unsigned node_count = 0;
static vfs_node* root = nullptr;

static vfs_node* make_node(const char* name, node_type type, const char* data, vfs_node* parent) {
    if (node_count >= 32) return nullptr;
    vfs_node* n = &nodes[node_count++];
    n->name = name;
    n->type = type;
    n->data = data;
    n->size = data ? static_cast<uint32_t>(lib::strlen(data)) : 0;
    n->parent = parent;
    n->child = nullptr;
    n->sibling = nullptr;
    if (parent) {
        n->sibling = parent->child;
        parent->child = n;
    }
    return n;
}

void vfs_init() {
    node_count = 0;
    root = make_node("/", node_type::DIR, nullptr, nullptr);

    auto* proc = make_node("proc", node_type::DIR, nullptr, root);
    auto* dev  = make_node("dev",  node_type::DIR, nullptr, root);

    make_node("welcome.txt", node_type::FILE,
        "Welcome to FeatherOS.\n"
        "Type 'help' for a list of kernel shell commands.\n",
        root);

    make_node("version", node_type::FILE,
        "FeatherOS kernel 0.2.0 x86_64\n",
        proc);

    make_node("cmdline", node_type::FILE,
        "feather.native=1 console=vga,serial\n",
        proc);

    make_node("null", node_type::DEV, "", dev);
    make_node("console", node_type::DEV, "", dev);
}

vfs_node* vfs_root() {
    return root;
}

static vfs_node* find_child(vfs_node* dir, const char* name, std::size_t len) {
    for (vfs_node* c = dir->child; c; c = c->sibling) {
        if (lib::strlen(c->name) == len && lib::strncmp(c->name, name, len) == 0) {
            return c;
        }
    }
    return nullptr;
}

vfs_node* vfs_lookup(const char* path) {
    if (!path || !root) return nullptr;
    if (path[0] == 0 || (path[0] == '/' && path[1] == 0)) return root;
    if (path[0] != '/') return nullptr;

    vfs_node* cur = root;
    const char* p = path + 1;
    while (*p) {
        const char* start = p;
        while (*p && *p != '/') p++;
        std::size_t len = static_cast<std::size_t>(p - start);
        if (len == 0) {
            if (*p == '/') { p++; continue; }
            break;
        }
        cur = find_child(cur, start, len);
        if (!cur) return nullptr;
        if (*p == '/') p++;
    }
    return cur;
}

void vfs_list(const char* path) {
    (void)path;
}

const char* vfs_read_all(const char* path, uint32_t* out_size) {
    vfs_node* n = vfs_lookup(path);
    if (!n || n->type == node_type::DIR) return nullptr;
    if (out_size) *out_size = n->size;
    return n->data ? n->data : "";
}

} // namespace feather::fs
