#pragma once
// FeatherOS feather-watch — Ambient Filesystem Perception Engine (C++20)
// Platform-independent interface with kernel-specific backends.

#include "feather/hal.hpp"
#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <filesystem>
#include <memory>

namespace feather::watch {

enum class WatchEvent : uint8_t { Created, Modified, Deleted, Renamed };

struct FileEvent {
    WatchEvent            kind;
    std::filesystem::path path;
};

using EventCallback = std::function<void(const FileEvent&)>;

// Abstract interface for platform-specific watchers
class IWorkspaceWatcher {
public:
    virtual ~IWorkspaceWatcher() = default;
    virtual void watch(const std::filesystem::path& root) = 0;
    virtual void stop() noexcept = 0;
    virtual std::vector<std::filesystem::path> roots() const = 0;
    std::atomic<bool> rescan{false};
};

std::unique_ptr<IWorkspaceWatcher> create_workspace_watcher(EventCallback cb);

/* ──────────────────────────────────────────────────────────────
 * Telemetry
 * ────────────────────────────────────────────────────────────── */

struct HardwareTelemetry {
    std::string arch;
    std::string kernel_version;
    std::string os_name;
    std::size_t cpu_cores        = 0;
    float       cpu_usage_pct    = 0.0f;
    uint64_t    mem_total_mb     = 0;
    uint64_t    mem_available_mb = 0;
    uint64_t    disk_available_gb= 0;
    std::string hostname;
    std::vector<std::string> alerts;
};

class Telemetry {
public:
    HardwareTelemetry sample(const std::filesystem::path& workspace);

private:
    float sample_cpu_usage();
};

} // namespace feather::watch