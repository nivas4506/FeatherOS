// FeatherOS IPC Transport — JSON-RPC 2.0 over Unix Domain Socket (Linux)
//                                      or Named Pipe (Windows fallback)
// C++20 implementation matching the Rust feather-ipc crate wire protocol:
// 4-byte big-endian length header + UTF-8 JSON body.

#include "feather/ipc.hpp"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <cstring>
#endif

#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace feather::ipc {

namespace {

std::filesystem::path runtime_dir() {
#if defined(_WIN32)
    if (auto* pd = std::getenv("TEMP")) return std::filesystem::path(pd) / "feather";
    return std::filesystem::current_path() / "feather";
#else
    if (auto* rd = std::getenv("XDG_RUNTIME_DIR")) return std::filesystem::path(rd) / "feather";
    if (auto* tmp = std::getenv("TMPDIR")) return std::filesystem::path(tmp) / "feather";
    return std::filesystem::path("/tmp") / "feather";
#endif
}

} // namespace

// ─── Default endpoints ─────────────────────────────────────────────────────────

std::filesystem::path default_socket_path() {
    return runtime_dir() / "featherd.sock";
}

std::string default_endpoint() {
#if defined(_WIN32)
    return R"(\\.\pipe\featherd)";
#else
    return default_socket_path().string();
#endif
}

// ─── Frame I/O (blocking, fd-based) ────────────────────────────────────────────

bool write_frame(int fd, std::string_view json) {
    std::uint32_t len = static_cast<std::uint32_t>(json.size());
    // 4-byte big-endian length header
    std::array<std::uint8_t, 4> header{
        static_cast<std::uint8_t>((len >> 24) & 0xFF),
        static_cast<std::uint8_t>((len >> 16) & 0xFF),
        static_cast<std::uint8_t>((len >>  8) & 0xFF),
        static_cast<std::uint8_t>((len >>  0) & 0xFF),
    };

    ssize_t written = 0;
    while (written < 4) {
        ssize_t n = ::send(fd, reinterpret_cast<const char*>(header.data()) + written,
                           4 - written, 0);
        if (n <= 0) return false;
        written += n;
    }

    const char* ptr = json.data();
    std::size_t remaining = json.size();
    while (remaining > 0) {
        ssize_t n = ::send(fd, ptr, remaining, 0);
        if (n <= 0) return false;
        ptr += n;
        remaining -= static_cast<std::size_t>(n);
    }
    return true;
}

std::string read_frame(int fd) {
    // Read 4-byte length header
    std::array<std::uint8_t, 4> header{};
    ssize_t got = 0;
    while (got < 4) {
        ssize_t n = ::recv(fd, reinterpret_cast<char*>(header.data()) + got,
                           4 - got, 0);
        if (n <= 0) return ""; // EOF or error
        got += n;
    }
    std::uint32_t len = (static_cast<std::uint32_t>(header[0]) << 24) |
                        (static_cast<std::uint32_t>(header[1]) << 16) |
                        (static_cast<std::uint32_t>(header[2]) <<  8) |
                        (static_cast<std::uint32_t>(header[3]) <<  0);

    if (len == 0) return "";
    if (len > MAX_FRAME_BYTES) throw std::runtime_error("Frame exceeds MAX_FRAME_BYTES");

    std::string body;
    body.resize(len);
    char* ptr = body.data();
    std::size_t remaining = len;
    while (remaining > 0) {
        ssize_t n = ::recv(fd, ptr, remaining, 0);
        if (n <= 0) return "";
        ptr += n;
        remaining -= static_cast<std::size_t>(n);
    }
    return body;
}

// ─── JSON-RPC constructors ─────────────────────────────────────────────────────

std::string make_request(std::uint64_t id, std::string_view method,
                          std::string_view params_json) {
    std::ostringstream os;
    os << "{\"jsonrpc\":\"2.0\",\"id\":" << id
       << ",\"method\":\"" << method << "\"";
    if (!params_json.empty() && std::string_view(params_json) != "{}") {
        os << ",\"params\":" << params_json;
    } else {
        os << ",\"params\":{}";
    }
    os << "}";
    return os.str();
}

std::string make_notification(std::string_view method,
                               std::string_view params_json) {
    std::ostringstream os;
    os << "{\"jsonrpc\":\"2.0\",\"method\":\"" << method << "\"";
    if (!params_json.empty() && std::string_view(params_json) != "{}") {
        os << ",\"params\":" << params_json;
    } else {
        os << ",\"params\":{}";
    }
    os << "}";
    return os.str();
}

std::string make_success(std::uint64_t id, std::string_view result_json) {
    std::ostringstream os;
    os << "{\"jsonrpc\":\"2.0\",\"id\":" << id
       << ",\"result\":" << result_json << "}";
    return os.str();
}

std::string make_error(std::uint64_t id, int code, std::string_view message) {
    std::ostringstream os;
    os << "{\"jsonrpc\":\"2.0\",\"id\":" << id
       << ",\"error\":{\"code\":" << code
       << ",\"message\":\"" << message << "\"}}";
    return os.str();
}

// ─── Blocking C++ Client ───────────────────────────────────────────────────────

Client::Client(std::string_view endpoint) : fd_(-1) {
    if (endpoint.empty()) {
#if defined(_WIN32)
        fd_ = -1; // named-pipe client not implemented in this stub
#else
        connect(default_socket_path().string());
#endif
    } else {
        connect(std::string(endpoint));
    }
}

Client::~Client() { disconnect(); }

Client::Client(Client&& other) noexcept : fd_(other.fd_), next_id_(other.next_id_) {
    other.fd_ = -1;
}

Client& Client::operator=(Client&& other) noexcept {
    if (this != &other) {
        disconnect();
        fd_ = other.fd_;
        next_id_ = other.next_id_;
        other.fd_ = -1;
    }
    return *this;
}

void Client::connect(const std::string& endpoint) {
#if defined(_WIN32)
    (void)endpoint;
    fd_ = -1;
#else
    fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd_ < 0) return;
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, endpoint.c_str(),
                 sizeof(addr.sun_path) - 1);
    if (::connect(fd_, reinterpret_cast<struct sockaddr*>(&addr),
                  sizeof(addr)) != 0) {
        ::close(fd_);
        fd_ = -1;
        return;
    }
#endif
}

void Client::disconnect() noexcept {
#if !defined(_WIN32)
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
#else
    fd_ = -1;
#endif
}

std::string Client::call(std::string_view method,
                          std::string_view params_json) {
    if (fd_ < 0) return "";
    std::string req = make_request(next_id_++, method, params_json);
    if (!write_frame(fd_, req)) return "";
    std::string resp = read_frame(fd_);
    if (resp.empty()) return "";
    // Extract result or error field crudely
    return resp;
}

} // namespace feather::ipc
