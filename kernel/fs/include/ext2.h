#pragma once
// FeatherOS Kernel — ext2 / ext3 / ext4 filesystem driver
// Mounts an ext2-family filesystem on a block device and exposes it via VFS.

#ifdef _MSC_VER
#include <intrin.h>
#pragma pack(push, 1)
#endif
#include <cstddef>
#include <cstdint>

namespace feather::fs::ext2 {

// ext2 superblock (located at offset 1024 in the filesystem)
struct superblock {
    uint32_t inodes_count;
    uint32_t blocks_count;
    uint32_t reserved_blocks_count;
    uint32_t free_blocks_count;
    uint32_t free_inodes_count;
    uint32_t first_data_block;
    uint32_t log_block_size;
    uint32_t log_cluster_size;
    uint32_t blocks_per_group;
    uint32_t clusters_per_group;
    uint32_t inodes_per_group;
    uint32_t mtime;
    uint32_t wtime;
    uint16_t mnt_count;
    uint16_t max_mnt_count;
    uint16_t magic;
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;
    uint32_t lastcheck;
    uint32_t check_interval;
    uint32_t creator_os;
    uint32_t rev_level;
    uint16_t def_resuid;
    uint16_t def_resgid;
} ;

// ext2 block group descriptor
struct block_group_desc {
    uint32_t block_bitmap_block;
    uint32_t inode_bitmap_block;
    uint32_t inode_table_block;
    uint16_t free_blocks_count;
    uint16_t free_inodes_count;
    uint16_t used_dirs_count;
    uint16_t flags;
    uint8_t  reserved[12];
};

// ext2 inode (128 bytes for rev 0, 256 bytes for rev 1+)
struct inode {
    uint16_t mode;
    uint16_t uid;
    uint32_t size_lo;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t links_count;
    uint32_t blocks_lo;
    uint32_t flags;
    uint32_t osd1;
    uint32_t block[15];    // 12 direct + 1 indirect + 1 double indirect + 1 triple indirect
    uint32_t generation;
    uint32_t file_acl_lo;
    uint32_t dir_acl_lo;
    uint32_t faddr_lo;
    uint8_t  osd2[12];
};

// ext2 directory entry
struct dir_entry {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[256];  // max name length
};

// File type values
enum file_type : uint8_t {
    FT_UNKNOWN   = 0,
    FT_REG_FILE  = 1,
    FT_DIR       = 2,
    FT_CHRDEV    = 3,
    FT_BLKDEV    = 4,
    FT_FIFO      = 5,
    FT_SOCK      = 6,
    FT_SYMLINK   = 7,
};

// ext2 magic number
constexpr uint16_t EXT2_SUPER_MAGIC = 0xEF53;

// Inode mode constants
constexpr uint16_t EXT2_S_IFREG  = 0x8000;
constexpr uint16_t EXT2_S_IFDIR  = 0x4000;
constexpr uint16_t EXT2_S_IFCHR  = 0x2000;
constexpr uint16_t EXT2_S_IFBLK  = 0x6000;
constexpr uint16_t EXT2_S_IFIFO  = 0x1000;
constexpr uint16_t EXT2_S_IFSOCK = 0xC000;
constexpr uint16_t EXT2_S_IFLNK  = 0xA000;

// Mount ext2 filesystem on a block device
// block_read: callback to read 512-byte sectors (sector, buffer, count) -> bool
// Returns true on success
bool mount(bool (*block_read)(uint64_t sector, void* buffer, uint32_t count));

// Unmount the filesystem
void unmount();

// Look up a path within the ext2 filesystem
// Returns a heap-allocated inode handle (opaque void*), or nullptr on failure
void* lookup(const char* path);

// Read file data into buffer
// Returns number of bytes read
uint32_t read_file(void* inode_ptr, void* buffer, uint32_t size, uint32_t offset);

// Get file size in bytes
uint64_t get_file_size(void* inode_ptr);

// Check if inode is a directory
bool is_directory(void* inode_ptr);

#ifdef _MSC_VER
#pragma pack(pop)
#endif

} // namespace feather::fs::ext2
