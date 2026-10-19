#pragma once
// FeatherOS featherd — Supervisor Daemon (C++20)
// Multi-platform: PORTAL (custom kernel), LINUX (Unix socket), WIN32 (Named pipe).
// Orchestrates command execution (Aegis safety), snapshots, journaling,
// filesystem perception, and telemetry.

#include "feather/hal.hpp"
#include "feather/ipc.hpp"
#include "feather/safety.hpp"
#include "feather/snapshot.hpp"
#include "feather/synapsed.hpp"
#include "feather/watch.hpp"

#include <atomic>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace featherd {

class EventBus {
public:
    void publish(std::string method, std::string params_json);
    using Handler = std::function<void(std::string /*method*/, std::string /*payload_json*/)>;
    std::size_t subscribe(Handler h);

private:
    std::mutex mutex_;
    std::map<std::size_t, Handler> subscribers_;
    std::size_t next_id_ = 0;
};

struct PendingCommand {
    std::string                                  session;
    std::string                                  command;
    std::filesystem::path                        cwd;
    std::chrono::steady_clock::time_point        created;
    bool                                         confirmed = false;
};

class Daemon {
public:
    Daemon(std::filesystem::path data_dir,
           std::filesystem::path workspace);

    int serve(std::string endpoint);

    std::string dispatch(std::string session, std::string method, std::string params_json);
    void emit(std::string method, std::string params_json);

    const std::filesystem::path& workspace() const noexcept { return workspace_; }

private:
    std::string cwd_for(std::string session, const std::string& params_json);
    std::string execute_command(std::string session,
                                std::string command,
                                std::filesystem::path cwd,
                                bool confirmed);

    std::filesystem::path data_dir_;
    std::filesystem::path workspace_;

    EventBus events_;
    std::atomic<bool> stopping_{false};

    std::mutex sessions_mutex_;
    std::map<std::string, std::filesystem::path> sessions_;

    std::mutex commands_mutex_;
    std::map<std::string, PendingCommand> pending_commands_;

    std::mutex journal_mutex_;
    feather::synapsed::SemanticJournal journal_;

    std::mutex snapshots_mutex_;
    feather::snapshot::SnapshotStore snapshot_store_;

    std::mutex safety_mutex_;
    feather::safety::SafetyEngine safety_engine_;

    std::mutex telemetry_mutex_;
    feather::watch::HardwareTelemetry telemetry_;

    std::mutex watcher_mutex_;
    std::unique_ptr<feather::watch::IWorkspaceWatcher> watcher_;

    std::mutex logs_mutex_;
    std::deque<std::string> logs_;
    std::size_t max_logs_ = 200;

    std::mutex actions_mutex_; // serializes command execution

    std::string gen_id();

    // Platform-specific connection handling
    void connection_loop(feather::ipc::PortalTransport transport);
};

} // namespace featherd