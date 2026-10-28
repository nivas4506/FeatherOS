// FeatherOS Kernel — FAT32 implementation
// Reads boot sector (BPB), follows FAT cluster chains, parses directories,
// and reads file data.

#include "fat32.h"
#include "../../lib/include/string.h"
#include "../../mm/include/heap.h"

namespace feather::fs::fat32 {

namespace {

// Block device read callback (set during mount)
static bool (*g_block_read)(uint64_t sector, void* buffer, uint32_t count) = nullptr;

// Simple memory copy (avoids <cstring> MSVC header issues)
static void mem_copy(void* dst, const void* src, size_t n) {
    uint8_t* d = static_cast<uint8_t*>(dst);
    const uint8_t* s = static_cast<const uint8_t*>(src);
    for (size_t i = 0; i < n; i++) d[i] = s[i];
}

// Cached BPB
static bpb_t g_bpb = {};

// Derived geometry
static uint32_t g_bytes_per_sector = 512;
static uint32_t g_sectors_per_cluster = 1;
static uint32_t g_bytes_per_cluster = 512;
static uint32_t g_reserved_sectors = 0;
static uint32_t g_fat_size_sectors = 0;
static uint32_t g_num_fats = 0;
static uint32_t g_data_region_sector = 0;
static uint32_t g_root_cluster = 2;

// Cache for one sector (for reading FAT entries)
static uint8_t g_sector_cache[512];
static uint64_t g_cached_sector = 0xFFFFFFFFFFFFFFFF;

// Read a sector into cache
bool read_sector(uint64_t sector) {
    if (!g_block_read) return false;
    if (sector == g_cached_sector) return true;
    if (!g_block_read(sector, g_sector_cache, 1)) return false;
    g_cached_sector = sector;
    return true;
}

// Convert cluster number to sector number
uint64_t cluster_to_sector(uint32_t cluster) {
    if (cluster < 2) return 0;
    return g_data_region_sector +
           static_cast<uint64_t>(cluster - 2) * g_sectors_per_cluster;
}

// Read a FAT entry (32-bit) for a cluster
uint32_t read_fat_entry(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = g_reserved_sectors + (fat_offset / g_bytes_per_sector);
    uint32_t offset_in_sector = fat_offset % g_bytes_per_sector;

    if (!read_sector(fat_sector)) return 0;

    uint32_t* entry = reinterpret_cast<uint32_t*>(g_sector_cache + offset_in_sector);
    return *entry & 0x0FFFFFFF;
}

// Read a full cluster into buffer
bool read_cluster(uint32_t cluster, void* buffer) {
    uint64_t sector = cluster_to_sector(cluster);
    if (sector == 0) return false;

    for (uint32_t i = 0; i < g_sectors_per_cluster; i++) {
        if (!g_block_read(sector + i,
            static_cast<uint8_t*>(buffer) + i * g_bytes_per_sector, 1)) {
            return false;
        }
    }
    return true;
}

// Follow cluster chain starting from start_cluster, calling callback for each cluster
bool follow_chain(uint32_t start_cluster,
                  bool (*callback)(uint32_t cluster, void* ctx),
                  void* ctx) {
    uint32_t cluster = start_cluster;
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        if (!callback(cluster, ctx)) return false;
        cluster = read_fat_entry(cluster);
    }
    return true;
}

// Context for directory search
struct dir_search_ctx {
    const char* target_name;
    uint32_t*   out_cluster;
    uint64_t*   out_size;
    bool        found;
};

// Parse directory entries from a cluster buffer
bool parse_dir_entries(const uint8_t* cluster_buf, uint32_t cluster_size,
                       dir_search_ctx* ctx) {
    uint32_t offset = 0;
    while (offset + 32 <= cluster_size) {
        const dir_entry* de =
            reinterpret_cast<const dir_entry*>(cluster_buf + offset);
        offset += 32;

        if (de->name[0] == DIR_ENTRY_FREE) break;
        if (de->name[0] == DIR_ENTRY_DELETED) continue;
        if ((de->attr & ATTR_LONG_NAME) == ATTR_LONG_NAME) continue;

        // Extract name (8.3 format)
        char name[13];
        uint32_t j = 0;
        for (uint32_t i = 0; i < 8 && de->name[i] != ' '; i++) {
            name[j++] = de->name[i];
        }
        if (de->name[8] != ' ') {
            name[j++] = '.';
            for (uint32_t i = 8; i < 11 && de->name[i] != ' '; i++) {
                name[j++] = de->name[i];
            }
        }
        name[j] = 0;

        if (ctx->target_name && lib::strcmp(name, ctx->target_name) == 0) {
            *ctx->out_cluster =
                (static_cast<uint32_t>(de->first_cluster_high) << 16) |
                de->first_cluster_low;
            *ctx->out_size = de->file_size;
            ctx->found = true;
            return false;  // stop following chain
        }
    }
    return true;  // continue following chain
}

// Callback for follow_chain during directory search
bool dir_search_callback(uint32_t cluster, void* ctx) {
    dir_search_ctx* search = reinterpret_cast<dir_search_ctx*>(ctx);
    uint8_t buf[4096];
    if (!read_cluster(cluster, buf)) return false;
    return parse_dir_entries(buf, g_bytes_per_cluster, search);
}

// Find a file/directory by name within a directory cluster chain
bool find_in_dir(uint32_t start_cluster, const char* target_name,
                 uint32_t& out_cluster, uint64_t& out_size) {
    dir_search_ctx ctx;
    ctx.target_name = target_name;
    ctx.out_cluster = &out_cluster;
    ctx.out_size    = &out_size;
    ctx.found       = false;
    return follow_chain(start_cluster, dir_search_callback, &ctx);
}

// File handle structure
struct file_handle_t {
    uint32_t cluster;
    uint64_t size;
    bool     is_dir;
};

} // anonymous namespace

bool mount(bool (*block_read)(uint64_t sector, void* buffer, uint32_t count)) {
    g_block_read = block_read;

    if (!read_sector(0)) return false;

    mem_copy(&g_bpb, g_sector_cache, sizeof(bpb_t));

    if (g_bpb.boot_signature != 0x29 && g_bpb.boot_signature != 0x28) return false;

    g_bytes_per_sector = g_bpb.bytes_per_sector;
    g_sectors_per_cluster = g_bpb.sectors_per_cluster;
    g_bytes_per_cluster = g_bytes_per_sector * g_sectors_per_cluster;
    g_reserved_sectors = g_bpb.reserved_sector_count;
    g_num_fats = g_bpb.num_fats;
    g_fat_size_sectors = g_bpb.fat_size32;
    g_root_cluster = g_bpb.root_cluster;

    g_data_region_sector = g_reserved_sectors + (g_num_fats * g_fat_size_sectors);

    return true;
}

void unmount() {
    g_block_read = nullptr;
}

void* lookup(const char* path) {
    if (!path || path[0] == 0) return nullptr;

    uint32_t cluster = g_root_cluster;

    const char* p = path;
    if (*p == '/') p++;

    while (*p) {
        while (*p == '/') p++;
        if (*p == 0) break;

        char component[13];
        uint32_t len = 0;
        while (*p && *p != '/' && len < sizeof(component) - 1) {
            component[len++] = *p++;
        }
        component[len] = 0;

        uint32_t found_cluster = 0;
        uint64_t found_size = 0;
        if (!find_in_dir(cluster, component, found_cluster, found_size)) return nullptr;

        cluster = found_cluster;
    }

    file_handle_t* fh = reinterpret_cast<file_handle_t*>(
        mm::kmalloc(sizeof(file_handle_t)));
    if (!fh) return nullptr;
    fh->cluster = cluster;
    fh->size = 0;
    fh->is_dir = false;

    uint8_t buf[4096];
    if (read_cluster(cluster, buf)) {
        const dir_entry* de =
            reinterpret_cast<const dir_entry*>(buf);
        fh->is_dir = (cluster == g_root_cluster) ||
                     ((de->attr & ATTR_SUBDIR) != 0);
        fh->size = de->file_size;
    }

    return fh;
}

uint32_t read_file(void* file_handle, void* buffer, uint32_t size, uint32_t offset) {
    file_handle_t* fh = reinterpret_cast<file_handle_t*>(file_handle);
    if (!fh || !buffer || size == 0) return 0;
    if (fh->is_dir) return 0;

    uint8_t* dst = reinterpret_cast<uint8_t*>(buffer);
    uint32_t remaining = size;
    uint32_t cur_offset = offset;
    uint32_t cluster = fh->cluster;

    uint8_t cluster_buf[4096];

    while (remaining > 0 && cluster >= 2 && cluster < 0x0FFFFFF8) {
        if (!read_cluster(cluster, cluster_buf)) break;

        uint32_t cluster_offset = cur_offset % g_bytes_per_cluster;
        uint32_t copy = (remaining < g_bytes_per_cluster - cluster_offset)
            ? remaining : (g_bytes_per_cluster - cluster_offset);

        mem_copy(dst, cluster_buf + cluster_offset, copy);
        dst += copy;
        remaining -= copy;
        cur_offset += copy;

        cluster = read_fat_entry(cluster);
    }

    return size - remaining;
}

uint64_t get_file_size(void* file_handle) {
    file_handle_t* fh = reinterpret_cast<file_handle_t*>(file_handle);
    if (!fh) return 0;
    return fh->size;
}

bool is_directory(void* file_handle) {
    file_handle_t* fh = reinterpret_cast<file_handle_t*>(file_handle);
    if (!fh) return false;
    return fh->is_dir;
}

} // namespace feather::fs::fat32
