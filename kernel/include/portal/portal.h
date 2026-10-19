/*
 * Portal Kernel API — Minimal IPC & Capability Interface for FeatherOS
 * 
 * This header defines the userspace-facing syscall interface for the Portal kernel.
 * Kernel-side implementation should provide these syscalls.
 * 
 * Syscall numbers (adjust for your kernel's syscall table):
 *   0x100 - portal_create
 *   0x101 - portal_connect
 *   0x102 - portal_send
 *   0x103 - portal_recv
 *   0x104 - portal_close
 *   0x105 - portal_spawn_restricted
 *   0x106 - portal_get_telemetry
 *   0x107 - portal_fs_op
 */

#ifndef PORTAL_PORTAL_H
#define PORTAL_PORTAL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──────────────────────────────────────────────────────────────
 * Opaque handle type — kernel manages the actual object
 * ────────────────────────────────────────────────────────────── */
typedef uint64_t portal_handle_t;
#define PORTAL_HANDLE_INVALID  ((portal_handle_t)-1)

/* ──────────────────────────────────────────────────────────────
 * Message structure — matches FeatherOS wire format:
 *   4-byte big-endian length + UTF-8 JSON-RPC 2.0 body
 * ────────────────────────────────────────────────────────────── */
#define PORTAL_MAX_MESSAGE_SIZE  (8 * 1024 * 1024)  // 8 MB

typedef struct {
    uint32_t length;        // Big-endian payload length
    uint8_t  data[];        // Flexible array member (payload)
} portal_message_t;

/* ──────────────────────────────────────────────────────────────
 * Capability rights (bitmask) — for future fd/shmem passing
 * ────────────────────────────────────────────────────────────── */
typedef uint32_t portal_caps_t;
#define PORTAL_CAP_NONE           0x00000000
#define PORTAL_CAP_FS_READ        0x00000001
#define PORTAL_CAP_FS_WRITE       0x00000002
#define PORTAL_CAP_FS_CREATE      0x00000004
#define PORTAL_CAP_FS_DELETE      0x00000008
#define PORTAL_CAP_PROC_SPAWN     0x00000010
#define PORTAL_CAP_PROC_KILL      0x00000020
#define PORTAL_CAP_IPC_CONNECT    0x00000040
#define PORTAL_CAP_IPC_LISTEN     0x00000080
#define PORTAL_CAP_TELEMETRY      0x00000100
#define PORTAL_CAP_ALL            0xFFFFFFFF

/* ──────────────────────────────────────────────────────────────
 * Portal creation / connection
 * ────────────────────────────────────────────────────────────── */

/**
 * portal_create - Create a new portal (server endpoint)
 * @name: Null-terminated portal name (max 255 chars), or NULL for anonymous
 * @caps: Initial capability set for this portal
 * @out_handle: Receives the portal handle on success
 * Returns: 0 on success, negative errno on failure
 * 
 * Kernel: Allocates portal object, registers name in global namespace if provided.
 */
int portal_create(const char* name, portal_caps_t caps, portal_handle_t* out_handle);

/**
 * portal_connect - Connect to a named portal (client)
 * @name: Portal name to connect to
 * @caps: Requested capabilities (kernel validates against server's caps)
 * @out_handle: Receives connected portal handle on success
 * Returns: 0 on success, negative errno on failure
 * 
 * Kernel: Looks up portal by name, checks capability intersection, creates connection.
 */
int portal_connect(const char* name, portal_caps_t caps, portal_handle_t* out_handle);

/* ──────────────────────────────────────────────────────────────
 * Message passing
 * ────────────────────────────────────────────────────────────── */

/**
 * portal_send - Send a message (blocking)
 * @handle: Portal handle
 * @msg: Message to send (length + payload)
 * @cap_handles: Optional array of capability handles to transfer (or NULL)
 * @cap_count: Number of capability handles in array
 * Returns: 0 on success, negative errno on failure
 * 
 * Kernel: Copies message to receiver's queue, transfers capabilities.
 * Blocks until receiver has buffer space (or use non-blocking variant).
 */
int portal_send(portal_handle_t handle, const portal_message_t* msg,
                const portal_handle_t* cap_handles, size_t cap_count);

/**
 * portal_recv - Receive a message (blocking)
 * @handle: Portal handle
 * @msg_buf: Buffer to receive message (must be >= PORTAL_MAX_MESSAGE_SIZE)
 * @cap_handles: Optional array to receive transferred capabilities (or NULL)
 * @cap_capacity: Capacity of cap_handles array
 * @out_cap_count: Receives actual number of capabilities received
 * Returns: 0 on success, negative errno on failure (ENOTCONN if peer closed)
 * 
 * Kernel: Blocks until message available. Copies message + capabilities.
 */
int portal_recv(portal_handle_t handle, portal_message_t* msg_buf,
                portal_handle_t* cap_handles, size_t cap_capacity, size_t* out_cap_count);

/* ──────────────────────────────────────────────────────────────
 * Lifecycle
 * ────────────────────────────────────────────────────────────── */

/**
 * portal_close - Close a portal handle
 * @handle: Portal handle to close
 * Returns: 0 on success, negative errno on failure
 * 
 * Kernel: Decrements refcount, destroys portal when both ends closed.
 */
int portal_close(portal_handle_t handle);

/* ──────────────────────────────────────────────────────────────
 * Restricted process spawning (sandboxing primitive)
 * ────────────────────────────────────────────────────────────── */

/**
 * portal_spawn_restricted - Spawn a child process with restricted portal namespace
 * @cmd: Null-terminated command line (argv[0] + args)
 * @cwd: Working directory path (null-terminated)
 * @allowed_caps: Capability mask for the child's default portal
 * @out_pid: Receives child process ID on success
 * @out_portal: Receives portal handle to communicate with child (or PORTAL_HANDLE_INVALID)
 * Returns: 0 on success, negative errno on failure
 * 
 * Kernel: Creates new process with:
 *   - New portal namespace (only inherits explicitly allowed portals)
 *   - Default portal with @allowed_caps
 *   - Resource limits (CPU, memory, file size) from kernel config
 *   - No portal_create/portal_connect unless PORTAL_CAP_IPC_* in allowed_caps
 */
int portal_spawn_restricted(const char* cmd, const char* cwd,
                            portal_caps_t allowed_caps,
                            uint32_t* out_pid, portal_handle_t* out_portal);

/**
 * portal_wait - Wait for child process termination
 * @pid: Process ID from portal_spawn_restricted
 * @out_exit_code: Receives exit code
 * Returns: 0 on success, negative errno on failure
 */
int portal_wait(uint32_t pid, int* out_exit_code);

/**
 * portal_kill - Send signal to process
 * @pid: Process ID
 * @signal: Signal number (SIGTERM=15, SIGKILL=9, etc.)
 * Returns: 0 on success, negative errno on failure
 */
int portal_kill(uint32_t pid, int signal);

/* ──────────────────────────────────────────────────────────────
 * Telemetry (kernel → userspace)
 * ────────────────────────────────────────────────────────────── */

typedef struct {
    char kernel_version[64];
    char architecture[32];
    uint32_t cpu_cores;
    float cpu_usage_pct;
    uint64_t mem_total_mb;
    uint64_t mem_available_mb;
    uint64_t disk_available_gb;
} portal_telemetry_t;

/**
 * portal_get_telemetry - Sample system telemetry
 * @out: Receives telemetry snapshot
 * Returns: 0 on success, negative errno on failure
 */
int portal_get_telemetry(portal_telemetry_t* out);

/* ──────────────────────────────────────────────────────────────
 * Filesystem operations via portal (for VFS backend)
 * ────────────────────────────────────────────────────────────── */

typedef enum {
    PORTAL_FS_READ = 0,
    PORTAL_FS_WRITE = 1,
    PORTAL_FS_OPEN = 2,
    PORTAL_FS_CLOSE = 3,
    PORTAL_FS_MKDIR = 4,
    PORTAL_FS_UNLINK = 5,
    PORTAL_FS_READDIR = 6,
    PORTAL_FS_STAT = 7,
    PORTAL_FS_RENAME = 8,
} portal_fs_op_t;

typedef struct {
    portal_fs_op_t op;
    char path[4096];
    union {
        struct { uint64_t offset; uint64_t length; } read;
        struct { uint64_t offset; uint64_t length; const uint8_t* data; } write;
        struct { int flags; int mode; } open;
        struct { int fd; } close;
        struct { int mode; } mkdir;
        struct { int fd; uint8_t* buf; size_t count; } readdir;
        struct { int fd; } stat;
        struct { char new_path[4096]; } rename;
    };
    // Output fields (kernel fills on success)
    union {
        int fd;                    // For open
        uint64_t bytes_transferred; // For read/write
        struct { uint64_t size; int is_dir; } stat_result;
    };
} portal_fs_request_t;

/**
 * portal_fs_op - Perform filesystem operation via syscall portal
 * @req: Request structure (input + output)
 * Returns: 0 on success, negative errno on failure
 * 
 * Kernel: Validates caller's PORTAL_CAP_FS_* caps, performs operation.
 * All paths are resolved relative to caller's working directory.
 */
int portal_fs_op(portal_fs_request_t* req);

#ifdef __cplusplus
}
#endif

#endif // PORTAL_PORTAL_H