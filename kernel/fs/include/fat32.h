#pragma once
// FeatherOS Kernel — FAT32 filesystem driver
// Mounts a FAT32 volume on a block device and exposes it via VFS.

#ifdef _MSC_VER
#include <intrin.h>
#pragma pack(push, 1)
#endif
#include <cstddef>
#include <cstdint>

namespace feather::fs::fat32 {

// FAT32 BIOS Parameter Block (in boot sector)
struct bpb_t {
    uint8_t  jmp_boot[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t  num_fats;
    uint16_t root_entry_count;   // 0 for FAT32
    uint16_t total_sectors16;    // 0 for FAT32
    uint8_t  media;
    uint16_t fat_size16;         // 0 for FAT32
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors32;
    // FAT32 specific:
    uint32_t fat_size32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fsinfo_sector;
    uint16_t backup_boot_sector;
    uint8_t  reserved2[12];
    uint8_t  drive_number;
    uint8_t  reserved3;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fat_type_label[8];
} ;

// FAT32 directory entry
struct dir_entry {
    char     name[11];
    uint8_t  attr;
    uint8_t  reserved_nt;
    uint8_t  creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
};

// File attributes
enum fat_attr : uint8_t {
    ATTR_READ_ONLY = 0x01,
    ATTR_HIDDEN    = 0x02,
    ATTR_SYSTEM    = 0x04,
    ATTR_VOLUME_ID = 0x08,
    ATTR_SUBDIR    = 0x10,
    ATTR_ARCHIVE   = 0x20,
    ATTR_LONG_NAME = 0x0F,
};

// Directory entry name terminators
constexpr uint8_t DIR_ENTRY_FREE     = 0x00;
constexpr uint8_t DIR_ENTRY_DELETED  = 0xE5;

// Mount FAT32 volume
bool mount(bool (*block_read)(uint64_t sector, void* buffer, uint32_t count));
void unmount();

// Look up a path
void* lookup(const char* path);

// Read file data
uint32_t read_file(void* file_handle, void* buffer, uint32_t size, uint32_t offset);

// Get file size
uint64_t get_file_size(void* file_handle);

// Check if directory
bool is_directory(void* file_handle);

#ifdef _MSC_VER
#pragma pack(pop)
#endif

} // namespace feather::fs::fat32
