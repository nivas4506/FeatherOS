// FeatherOS feather-watch — Linux inotify filesystem event watcher
#include "feather/watch.hpp"
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <sys/inotify.h>
#include <limits.h>

namespace feather::watch {

class LinuxWorkspaceWatcher final : public IWorkspaceWatcher {
public:
    explicit LinuxWorkspaceWatcher(EventCallback cb) : cb_(std::move(cb)) {
        inotify_fd_ = ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (inotify_fd_ < 0)
            throw std::runtime_error("inotify_init1 failed");
    }

    ~LinuxWorkspaceWatcher() { stop(); }

    void watch(const std::filesystem::path& root) override {
        int wd = ::inotify_add_watch(inotify_fd_, root.c_str(),
            IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
        if (wd < 0) return;

        std::error_code ec;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec))
            if (entry.is_directory())
                ::inotify_add_watch(inotify_fd_, entry.path().c_str(),
                    IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);

        {
            std::lock_guard<std::mutex> lock(roots_mutex_);
            roots_.push_back(root);
        }

        if (!running_.exchange(true)) {
            thread_ = std::thread([this] { run(); });
        }
    }

    void stop() noexcept override {
        running_.store(false);
        if (inotify_fd_ >= 0) { ::close(inotify_fd_); inotify_fd_ = -1; }
        if (thread_.joinable()) thread_.join();
    }

    std::vector<std::filesystem::path> roots() const override {
        std::lock_guard<std::mutex> lock(roots_mutex_);
        return roots_;
    }

private:
    void run() {
        constexpr size_t BUF_SZ = sizeof(inotify_event) + NAME_MAX + 1;
        alignas(inotify_event) char buf[BUF_SZ * 64];

        while (running_.load()) {
            ssize_t n = ::read(inotify_fd_, buf, sizeof(buf));
            if (n <= 0) { usleep(10000); continue; }

            for (ssize_t i = 0; i < n; ) {
                auto* ev = reinterpret_cast<inotify_event*>(buf + i);
                std::filesystem::path path = (ev->len > 0) ? ev->name : "";

                WatchEvent kind = WatchEvent::Modified;
                if      (ev->mask & IN_CREATE)    kind = WatchEvent::Created;
                else if (ev->mask & IN_DELETE)    kind = WatchEvent::Deleted;
                else if (ev->mask & IN_MOVED_FROM || ev->mask & IN_MOVED_TO)
                                              kind = WatchEvent::Renamed;

                cb_({kind, path});
                i += static_cast<ssize_t>(sizeof(inotify_event) + ev->len);
            }
        }
    }

    EventCallback cb_;
    int inotify_fd_ = -1;
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::vector<std::filesystem::path> roots_;
    mutable std::mutex roots_mutex_;
};

std::unique_ptr<IWorkspaceWatcher> create_workspace_watcher(EventCallback cb) {
    return std::make_unique<LinuxWorkspaceWatcher>(std::move(cb));
}

/* ──────────────────────────────────────────────────────────────
 * Telemetry — uses HAL SystemTelemetryProvider
 * ────────────────────────────────────────────────────────────── */
HardwareTelemetry Telemetry::sample(const std::filesystem::path& workspace) {
    HardwareTelemetry t;
    
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
        t.arch = telemetry->architecture();
        t.kernel_version = telemetry->kernel_version();
        t.cpu_cores = std::thread::hardware_concurrency();
    }
    
    t.os_name = "Linux";
    return t;
}

float Telemetry::sample_cpu_usage() {
    auto telemetry = feather::hal::create_telemetry_provider();
    auto result = telemetry->sample_telemetry();
    if (result.has_value()) {
        return result.value().cpu_usage_pct;
    }
    return 0.0f;
}

} // namespace feather::watch