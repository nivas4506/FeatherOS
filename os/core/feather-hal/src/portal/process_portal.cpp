/*
 * Portal ProcessManager Backend
 * Implements feather::hal::ProcessManager using portal_spawn_restricted syscall.
 */

#include "feather/hal.hpp"
#include <portal/portal.h>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <cerrno>

namespace feather::hal {

class PortalProcessManager final : public ProcessManager {
public:
    PortalProcessManager() = default;

    Expected<ProcessOutput>
    execute(std::string_view cmd,
            const std::filesystem::path& cwd,
            SandboxKind sandbox,
            std::optional<std::chrono::seconds> timeout) override
    {
        // For MVP: all sandbox kinds map to restricted portal spawn
        // Future: switch on sandbox kind for different isolation levels
        (void)sandbox;

        // Default capabilities for restricted processes:
        // - FS read/write/create in cwd only
        // - No process spawn/kill, no IPC listen, no telemetry
        constexpr portal_caps_t kDefaultCaps =
            PORTAL_CAP_FS_READ | PORTAL_CAP_FS_WRITE | PORTAL_CAP_FS_CREATE;

        portal_handle_t child_portal = PORTAL_HANDLE_INVALID;
        uint32_t child_pid = 0;

        // Convert paths to null-terminated strings
        std::string cmd_str(cmd);
        std::string cwd_str = cwd.string();

        auto start = std::chrono::steady_clock::now();

        int ret = portal_spawn_restricted(cmd_str.c_str(), cwd_str.c_str(),
                                          kDefaultCaps, &child_pid, &child_portal);
        if (ret < 0) {
            return Expected<ProcessOutput>::err(std::string("portal_spawn_restricted failed: ") + std::strerror(-ret));
        }

        // Wait for process completion with optional timeout
        int exit_code = -1;
        if (timeout) {
            // Poll with timeout (kernel doesn't support timed wait yet)
            auto deadline = start + *timeout;
            while (std::chrono::steady_clock::now() < deadline) {
                ret = portal_wait(child_pid, &exit_code);
                if (ret == 0) break;  // Process exited
                if (ret != -EAGAIN) { // Real error
                    portal_close(child_portal);
                    return Expected<ProcessOutput>::err(std::string("portal_wait failed: ") + std::strerror(-ret));
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (exit_code == -1) {
                // Timeout — kill the process
                portal_kill(child_pid, 9); // SIGKILL
                portal_wait(child_pid, &exit_code); // Reap
            }
        } else {
            // No timeout — block indefinitely
            ret = portal_wait(child_pid, &exit_code);
            if (ret < 0) {
                portal_close(child_portal);
                return Expected<ProcessOutput>::err(std::string("portal_wait failed: ") + std::strerror(-ret));
            }
        }

        auto end = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        // Read stdout/stderr from child portal if available
        std::string stdout_data, stderr_data;
        if (child_portal != PORTAL_HANDLE_INVALID) {
            // Protocol: child sends two messages: {stream: "stdout", data: "..."} then stderr
            for (int stream = 0; stream < 2; ++stream) {
                portal_message_t msg_buf{};
                size_t cap_count = 0;
                ret = portal_recv(child_portal, &msg_buf, nullptr, 0, &cap_count);
                if (ret == 0 && msg_buf.length > 0) {
                    std::string payload(reinterpret_cast<char*>(msg_buf.data), msg_buf.length);
                    // Simple parsing: expect {"stream":"stdout","data":"..."}
                    if (payload.find("\"stdout\"") != std::string::npos) {
                        stdout_data = extract_json_string(payload, "data");
                    } else if (payload.find("\"stderr\"") != std::string::npos) {
                        stderr_data = extract_json_string(payload, "data");
                    }
                }
            }
            portal_close(child_portal);
        }

        return ProcessOutput{
            .exit_code = exit_code,
            .stdout_data = std::move(stdout_data),
            .stderr_data = std::move(stderr_data),
            .duration_ms = static_cast<std::uint64_t>(duration_ms)
        };
    }

    ExpectedVoid terminate(std::uint32_t pid) override {
        int ret = portal_kill(pid, 15); // SIGTERM
        if (ret < 0) {
            return ExpectedVoid::err(std::string("portal_kill failed: ") + std::strerror(-ret));
        }
        int exit_code;
        ret = portal_wait(pid, &exit_code);
        if (ret < 0 && ret != -ECHILD) { // ECHILD = already reaped
            return ExpectedVoid::err(std::string("portal_wait after kill failed: ") + std::strerror(-ret));
        }
        return ExpectedVoid::ok(Void{});
    }

private:
    static std::string extract_json_string(const std::string& json, const char* key) {
        std::string search = std::string("\"") + key + "\":\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos += search.length();
        size_t end = json.find('"', pos);
        if (end == std::string::npos) return "";
        // Handle escaped quotes
        std::string result;
        result.reserve(end - pos);
        for (size_t i = pos; i < end; ++i) {
            if (json[i] == '\\' && i + 1 < end && json[i + 1] == '"') {
                result += '"';
                ++i;
            } else {
                result += json[i];
            }
        }
        return result;
    }
};

std::unique_ptr<ProcessManager> create_process_manager() {
    return std::make_unique<PortalProcessManager>();
}

} // namespace feather::hal