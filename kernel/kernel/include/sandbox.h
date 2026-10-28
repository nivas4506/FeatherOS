#pragma once
// FeatherOS Kernel — Sandboxed Execution Domain
// Sandboxed execution domains (ring 3 + seccomp-like filters) for
// aegis-exec safety engine. Provides syscall filtering and memory
// domain isolation for untrusted cognitive agents.

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>
#include <cstddef>

namespace feather::kernel::sandbox {

// ──────────────────────────────────────────────────────────────
// Sandbox Policies
// ──────────────────────────────────────────────────────────────

// Sandbox domain ID
using sandbox_id_t = uint32_t;

// Sandbox rule action
enum class sandbox_action : uint8_t {
    SANDBOX_ALLOW   = 0,  // Allow syscall
    SANDBOX_DENY    = 1,  // Deny syscall (return -EPERM)
    SANDBOX_LOG     = 2,  // Log and allow
    SANDBOX_TRAP    = 3,  // Trap to userspace handler
    SANDBOX_KILL    = 4,  // Kill the task
};

// Syscall filter rule
struct sandbox_rule {
    uint64_t syscall_num;   // Syscall number (0 = match all)
    sandbox_action action;  // Action to take
    uint64_t arg_mask;      // Argument mask for matching
    uint64_t arg_value;     // Argument value to match
};

// Sandbox domain configuration
struct sandbox_domain {
    sandbox_id_t id;                // Domain ID
    uint64_t     flags;             // Domain flags
    uint64_t     max_memory;        // Max memory allocation (bytes)
    uint64_t     max_cpu_time_ns;   // Max CPU time (nanoseconds)
    uint32_t     max_fds;           // Max file descriptors
    bool         allow_network;     // Allow network access
    bool         allow_filesystem;  // Allow filesystem access
    bool         allow_spawn;       // Allow process spawning
    bool         allow_mmap_exec;   // Allow executable memory mappings
};

// Sandbox violation event
struct sandbox_violation {
    sandbox_id_t domain_id;   // Domain that violated
    uint64_t     task_id;     // Task ID
    uint64_t     syscall_num; // Syscall that was blocked
    uint64_t     arg1;        // Syscall arg 1
    uint64_t     arg2;        // Syscall arg 2
    uint64_t     timestamp;   // Timestamp of violation
};

// ──────────────────────────────────────────────────────────────
// Public API
// ──────────────────────────────────────────────────────────────

// Initialize sandbox subsystem
void sandbox_init();

// Create a new sandbox domain
sandbox_id_t sandbox_create(const sandbox_domain* config);

// Destroy a sandbox domain
void sandbox_destroy(sandbox_id_t id);

// Add a syscall rule to a domain
int sandbox_add_rule(sandbox_id_t id, const sandbox_rule* rule);

// Remove a syscall rule from a domain
int sandbox_remove_rule(sandbox_id_t id, uint64_t syscall_num);

// Check if a syscall is allowed in a domain
bool sandbox_check(uint64_t syscall_num, uint64_t arg1, uint64_t arg2,
                   uint64_t arg3, uint64_t arg4, uint64_t arg5,
                   sandbox_action* action);

// Check memory access permission
bool sandbox_check_mem(sandbox_id_t id, void* addr, uint64_t size, bool write);

// Check file access permission
bool sandbox_check_file(sandbox_id_t id, const char* path, bool write);

// Get domain configuration
bool sandbox_get_config(sandbox_id_t id, sandbox_domain* config);

// Register violation callback
typedef void (*violation_cb_t)(const sandbox_violation* v);
void sandbox_register_violation_cb(violation_cb_t cb);

// Get violation count for a domain
uint64_t sandbox_get_violations(sandbox_id_t id);

} // namespace feather::kernel::sandbox
