// Portal Kernel Syscall Stubs — For demonstration without running kernel
// In production, these would be actual syscalls implemented in the kernel

#include <portal/portal.h>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <atomic>

// ──────────────────────────────────────────────────────────────
// Mock state
// ──────────────────────────────────────────────────────────────
static std::mutex g_mutex;
static std::unordered_map<portal_handle_t, std::string> g_portal_messages;
static std::atomic<portal_handle_t> g_next_handle{1};
static std::atomic<uint32_t> g_next_pid{1000};

struct MockProcess {
    uint32_t pid;
    portal_handle_t portal;
    std::string cmd;
    int exit_code = 0;
    bool running = true;
};
static std::unordered_map<uint32_t, MockProcess> g_processes;

// ──────────────────────────────────────────────────────────────
// Portal lifecycle
// ──────────────────────────────────────────────────────────────
int portal_create(const char* name, portal_caps_t caps, portal_handle_t* out_handle) {
    std::lock_guard<std::mutex> lock(g_mutex);
    portal_handle_t handle = g_next_handle++;
    *out_handle = handle;
    g_portal_messages[handle] = "";
    return 0;
}

int portal_connect(const char* name, portal_caps_t caps, portal_handle_t* out_handle) {
    std::lock_guard<std::mutex> lock(g_mutex);
    portal_handle_t handle = g_next_handle++;
    *out_handle = handle;
    g_portal_messages[handle] = "";
    return 0;
}

int portal_close(portal_handle_t handle) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_portal_messages.erase(handle);
    return 0;
}

// ──────────────────────────────────────────────────────────────
// Message passing
// ──────────────────────────────────────────────────────────────
int portal_send(portal_handle_t handle, const portal_message_t* msg,
                const portal_handle_t* cap_handles, size_t cap_count) {
    (void)cap_handles;
    (void)cap_count;
    
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_portal_messages.find(handle) == g_portal_messages.end()) {
        return -1; // EPIPE
    }
    
    // Store message for recv
    std::string payload(reinterpret_cast<const char*>(msg->data), msg->length);
    g_portal_messages[handle] = payload;
    return 0;
}

int portal_recv(portal_handle_t handle, portal_message_t* msg_buf,
                portal_handle_t* cap_handles, size_t cap_capacity, size_t* out_cap_count) {
    (void)cap_handles;
    (void)cap_capacity;
    if (out_cap_count) *out_cap_count = 0;
    
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_portal_messages.find(handle);
    if (it == g_portal_messages.end()) {
        return -1; // ENOTCONN
    }
    
    const std::string& payload = it->second;
    if (payload.empty()) {
        return -2; // EAGAIN - no message available
    }
    
    msg_buf->length = static_cast<uint32_t>(payload.size());
    std::memcpy(msg_buf->data, payload.data(), payload.size());
    it->second.clear(); // consume message
    return 0;
}

// ──────────────────────────────────────────────────────────────
// Process management
// ──────────────────────────────────────────────────────────────
int portal_spawn_restricted(const char* cmd, const char* cwd,
                            portal_caps_t allowed_caps,
                            uint32_t* out_pid, portal_handle_t* out_portal) {
    std::lock_guard<std::mutex> lock(g_mutex);
    
    uint32_t pid = g_next_pid++;
    portal_handle_t child_portal = g_next_handle++;
    
    MockProcess proc;
    proc.pid = pid;
    proc.portal = child_portal;
    proc.cmd = cmd;
    
    g_processes[pid] = proc;
    g_portal_messages[child_portal] = "";
    
    *out_pid = pid;
    *out_portal = child_portal;
    
    // Simulate async process execution
    std::thread([pid, cmd, child_portal]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_processes.find(pid);
        if (it != g_processes.end()) {
            it->second.running = false;
            it->second.exit_code = 0;
            
            // Send stdout
            std::string stdout_msg = "{\"stream\":\"stdout\",\"data\":\"Process output: " + std::string(cmd) + "\\n\"}";
            g_portal_messages[child_portal] = stdout_msg;
        }
    }).detach();
    
    return 0;
}

int portal_wait(uint32_t pid, int* out_exit_code) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_processes.find(pid);
    if (it == g_processes.end()) {
        return -10; // ECHILD
    }
    
    if (it->second.running) {
        return -11; // EAGAIN
    }
    
    *out_exit_code = it->second.exit_code;
    return 0;
}

int portal_kill(uint32_t pid, int signal) {
    (void)signal;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_processes.find(pid);
    if (it == g_processes.end()) {
        return -3; // ESRCH
    }
    it->second.running = false;
    it->second.exit_code = 128 + signal;
    return 0;
}

// ──────────────────────────────────────────────────────────────
// Telemetry
// ──────────────────────────────────────────────────────────────
int portal_get_telemetry(portal_telemetry_t* out) {
    std::strncpy(out->kernel_version, "Portal Kernel 0.1.0", sizeof(out->kernel_version) - 1);
    std::strncpy(out->architecture, "x86_64", sizeof(out->architecture) - 1);
    out->cpu_cores = std::thread::hardware_concurrency();
    out->cpu_usage_pct = 12.5f;
    out->mem_total_mb = 8192;
    out->mem_available_mb = 4096;
    out->disk_available_gb = 128;
    return 0;
}

// ──────────────────────────────────────────────────────────────
// Filesystem (simplified mock using real filesystem)
// ──────────────────────────────────────────────────────────────
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>

int portal_fs_op(portal_fs_request_t* req) {
    namespace fs = std::filesystem;
    
    try {
        switch (req->op) {
            case PORTAL_FS_READ: {
                fs::path p(req->path);
                if (!fs::exists(p)) return -2; // ENOENT
                auto size = fs::file_size(p);
                
                uint64_t offset = req->read.offset;
                uint64_t length = req->read.length;
                if (offset >= size) {
                    req->bytes_transferred = 0;
                    return 0;
                }
                length = (length > size - offset) ? size - offset : length;
                
                // Copy file data to userspace buffer
                std::ifstream ifs(p, std::ios::binary);
                ifs.seekg(offset);
                if (req->read.data && length > 0) {
                    ifs.read(reinterpret_cast<char*>(req->read.data), length);
                    req->bytes_transferred = ifs.gcount();
                } else {
                    req->bytes_transferred = length;
                }
                return 0;
            }
            case PORTAL_FS_WRITE: {
                fs::path p(req->path);
                fs::create_directories(p.parent_path());
                std::ofstream ofs(p, std::ios::binary);
                ofs.write(reinterpret_cast<const char*>(req->write.data), req->write.length);
                req->bytes_transferred = req->write.length;
                return 0;
            }
            case PORTAL_FS_UNLINK: {
                fs::remove(req->path);
                return 0;
            }
            case PORTAL_FS_MKDIR: {
                fs::create_directories(req->path);
                return 0;
            }
            case PORTAL_FS_RENAME: {
                fs::rename(req->path, req->rename.new_path);
                return 0;
            }
            case PORTAL_FS_READDIR: {
                fs::path p(req->path);
                if (!fs::exists(p) || !fs::is_directory(p)) return -20; // ENOTDIR
                
                std::vector<std::string> entries;
                for (const auto& entry : fs::directory_iterator(p)) {
                    entries.push_back(entry.path().filename().string());
                }
                
                // Pack into buffer: count(u32) + [name_len(u16) + name + is_dir(u8)]...
                uint8_t* buf = req->readdir.buf;
                size_t cap = req->readdir.count;
                size_t offset = 0;
                
                if (cap < 4) return -34; // ERANGE
                *reinterpret_cast<uint32_t*>(buf) = static_cast<uint32_t>(entries.size());
                offset = 4;
                
                for (const auto& name : entries) {
                    if (offset + 2 + name.size() + 1 > cap) break;
                    *reinterpret_cast<uint16_t*>(buf + offset) = static_cast<uint16_t>(name.size());
                    offset += 2;
                    std::memcpy(buf + offset, name.data(), name.size());
                    offset += name.size();
                    buf[offset] = fs::is_directory(fs::path(req->path) / name) ? 1 : 0;
                    offset++;
                }
                req->readdir.count = offset;
                return 0;
            }
            case PORTAL_FS_STAT: {
                fs::path p(req->path);
                if (!fs::exists(p)) return -2; // ENOENT
                req->stat_result.size = fs::file_size(p);
                req->stat_result.is_dir = fs::is_directory(p) ? 1 : 0;
                return 0;
            }
            default:
                return -38; // ENOSYS
        }
    } catch (...) {
        return -5; // EIO
    }
}