// FeatherOS Kernel — ext2 / ext3 / ext4 implementation
// Reads superblock, block group descriptors, inodes, directories, and file data.

#include "ext2.h"
#include "../../lib/include/string.h"
#include "../../mm/include/heap.h"

namespace feather::fs::ext2 {

namespace {

// Block device read callback (set during mount)
static bool (*g_block_read)(uint64_t sector, void* buffer, uint32_t count) = nullptr;

// Simple byte comparison (avoids <cstring> MSVC header issues)
static bool mem_equal(const void* a, const void* b, size_t n) {
    const uint8_t* pa = static_cast<const uint8_t*>(a);
    const uint8_t* pb = static_cast<const uint8_t*>(b);
    for (size_t i = 0; i < n; i++) {
        if (pa[i] != pb[i]) return false;
    }
    return true;
}

// Simple memory copy
static void mem_copy(void* dst, const void* src, size_t n) {
    uint8_t* d = static_cast<uint8_t*>(dst);
    const uint8_t* s = static_cast<const uint8_t*>(src);
    for (size_t i = 0; i < n; i++) d[i] = s[i];
}

// Filesystem geometry
static uint32_t g_block_size = 1024;
static uint32_t g_blocks_per_group = 0;
static uint32_t g_inodes_per_group = 0;
static uint32_t g_inode_size = 128;
static uint32_t g_first_data_block = 1;

// Cached superblock
static superblock g_sb = {};

// Read a block (filesystem block size) into buffer
bool read_block(uint32_t block, void* buffer) {
    if (!g_block_read) return false;
    uint32_t sectors_per_block = g_block_size / 512;
    uint64_t start_sector = static_cast<uint64_t>(block) * sectors_per_block;
    return g_block_read(start_sector, buffer, sectors_per_block);
}

// Read the superblock (located at offset 1024)
bool read_superblock() {
    uint8_t buf[1024];
    if (!g_block_read(2, buf, 2)) return false;  // sector 2, 2 sectors = 1024 bytes
    mem_copy(&g_sb, buf, sizeof(superblock));
    if (g_sb.magic != EXT2_SUPER_MAGIC) return false;

    if (g_sb.log_block_size == 0) {
        g_block_size = 1024;
    } else {
        g_block_size = 1024u << g_sb.log_block_size;
    }
    g_blocks_per_group = g_sb.blocks_per_group;
    g_inodes_per_group = g_sb.inodes_per_group;
    g_first_data_block = g_sb.first_data_block;
    g_inode_size = (g_sb.rev_level >= 1) ? 256 : 128;

    return true;
}

// Read a block group descriptor
bool read_block_group_desc(uint32_t group, block_group_desc& bgd) {
    uint32_t desc_block = (g_block_size == 1024) ? 2 : 1;
    uint32_t desc_size = sizeof(block_group_desc);
    uint32_t descs_per_block = g_block_size / desc_size;
    uint32_t block = desc_block + (group / descs_per_block);
    uint32_t offset_in_block = (group % descs_per_block) * desc_size;

    uint8_t buf[4096];
    if (!read_block(block, buf)) return false;
    mem_copy(&bgd, buf + offset_in_block, desc_size);
    return true;
}

// Read an inode by number (1-based)
bool read_inode(uint32_t inode_num, inode& ino) {
    if (inode_num == 0 || inode_num > g_sb.inodes_count) return false;

    uint32_t group = (inode_num - 1) / g_inodes_per_group;
    uint32_t index = (inode_num - 1) % g_inodes_per_group;

    block_group_desc bgd;
    if (!read_block_group_desc(group, bgd)) return false;

    uint32_t inode_table_block = bgd.inode_table_block;
    uint32_t inodes_per_block = g_block_size / g_inode_size;
    uint32_t block = inode_table_block + (index / inodes_per_block);
    uint32_t offset_in_block = (index % inodes_per_block) * g_inode_size;

    uint8_t buf[4096];
    if (!read_block(block, buf)) return false;
    size_t copy_size = (g_inode_size > sizeof(ino)) ? sizeof(ino) : g_inode_size;
    mem_copy(&ino, buf + offset_in_block, copy_size);
    return true;
}

// Read directory entries from an inode
bool read_dir_entries(void* inode_handle, void* entries_buf, uint32_t buf_size) {
    inode* ino = reinterpret_cast<inode*>(inode_handle);
    if (!ino || !is_directory(inode_handle)) return false;

    uint32_t offset = 0;
    uint8_t* dst = reinterpret_cast<uint8_t*>(entries_buf);
    uint32_t dst_offset = 0;

    for (int i = 0; i < 12 && dst_offset < buf_size; i++) {
        uint32_t block = ino->block[i];
        if (block == 0) continue;

        uint8_t block_buf[4096];
        if (g_block_size > sizeof(block_buf)) continue;
        if (!read_block(block, block_buf)) continue;

        uint32_t pos = 0;
        while (pos < g_block_size) {
            dir_entry* de = reinterpret_cast<dir_entry*>(block_buf + pos);
            if (de->rec_len == 0) break;
            if (de->inode != 0) {
                uint32_t copy_len = de->name_len;
                if (dst_offset + sizeof(uint32_t) + copy_len + 1 > buf_size) break;
                *reinterpret_cast<uint32_t*>(dst + dst_offset) = de->inode;
                dst_offset += sizeof(uint32_t);
                mem_copy(dst + dst_offset, de->name, copy_len);
                dst_offset += copy_len;
                dst[dst_offset] = 0;
                dst_offset++;
            }
            pos += de->rec_len;
            if (de->rec_len == 0) break;
        }
    }
    return dst_offset > 0;
}

// Resolve a path component within a directory
bool resolve_component(void* dir_inode_handle, const char* name, uint32_t& out_inode) {
    // dir_inode_handle is a pointer to the inode number (uint32_t)
    uint32_t inode_num = *reinterpret_cast<uint32_t*>(dir_inode_handle);
    inode dir_ino;
    if (!read_inode(inode_num, dir_ino)) return false;

    uint8_t entries_buf[4096];
    if (!read_dir_entries(dir_inode_handle, entries_buf, sizeof(entries_buf))) return false;

    uint32_t offset = 0;
    while (offset < sizeof(entries_buf)) {
        uint32_t ino = *reinterpret_cast<uint32_t*>(entries_buf + offset);
        offset += sizeof(uint32_t);
        if (ino == 0) break;
        const char* entry_name = reinterpret_cast<const char*>(entries_buf + offset);
        offset += lib::strlen(entry_name) + 1;
        if (lib::strcmp(entry_name, name) == 0) {
            out_inode = ino;
            return true;
        }
    }
    return false;
}

// Read file data blocks into buffer
bool read_file_data(inode& ino, void* buffer, uint32_t size, uint32_t offset, uint32_t& bytes_read) {
    bytes_read = 0;
    if (!buffer || size == 0) return false;

    uint32_t file_size = ino.size_lo;
    if (offset >= file_size) return false;
    uint32_t to_read = (size < file_size - offset) ? size : file_size - offset;

    uint8_t* dst = reinterpret_cast<uint8_t*>(buffer);
    uint32_t remaining = to_read;
    uint32_t cur_offset = offset;

    for (int i = 0; i < 15 && remaining > 0; i++) {
        uint32_t block = ino.block[i];
        if (block == 0) break;
        if (i >= 12) break;  // indirect blocks not yet supported

        uint32_t block_offset = cur_offset % g_block_size;
        uint32_t block_remaining = g_block_size - block_offset;
        uint32_t copy = (remaining < block_remaining) ? remaining : block_remaining;

        uint8_t block_buf[4096];
        if (g_block_size > sizeof(block_buf)) break;
        if (!read_block(block, block_buf)) break;

        mem_copy(dst, block_buf + block_offset, copy);
        dst += copy;
        remaining -= copy;
        cur_offset += copy;
    }

    bytes_read = to_read - remaining;
    return bytes_read > 0;
}

} // anonymous namespace

bool mount(bool (*block_read)(uint64_t sector, void* buffer, uint32_t count)) {
    g_block_read = block_read;
    return read_superblock();
}

void unmount() {
    g_block_read = nullptr;
}

void* lookup(const char* path) {
    if (!path || path[0] == 0) return nullptr;

    uint32_t inode_num = 2;
    inode root_ino;
    if (!read_inode(inode_num, root_ino)) return nullptr;

    const char* p = path;
    if (*p == '/') p++;

    while (*p) {
        while (*p == '/') p++;
        if (*p == 0) break;

        char component[256];
        uint32_t len = 0;
        while (*p && *p != '/' && len < sizeof(component) - 1) {
            component[len++] = *p++;
        }
        component[len] = 0;

        uint32_t next_inode = 0;
        void* dir_handle = reinterpret_cast<void*>(&inode_num);
        if (!resolve_component(dir_handle, component, next_inode)) return nullptr;

        if (!read_inode(next_inode, root_ino)) return nullptr;
        inode_num = next_inode;
    }

    inode* result = reinterpret_cast<inode*>(mm::kmalloc(sizeof(inode)));
    if (!result) return nullptr;
    if (!read_inode(inode_num, *result)) {
        mm::kfree(result);
        return nullptr;
    }
    return result;
}

uint32_t read_file(void* inode_ptr, void* buffer, uint32_t size, uint32_t offset) {
    inode* ino = reinterpret_cast<inode*>(inode_ptr);
    if (!ino) return 0;

    uint32_t bytes_read = 0;
    read_file_data(*ino, buffer, size, offset, bytes_read);
    return bytes_read;
}

uint64_t get_file_size(void* inode_ptr) {
    inode* ino = reinterpret_cast<inode*>(inode_ptr);
    if (!ino) return 0;
    return ino->size_lo;
}

bool is_directory(void* inode_ptr) {
    inode* ino = reinterpret_cast<inode*>(inode_ptr);
    if (!ino) return false;
    return (ino->mode & EXT2_S_IFDIR) != 0;
}

} // namespace feather::fs::ext2
