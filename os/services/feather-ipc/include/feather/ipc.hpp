#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// FeatherOS IPC Transport — JSON-RPC 2.0 over Unix Domain Socket (Linux)
//                                      or Named Pipe (Windows fallback)
// ─────────────────────────────────────────────────────────────────────────────
#include <string>
#include <string_view>
#include <functional>
#include <cstdint>
#include <filesystem>

namespace feather::ipc {

// 4-byte big-endian length header + UTF-8 JSON body (same wire protocol as
// the Rust feather-ipc crate so that Rust apps can connect to C++ featherd).
static constexpr std::size_t MAX_FRAME_BYTES = 8 * 1024 * 1024; // 8 MiB

std::filesystem::path default_socket_path();
std::string           default_endpoint();

// ─── Frame I/O helpers (blocking, file-descriptor based) ─────────────────────
bool  write_frame(int fd, std::string_view json);
// Returns empty string on EOF, throws on error.
std::string read_frame(int fd);

// ─── JSON-RPC 2.0 constructors ────────────────────────────────────────────────
std::string make_request(uint64_t id, std::string_view method, std::string_view params_json);
std::string make_notification(std::string_view method, std::string_view params_json);
std::string make_success(uint64_t id, std::string_view result_json);
std::string make_error(uint64_t id, int code, std::string_view message);

// ─── Synchronous blocking client ─────────────────────────────────────────────
class Client {
public:
    explicit Client(std::string_view endpoint = "");
    ~Client();

    // Non-copyable, movable
    Client(const Client&)            = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) noexcept;
    Client& operator=(Client&&) noexcept;

    // Blocking call — returns result JSON string
    std::string call(std::string_view method, std::string_view params_json = "{}");

    bool connected() const noexcept { return fd_ >= 0; }
    void disconnect() noexcept;

private:
    int      fd_      = -1;
    uint64_t next_id_ = 0;
};

// ─── Non-blocking connection server helper ────────────────────────────────────
// Accepts one connection and calls handler(fd) synchronously.
void accept_one(std::string_view endpoint,
                std::function<void(int /*client_fd*/)> handler);

} // namespace feather::ipc
