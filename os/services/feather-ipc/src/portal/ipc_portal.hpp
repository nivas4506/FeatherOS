/*
 * Portal IPC Transport — JSON-RPC 2.0 over Portal Kernel IPC
 * 
 * Implements the same wire protocol as Unix sockets/named pipes:
 *   4-byte big-endian length + UTF-8 JSON-RPC 2.0 body
 * 
 * Uses portal_send/portal_recv syscalls for message passing.
 */

#pragma once

#include <feather/ipc.hpp>
#include <portal/portal.h>
#include <string>
#include <vector>
#include <cstdint>
#include <expected>

namespace feather::ipc {

/* ──────────────────────────────────────────────────────────────
 * PortalTransport — blocking frame I/O on portal handle
 * ────────────────────────────────────────────────────────────── */
class PortalTransport {
public:
    explicit PortalTransport(portal_handle_t handle) : handle_(handle) {}
    PortalTransport(const PortalTransport&) = delete;
    PortalTransport& operator=(const PortalTransport&) = delete;
    PortalTransport(PortalTransport&& other) noexcept : handle_(other.handle_) { other.handle_ = PORTAL_HANDLE_INVALID; }
    PortalTransport& operator=(PortalTransport&& other) noexcept {
        if (this != &other) { close(); handle_ = other.handle_; other.handle_ = PORTAL_HANDLE_INVALID; }
        return *this;
    }
    ~PortalTransport() { close(); }

    // Connect to featherd portal by name
    static std::expected<PortalTransport, std::string> connect(std::string_view endpoint) {
        portal_caps_t caps = PORTAL_CAP_IPC_CONNECT;
        portal_handle_t handle;
        int ret = portal_connect(endpoint.data(), caps, &handle);
        if (ret < 0) {
            return std::unexpected("portal_connect failed: " + std::string(strerror(-ret)));
        }
        return PortalTransport(handle);
    }

    // Frame I/O (matching feather-ipc wire protocol)
    bool write_frame(std::string_view json) {
        if (handle_ == PORTAL_HANDLE_INVALID) return false;

        uint32_t len = static_cast<uint32_t>(json.size());
        if (len > MAX_FRAME_BYTES) return false;

        // Build message: 4-byte BE length + payload
        std::vector<uint8_t> msg_buf(4 + len);
        msg_buf[0] = (len >> 24) & 0xFF;
        msg_buf[1] = (len >> 16) & 0xFF;
        msg_buf[2] = (len >> 8)  & 0xFF;
        msg_buf[3] = len & 0xFF;
        std::memcpy(msg_buf.data() + 4, json.data(), len);

        portal_message_t msg{};
        msg.length = len;
        // Note: portal_message_t uses flexible array member, so we need to allocate properly
        // For simplicity, we'll use a helper that takes pre-built buffer
        return send_buffer(msg_buf);
    }

    std::string read_frame() {
        if (handle_ == PORTAL_HANDLE_INVALID) return "";

        // Receive message into buffer
        std::vector<uint8_t> recv_buf(4 + MAX_FRAME_BYTES);
        portal_message_t* msg = reinterpret_cast<portal_message_t*>(recv_buf.data());

        size_t cap_count = 0;
        int ret = portal_recv(handle_, msg, nullptr, 0, &cap_count);
        if (ret < 0) return "";

        // Validate length
        uint32_t len = msg->length;
        if (len == 0 || len > MAX_FRAME_BYTES) return "";

        // Extract payload
        return std::string(reinterpret_cast<char*>(msg->data), len);
    }

    portal_handle_t native_handle() const noexcept { return handle_; }

private:
    portal_handle_t handle_ = PORTAL_HANDLE_INVALID;

    void close() {
        if (handle_ != PORTAL_HANDLE_INVALID) {
            portal_close(handle_);
            handle_ = PORTAL_HANDLE_INVALID;
        }
    }

    // Send pre-built buffer (length header + payload)
    bool send_buffer(const std::vector<uint8_t>& buf) {
        // Portal send expects portal_message_t with flexible array
        // We allocate a proper message buffer
        portal_message_t* msg = reinterpret_cast<portal_message_t*>(const_cast<uint8_t*>(buf.data()));
        msg->length = static_cast<uint32_t>(buf.size() - 4);
        int ret = portal_send(handle_, msg, nullptr, 0);
        return ret == 0;
    }
};

/* ──────────────────────────────────────────────────────────────
 * PortalClient — JSON-RPC 2.0 client over PortalTransport
 * ────────────────────────────────────────────────────────────── */
class PortalClient {
public:
    explicit PortalClient(std::string_view endpoint = default_endpoint()) {
        auto result = PortalTransport::connect(endpoint);
        if (!result) {
            transport_ = std::nullopt;
            return;
        }
        transport_ = std::move(*result);
        next_id_ = 1;
    }

    bool connected() const noexcept { return transport_.has_value(); }

    // Synchronous call: returns result JSON or empty string on error
    std::string call(std::string_view method, std::string_view params_json) {
        if (!transport_) return "";
        return call_impl(method, params_json, nullptr);
    }

    // Call with event handler for notifications
    template<typename EventHandler>
    std::string call_with_events(std::string_view method, std::string_view params_json, EventHandler&& on_event) {
        if (!transport_) return "";
        return call_impl(method, params_json, &on_event);
    }

private:
    std::optional<PortalTransport> transport_;
    std::uint64_t next_id_ = 1;

    template<typename EventHandler>
    std::string call_impl(std::string_view method, std::string_view params_json, EventHandler* on_event) {
        std::uint64_t id = next_id_++;

        // Build request
        std::string request = make_request(id, method, params_json);

        // Send
        if (!transport_->write_frame(request)) return "";

        // Receive loop
        while (true) {
            std::string response = transport_->read_frame();
            if (response.empty()) return ""; // Disconnected or error

            // Parse JSON-RPC response
            // For MVP: simple string search (replace with nlohmann/json in production)
            if (contains_id(response, id)) {
                if (contains_error(response)) return "";
                return extract_result(response);
            }

            // Notification (no id) — forward to handler
            if (on_event && is_notification(response)) {
                (*on_event)(response);
            }
        }
    }

    // JSON-RPC helpers (minimal, replace with proper JSON parser)
    static std::string make_request(std::uint64_t id, std::string_view method, std::string_view params) {
        std::string req = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"method\":\"" + std::string(method) + "\"";
        if (!params.empty() && params != "{}") {
            req += ",\"params\":" + std::string(params);
        } else {
            req += ",\"params\":{}";
        }
        req += "}";
        return req;
    }

    static bool contains_id(const std::string& json, std::uint64_t id) {
        return json.find("\"id\":" + std::to_string(id)) != std::string::npos;
    }

    static bool contains_error(const std::string& json) {
        return json.find("\"error\"") != std::string::npos;
    }

    static bool is_notification(const std::string& json) {
        return json.find("\"id\"") == std::string::npos && json.find("\"method\"") != std::string::npos;
    }

    static std::string extract_result(const std::string& json) {
        size_t pos = json.find("\"result\":");
        if (pos == std::string::npos) return "";
        pos += 9; // skip "result":
        // Find matching brace/bracket for nested structures
        int depth = 0;
        size_t start = pos;
        for (size_t i = pos; i < json.size(); ++i) {
            char c = json[i];
            if (c == '{' || c == '[') depth++;
            else if (c == '}' || c == ']') {
                if (depth == 0) return json.substr(start, i - start + 1);
                depth--;
            }
        }
        return json.substr(start); // fallback
    }
};

/* ──────────────────────────────────────────────────────────────
 * Default endpoint resolution
 * ────────────────────────────────────────────────────────────── */
inline std::string default_endpoint() {
    // Portal kernel uses named portals, not filesystem sockets
    return "featherd"; // Portal name
}

inline std::filesystem::path data_dir() {
    // Same as existing implementation
    if (auto* env = std::getenv("FEATHER_HOME")) return std::filesystem::path(env);
    if (auto* env = std::getenv("HOME")) return std::filesystem::path(env) / ".feather";
    if (auto* env = std::getenv("USERPROFILE")) return std::filesystem::path(env) / ".feather";
    return std::filesystem::current_path() / ".feather";
}

} // namespace feather::ipc