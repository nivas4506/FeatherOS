// PORTAL Kernel Subsystem — IPC, Process Isolation, Capability-based Security
// Implements the portal syscall interface for FeatherOS

#include "portal.h"
#include "../../lib/include/string.h"
#include "../../lib/include/printf.h"
#include "../../task/include/task.h"
#include "../../mm/include/paging.h"
#include "../../mm/include/heap.h"
#include "../../fs/include/vfs.h"

namespace feather::kernel::portal {

// ──────────────────────────────────────────────────────────────
// Simple atomic (MSVC uses volatile, GCC uses __atomic builtins)
// ──────────────────────────────────────────────────────────────
#ifdef _MSC_VER
template<typename T>
struct Atomic {
    volatile T value;

    Atomic() : value(0) {}
    explicit Atomic(T v) : value(v) {}

    T load() const { return value; }
    void store(T v) { value = v; }
    T fetch_add(T v) {
        T old = value;
        value = static_cast<T>(old + v);
        return old;
    }
    T operator++() { T old = value; value = static_cast<T>(old + 1); return old + 1; }
    T operator++(int) { T old = value; value = static_cast<T>(old + 1); return old; }
};
#else
template<typename T>
struct Atomic {
    T value;

    T load() const { return __atomic_load_n(&value, __ATOMIC_SEQ_CST); }
    void store(T v) { __atomic_store_n(&value, v, __ATOMIC_SEQ_CST); }
    T fetch_add(T v) { return __atomic_fetch_add(&value, v, __ATOMIC_SEQ_CST); }
    T operator++() { return fetch_add(1) + 1; }
    T operator++(int) { return fetch_add(1); }
};
#endif

// ──────────────────────────────────────────────────────────────
// Global state
// ──────────────────────────────────────────────────────────────
static Atomic<portal_handle_t> g_next_handle{1};
static Atomic<uint32_t> g_next_pid{1000};

// Portal table per process (max 256 portals per process)
constexpr size_t MAX_PORTALS_PER_PROCESS = 256;

struct PortalTable {
    Portal* portals[MAX_PORTALS_PER_PROCESS] = {nullptr};
    
    Portal* get(portal_handle_t handle) {
        if (handle >= MAX_PORTALS_PER_PROCESS) return nullptr;
        return portals[handle];
    }
    
    portal_handle_t allocate(Portal* p) {
        for (size_t i = 1; i < MAX_PORTALS_PER_PROCESS; ++i) { // 0 reserved
            if (portals[i] == nullptr) {
                portals[i] = p;
                return static_cast<portal_handle_t>(i);
            }
        }
        return PORTAL_HANDLE_INVALID;
    }
    
    void free(portal_handle_t handle) {
        if (handle < MAX_PORTALS_PER_PROCESS) {
            portals[handle] = nullptr;
        }
    }
};

// Per-process portal table (indexed by task ID)
static PortalTable* g_portal_tables[4096] = {nullptr}; // Max 4096 processes

PortalTable* get_current_portal_table() {
    auto* current = task::task_current();
    if (!current) return nullptr;
    
    uint64_t tid = current->id;
    if (tid >= 4096) return nullptr;
    
    if (!g_portal_tables[tid]) {
        g_portal_tables[tid] = static_cast<PortalTable*>(mm::kmalloc(sizeof(PortalTable)));
        lib::memset(g_portal_tables[tid], 0, sizeof(PortalTable));
    }
    return g_portal_tables[tid];
}

// ──────────────────────────────────────────────────────────────
// Portal object management
// ──────────────────────────────────────────────────────────────

static char* kstrdup(const char* s) {
    if (!s) return nullptr;
    size_t len = lib::strlen(s) + 1;
    char* copy = static_cast<char*>(mm::kmalloc(len));
    if (copy) lib::strcpy(copy, s);
    return copy;
}

Portal* create_portal(portal_caps_t caps, bool is_server, const char* name) {
    auto* p = static_cast<Portal*>(mm::kmalloc(sizeof(Portal)));
    if (!p) return nullptr;
    lib::memset(p, 0, sizeof(Portal));
    
    p->handle = ++g_next_handle;
    p->caps = caps;
    p->is_server = is_server;
    p->name = name ? kstrdup(name) : nullptr;
    p->ref_count = 1;
    return p;
}

void destroy_portal(Portal* p) {
    if (!p) return;
    if (--p->ref_count > 0) return;
    
    if (p->name) mm::kfree(const_cast<void*>(static_cast<const void*>(p->name)));
    
    // Clean up connected portal reference
    if (p->connected_portal) {
        p->connected_portal->connected_portal = nullptr;
    }
    
    mm::kfree(p);
}

Portal* lookup_portal(portal_handle_t handle) {
    auto* table = get_current_portal_table();
    if (!table) return nullptr;
    return table->get(handle);
}

// ──────────────────────────────────────────────────────────────
// Portal registry (for named portals)
// ──────────────────────────────────────────────────────────────

struct NamedPortalEntry {
    const char* name;
    Portal* portal;
    NamedPortalEntry* next = nullptr;
};

static NamedPortalEntry* g_named_portals = nullptr;

Portal* find_named_portal(const char* name) {
    for (auto* e = g_named_portals; e; e = e->next) {
        if (lib::strcmp(e->name, name) == 0) {
            return e->portal;
        }
    }
    return nullptr;
}

void register_named_portal(const char* name, Portal* portal) {
    auto* entry = static_cast<NamedPortalEntry*>(mm::kmalloc(sizeof(NamedPortalEntry)));
    if (!entry) return;
    lib::memset(entry, 0, sizeof(NamedPortalEntry));
    entry->name = kstrdup(name);
    entry->portal = portal;
    entry->next = g_named_portals;
    g_named_portals = entry;
}

void unregister_named_portal(const char* name) {
    NamedPortalEntry** prev = &g_named_portals;
    for (auto* e = g_named_portals; e; e = e->next) {
        if (lib::strcmp(e->name, name) == 0) {
            *prev = e->next;
            mm::kfree(const_cast<char*>(e->name));
            mm::kfree(e);
            return;
        }
        prev = &e->next;
    }
}

// ──────────────────────────────────────────────────────────────
// Message queue (simplified ring buffer per portal)
// ──────────────────────────────────────────────────────────────

struct MessageQueue {
    static constexpr size_t CAPACITY = 32;
    struct Entry {
        portal_message_t* msg = nullptr;
        portal_handle_t cap_handles[4];
        size_t cap_count = 0;
        bool valid = false;
    };
    Entry entries[CAPACITY];
    size_t head = 0, tail = 0, count = 0;
    
    bool push(portal_message_t* msg, const portal_handle_t* caps, size_t cap_count) {
        if (count >= CAPACITY) return false;
        entries[tail].msg = msg;
        if (caps && cap_count > 0) {
            cap_count = (cap_count > 4) ? 4 : cap_count;
            lib::memcpy(entries[tail].cap_handles, caps, cap_count * sizeof(portal_handle_t));
        }
        entries[tail].cap_count = cap_count;
        entries[tail].valid = true;
        tail = (tail + 1) % CAPACITY;
        ++count;
        return true;
    }
    
    bool pop(Entry& out) {
        if (count == 0) return false;
        out = entries[head];
        entries[head].valid = false;
        head = (head + 1) % CAPACITY;
        --count;
        return true;
    }
    
    size_t size() const { return count; }
};

static MessageQueue g_message_queues[4096]; // Per portal handle

MessageQueue& get_queue(portal_handle_t handle) {
    return g_message_queues[handle % 4096];
}

// ──────────────────────────────────────────────────────────────
// Process management for spawn
// ──────────────────────────────────────────────────────────────

struct UserProcess {
    uint32_t pid;
    portal_handle_t portal;
    portal_caps_t caps;
    const char* cmd;
    const char* cwd;
    int exit_code = -1;
    bool exited = false;
    task::task* kernel_task = nullptr;
};

static UserProcess g_user_processes[4096];

UserProcess* allocate_user_process(const char* cmd, const char* cwd, portal_caps_t caps) {
    for (size_t i = 0; i < 4096; ++i) {
        if (!g_user_processes[i].kernel_task) {
            auto* p = &g_user_processes[i];
            p->pid = ++g_next_pid;
            p->cmd = kstrdup(cmd);
            p->cwd = kstrdup(cwd);
            p->caps = caps;
            return p;
        }
    }
    return nullptr;
}

UserProcess* find_user_process(uint32_t pid) {
    for (size_t i = 0; i < 4096; ++i) {
        if (g_user_processes[i].pid == pid) {
            return &g_user_processes[i];
        }
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────
// Telemetry
// ──────────────────────────────────────────────────────────────

int64_t portal_sys_get_telemetry(portal_telemetry_t* out) {
    lib::strcpy(out->kernel_version, "Portal Kernel 0.2.0");
    lib::strcpy(out->architecture, "x86_64");
    out->cpu_cores = 1;
    out->cpu_usage_pct = 0.0f;
    out->mem_total_mb = 64;
    out->mem_available_mb = 32;
    out->disk_available_gb = 0;
    return 0;
}

// ──────────────────────────────────────────────────────────────
// Syscall implementations
// ──────────────────────────────────────────────────────────────

int64_t portal_sys_create(const char* name, portal_caps_t caps, portal_handle_t* out_handle) {
    // Validate caps - only allow certain caps for server portals
    if (caps & PORTAL_CAP_PROC_SPAWN) return -1; // EPERM
    if (caps & PORTAL_CAP_PROC_KILL) return -1;
    
    auto* portal = create_portal(caps, true, name);
    if (!portal) return -12; // ENOMEM
    
    if (name) {
        register_named_portal(name, portal);
    }
    
    auto* table = get_current_portal_table();
    if (!table) {
        destroy_portal(portal);
        return -12; // ENOMEM
    }
    
    portal_handle_t handle = table->allocate(portal);
    if (handle == PORTAL_HANDLE_INVALID) {
        destroy_portal(portal);
        return -12; // ENOMEM
    }
    
    *out_handle = handle;
    return 0;
}

int64_t portal_sys_connect(const char* name, portal_caps_t caps, portal_handle_t* out_handle) {
    auto* server = find_named_portal(name);
    if (!server) return -2; // ENOENT
    
    portal_caps_t granted = caps & server->caps;
    if (granted == 0) return -13; // EACCES
    
    auto* client = create_portal(granted, false, name);
    if (!client) return -12;
    
    client->connected_portal = server;
    server->connected_portal = client;
    
    auto* table = get_current_portal_table();
    if (!table) {
        destroy_portal(client);
        return -12;
    }
    
    portal_handle_t handle = table->allocate(client);
    if (handle == PORTAL_HANDLE_INVALID) {
        destroy_portal(client);
        return -12;
    }
    
    *out_handle = handle;
    return 0;
}

int64_t portal_sys_send(portal_handle_t handle, const portal_message_t* msg,
                        const portal_handle_t* cap_handles, size_t cap_count) {
    auto* portal = lookup_portal(handle);
    if (!portal) return -9; // EBADF
    
    Portal* target = portal->connected_portal ? portal->connected_portal : portal;
    if (!target) return -32; // EPIPE
    
    auto& queue = get_queue(target->handle);
    
    size_t total_size = sizeof(portal_message_t) + msg->length;
    auto* msg_copy = static_cast<portal_message_t*>(mm::kmalloc(total_size));
    if (!msg_copy) return -12;
    lib::memcpy(msg_copy, msg, total_size);
    
    portal_handle_t cap_copy[4] = {0};
    if (cap_handles && cap_count > 0) {
        cap_count = (cap_count > 4) ? 4 : cap_count;
        lib::memcpy(cap_copy, cap_handles, cap_count * sizeof(portal_handle_t));
    }
    
    if (!queue.push(msg_copy, cap_copy, cap_count)) {
        mm::kfree(msg_copy);
        return -12;
    }
    
    return 0;
}

int64_t portal_sys_recv(portal_handle_t handle, portal_message_t* msg_buf,
                        portal_handle_t* cap_handles, size_t cap_capacity, size_t* out_cap_count) {
    auto* portal = lookup_portal(handle);
    if (!portal) return -9; // EBADF
    
    auto& queue = get_queue(portal->handle);
    MessageQueue::Entry entry;
    
    if (!queue.pop(entry)) {
        return -11; // EAGAIN
    }
    
    if (out_cap_count) *out_cap_count = entry.cap_count;
    if (cap_handles && cap_capacity > 0 && entry.cap_count > 0) {
        size_t copy_count = (entry.cap_count < cap_capacity) ? entry.cap_count : cap_capacity;
        lib::memcpy(cap_handles, entry.cap_handles, copy_count * sizeof(portal_handle_t));
    }
    
    size_t copy_len = (entry.msg->length < PORTAL_MAX_MESSAGE_SIZE) ? entry.msg->length : PORTAL_MAX_MESSAGE_SIZE;
    lib::memcpy(msg_buf->data, entry.msg->data, copy_len);
    msg_buf->length = static_cast<uint32_t>(copy_len);
    
    mm::kfree(entry.msg);
    return 0;
}

int64_t portal_sys_close(portal_handle_t handle) {
    auto* table = get_current_portal_table();
    if (!table) return -9;
    
    auto* portal = table->get(handle);
    if (!portal) return -9;
    
    table->free(handle);
    
    if (portal->is_server && portal->name) {
        unregister_named_portal(portal->name);
    }
    
    destroy_portal(portal);
    return 0;
}

// ──────────────────────────────────────────────────────────────
// Process spawning
// ──────────────────────────────────────────────────────────────

int64_t portal_sys_spawn_restricted(const char* cmd, const char* cwd,
                                    portal_caps_t allowed_caps,
                                    uint32_t* out_pid, portal_handle_t* out_portal) {
    auto* proc = allocate_user_process(cmd, cwd, allowed_caps);
    if (!proc) return -12; // ENOMEM
    
    auto* portal = create_portal(allowed_caps, false, "proc_portal");
    if (!portal) {
        // Cleanup proc
        return -12;
    }
    proc->portal = portal->handle;
    
    auto* table = get_current_portal_table();
    if (!table) {
        destroy_portal(portal);
        return -12;
    }
    table->allocate(portal);
    
    // Create kernel task for now (placeholder for userspace)
    proc->kernel_task = reinterpret_cast<task::task*>(task::task_create(
        [] (void*) {
            lib::kprintf("[proc %u] executing: %s\n", task::task_current()->id, "cmd");
            task::task_exit(0);
        },
        nullptr,
        task::task_priority::NORMAL,
        16384
    ));
    
    *out_pid = proc->pid;
    *out_portal = proc->portal;
    return 0;
}

int64_t portal_sys_wait(uint32_t pid, int* out_exit_code) {
    auto* proc = find_user_process(pid);
    if (!proc) return -10; // ECHILD
    
    if (!proc->exited) {
        return -11; // EAGAIN
    }
    
    *out_exit_code = proc->exit_code;
    return 0;
}

int64_t portal_sys_kill(uint32_t pid, int signal) {
    auto* proc = find_user_process(pid);
    if (!proc) return -3; // ESRCH
    
    if (proc->kernel_task) {
        proc->exit_code = 128 + signal;
        proc->exited = true;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────
// Filesystem operations
// ──────────────────────────────────────────────────────────────

int64_t portal_sys_fs_op(portal_fs_request_t* req) {
    namespace fs = feather::fs;
    
    switch (req->op) {
        case portal_fs_op_t::PORTAL_FS_READ: {
            auto* node = fs::vfs_lookup(req->path);
            if (!node || node->type != fs::node_type::FILE) return -2;
            
            uint64_t offset = req->read.offset;
            uint64_t length = req->read.length;
            if (offset >= node->size) {
                req->bytes_transferred = 0;
                return 0;
            }
            length = (length > node->size - offset) ? node->size - offset : length;
            
            // Copy file data to userspace buffer
            const char* src = node->data ? node->data + offset : "";
            if (req->read.data) {
                lib::memcpy(req->read.data, src, length);
            }
            req->bytes_transferred = length;
            return 0;
        }
        
        case portal_fs_op_t::PORTAL_FS_WRITE: {
            req->bytes_transferred = req->write.length;
            return 0;
        }
        
        case portal_fs_op_t::PORTAL_FS_UNLINK:
            return -38; // ENOSYS
        
        case portal_fs_op_t::PORTAL_FS_MKDIR:
            return -38;
        
        case portal_fs_op_t::PORTAL_FS_READDIR: {
            auto* node = fs::vfs_lookup(req->path);
            if (!node || node->type != fs::node_type::DIR) return -20;
            
            size_t offset = 0;
            uint8_t* buf = req->readdir.buf;
            size_t cap = req->readdir.count;
            
            if (cap < 4) return -34;
            
            uint32_t count = 0;
            for (auto* child = node->child; child; child = child->sibling) ++count;
            
            *reinterpret_cast<uint32_t*>(buf) = count;
            offset = 4;
            
            for (auto* child = node->child; child && offset < cap; child = child->sibling) {
                size_t name_len = lib::strlen(child->name);
                if (offset + 2 + name_len + 1 > cap) break;
                
                *reinterpret_cast<uint16_t*>(buf + offset) = static_cast<uint16_t>(name_len);
                offset += 2;
                lib::memcpy(buf + offset, child->name, name_len);
                offset += name_len;
                buf[offset] = (child->type == fs::node_type::DIR) ? 1 : 0;
                offset++;
            }
            req->readdir.count = offset;
            return 0;
        }
        
        case portal_fs_op_t::PORTAL_FS_STAT: {
            auto* node = fs::vfs_lookup(req->path);
            if (!node) return -2;
            
            req->stat_result.size = node->size;
            req->stat_result.is_dir = (node->type == fs::node_type::DIR) ? 1 : 0;
            return 0;
        }
        
        case portal_fs_op_t::PORTAL_FS_RENAME:
            return -38;
        
        default:
            return -38;
    }
}

// ──────────────────────────────────────────────────────────────
// Init
// ──────────────────────────────────────────────────────────────

void portal_init() {
    lib::kputs("[portal] subsystem initialized\n");
}

} // namespace feather::kernel::portal