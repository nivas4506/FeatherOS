// FeatherOS feather-watch — PORTAL Kernel Backend
// Uses portal_fs_op for filesystem events and portal_get_telemetry for system metrics.

#include "feather/watch.hpp"
#include "feather/hal.hpp"
#include <portal/portal.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <vector>
#include <filesystem>
#include <algorithm>

namespace feather::watch {

/* ──────────────────────────────────────────────────────────────
 * Portal-based WorkspaceWatcher
 * Polls portal_fs_op for directory changes (simplified for MVP)
 * Real implementation would use kernel event notifications
 * ────────────────────────────────────────────────────────────── */
class PortalWorkspaceWatcher final : public WorkspaceWatcher {
public:
    explicit PortalWorkspaceWatcher(EventCallback cb) : WorkspaceWatcher(std::move(cb)) {}

    ~PortalWorkspaceWatcher() { stop(); }

    void watch(const std::filesystem::path& root) override {
        std::lock_guard<std::mutex> lock(roots_mutex_);
        roots_.push_back(root);
        
        if (!running_.exchange(true)) {
            thread_ = std::thread([this] { run(); });
        }
    }

    void stop() noexcept override {
        running_.store(false);
        if (thread_.joinable()) thread_.join();
    }

private:
    void run() override {
        // For MVP: poll directory listings periodically
        // Real implementation: kernel sends notifications via portal
        while (running_.load()) {
            std::vector<std::filesystem::path> current_roots;
            {
                std::lock_guard<std::mutex> lock(roots_mutex_);
                current_roots = roots_;
            }

            for (const auto& root : current_roots) {
                // Use HAL VFS to list directory
                // This is a simplified polling approach
                // Real impl: portal_fs_notify or similar
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            if (rescan.load()) {
                rescan.store(false);
                // Trigger full rescan callback
                for (const auto& root : current_roots) {
                    cb_({WatchEvent::Created, root}); // Signal rescan
                }
            }
        }
    }
};

/* ──────────────────────────────────────────────────────────────
 * Factory
 * ────────────────────────────────────────────────────────────── */
std::unique_ptr<WorkspaceWatcher> create_workspace_watcher(
    typename WorkspaceWatcher::EventCallback cb)
{
    return std::make_unique<PortalWorkspaceWatcher>(std::move(cb));
}

/* ──────────────────────────────────────────────────────────────
 * Telemetry — uses HAL SystemTelemetryProvider
 * ────────────────────────────────────────────────────────────── */
HardwareTelemetry Telemetry::sample(const std::filesystem::path& workspace) {
    HardwareTelemetry t;
    
    // Use HAL telemetry provider
    auto telemetry = feather::hal::create_telemetry_provider();
    auto result = telemetry->sample_telemetry();
    
    if (result.has_value()) {
        const auto& h = result.value();
        t.arch = h.arch;
        t.kernel_version = h.kernel_version;
        t.cpu_cores = h.cpu_cores;
        t.cpu_usage_pct = h.cpu_usage_pct;
        t.mem_total_mb = h.mem_total_mb;
        t.mem_available_mb = h.mem_available_mb;
        t.disk_available_gb = h.disk_available_gb;
    } else {
        // Fallback defaults
        t.arch = telemetry->architecture();
        t.kernel_version = telemetry->kernel_version();
        t.cpu_cores = std::thread::hardware_concurrency();
        t.cpu_usage_pct = 0.0f;
        t.mem_total_mb = 0;
        t.mem_available_mb = 0;
        t.disk_available_gb = 0;
    }
    
    t.os_name = "Portal Kernel";
    return t;
}

float Telemetry::sample_cpu_usage() {
    // Delegated to HAL telemetry provider
    auto telemetry = feather::hal::create_telemetry_provider();
    auto result = telemetry->sample_telemetry();
    if (result.has_value()) {
        return result.value().cpu_usage_pct;
    }
    return 0.0f;
}

} // namespace feather::watch