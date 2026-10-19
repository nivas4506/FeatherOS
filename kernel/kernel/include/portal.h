#pragma once
// PORTAL Kernel Syscall Interface
// Matches userspace portal.h exactly

#include <cstdint>
#include <cstddef>

namespace feather::kernel::portal {

// ──────────────────────────────────────────────────────────────
// Opaque handle type
// ──────────────────────────────────────────────────────────────
using portal_handle_t = uint64_t;
constexpr portal_handle_t PORTAL_HANDLE_INVALID = static_cast<portal_handle_t>(-1);

// ──────────────────────────────────────────────────────────────
// Message structure
// ──────────────────────────────────────────────────────────────
constexpr uint32_t PORTAL_MAX_MESSAGE_SIZE = 8 * 1024 * 1024;

struct portal_message_t {
    uint32_t length;
    uint8_t  data[]; // Flexible array member
};

// ──────────────────────────────────────────────────────────────
// Capability rights
// ──────────────────────────────────────────────────────────────
using portal_caps_t = uint32_t;
constexpr portal_caps_t PORTAL_CAP_NONE           = 0x00000000;
constexpr portal_caps_t PORTAL_CAP_FS_READ        = 0x00000001;
constexpr portal_caps_t PORTAL_CAP_FS_WRITE       = 0x00000002;
constexpr portal_caps_t PORTAL_CAP_FS_CREATE      = 0x00000004;
constexpr portal_caps_t PORTAL_CAP_FS_DELETE      = 0x00000008;
constexpr portal_caps_t PORTAL_CAP_PROC_SPAWN     = 0x00000010;
constexpr portal_caps_t PORTAL_CAP_PROC_KILL      = 0x00000020;
constexpr portal_caps_t PORTAL_CAP_IPC_CONNECT    = 0x00000040;
constexpr portal_caps_t PORTAL_CAP_IPC_LISTEN     = 0x00000080;
constexpr portal_caps_t PORTAL_CAP_TELEMETRY      = 0x00000100;
constexpr portal_caps_t PORTAL_CAP_ALL            = 0xFFFFFFFF;

// ──────────────────────────────────────────────────────────────
// Syscall numbers (matching userspace)
// ──────────────────────────────────────────────────────────────
enum class Syscall : uint64_t {
    // Existing
    SYS_PUTCHAR    = 0,
    SYS_PUTS       = 1,
    SYS_GETCHAR    = 2,
    SYS_EXIT       = 3,
    SYS_YIELD      = 4,
    
    // PORTAL syscalls (0x100-0x10F)
    SYS_PORTAL_CREATE         = 0x100,
    SYS_PORTAL_CONNECT        = 0x101,
    SYS_PORTAL_SEND           = 0x102,
    SYS_PORTAL_RECV           = 0x103,
    SYS_PORTAL_CLOSE          = 0x104,
    SYS_PORTAL_SPAWN_RESTRICTED = 0x105,
    SYS_PORTAL_WAIT           = 0x106,
    SYS_PORTAL_KILL           = 0x107,
    SYS_PORTAL_GET_TELEMETRY  = 0x108,
    SYS_PORTAL_FS_OP          = 0x109,
};

// ──────────────────────────────────────────────────────────────
// Telemetry structure
// ──────────────────────────────────────────────────────────────
struct portal_telemetry_t {
    char kernel_version[64];
    char architecture[32];
    uint32_t cpu_cores;
    float cpu_usage_pct;
    uint64_t mem_total_mb;
    uint64_t mem_available_mb;
    uint64_t disk_available_gb;
};

// ──────────────────────────────────────────────────────────────
// Filesystem operations
// ──────────────────────────────────────────────────────────────
enum class portal_fs_op_t : uint32_t {
    PORTAL_FS_READ    = 0,
    PORTAL_FS_WRITE   = 1,
    PORTAL_FS_OPEN    = 2,
    PORTAL_FS_CLOSE   = 3,
    PORTAL_FS_MKDIR   = 4,
    PORTAL_FS_UNLINK  = 5,
    PORTAL_FS_READDIR = 6,
    PORTAL_FS_STAT    = 7,
    PORTAL_FS_RENAME  = 8,
};

struct portal_fs_request_t {
    portal_fs_op_t op;
    char path[4096];
    union {
        struct { uint64_t offset; uint64_t length; uint8_t* data; } read;
        struct { uint64_t offset; uint64_t length; const uint8_t* data; } write;
        struct { int flags; int mode; } open;
        struct { int fd; } close;
        struct { int mode; } mkdir;
        struct { int fd; uint8_t* buf; size_t count; } readdir;
        struct { int fd; } stat;
        struct { char new_path[4096]; } rename;
    };
    // Output fields
    union {
        int fd;
        uint64_t bytes_transferred;
        struct { uint64_t size; int is_dir; } stat_result;
    };
};

// ──────────────────────────────────────────────────────────────
// Portal object (kernel-side)
// ──────────────────────────────────────────────────────────────
struct Portal {
    portal_handle_t handle;
    portal_caps_t caps;
    bool is_server;          // true if created via portal_create (listening)
    const char* name;        // Portal name (for servers)
    
    // Message queue
    struct Message {
        portal_message_t* msg;
        portal_handle_t* cap_handles;
        size_t cap_count;
    };
    
    // For server portals: list of connected client portals
    // For client portals: pointer to connected server portal
    Portal* connected_portal = nullptr;
    
    // Reference counting
    uint32_t ref_count = 1;
};

// ──────────────────────────────────────────────────────────────
// Portal subsystem functions
// ──────────────────────────────────────────────────────────────

// Initialize portal subsystem
void portal_init();

// Syscall handlers (called from syscall_handler)
int64_t portal_sys_create(const char* name, portal_caps_t caps, portal_handle_t* out_handle);
int64_t portal_sys_connect(const char* name, portal_caps_t caps, portal_handle_t* out_handle);
int64_t portal_sys_send(portal_handle_t handle, const portal_message_t* msg,
                        const portal_handle_t* cap_handles, size_t cap_count);
int64_t portal_sys_recv(portal_handle_t handle, portal_message_t* msg_buf,
                        portal_handle_t* cap_handles, size_t cap_capacity, size_t* out_cap_count);
int64_t portal_sys_close(portal_handle_t handle);
int64_t portal_sys_spawn_restricted(const char* cmd, const char* cwd,
                                    portal_caps_t allowed_caps,
                                    uint32_t* out_pid, portal_handle_t* out_portal);
int64_t portal_sys_wait(uint32_t pid, int* out_exit_code);
int64_t portal_sys_kill(uint32_t pid, int signal);
int64_t portal_sys_get_telemetry(portal_telemetry_t* out);
int64_t portal_sys_fs_op(portal_fs_request_t* req);

// Helper: get current process's portal table
struct PortalTable* get_current_portal_table();

} // namespace feather::kernel::portal