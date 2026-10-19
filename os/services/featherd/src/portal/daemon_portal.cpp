// FeatherOS featherd — Supervisor Daemon (C++20) — PORTAL Kernel Backend
// Uses Portal IPC for communication, HAL for all OS operations.

#include "featherd/featherd.hpp"
#include <feather/ipc.hpp>
#include <nlohmann/json.hpp>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include <random>
#include <sstream>

namespace featherd {
using json = nlohmann::json;

namespace {
std::string gen_random_id() {
    static std::mt19937_64 rng{std::random_device{}()};
    std::ostringstream oss;
    oss << std::hex << rng();
    return oss.str();
}

json telemetry_to_json(const feather::watch::HardwareTelemetry& t) {
    return {{"arch", t.arch},
            {"kernel_version", t.kernel_version},
            {"os_name", t.os_name},
            {"hostname", t.hostname},
            {"cpu_cores", static_cast<json::number_integer_t>(t.cpu_cores)},
            {"cpu_usage_pct", t.cpu_usage_pct},
            {"mem_total_mb", static_cast<json::number_integer_t>(t.mem_total_mb)},
            {"mem_available_mb", static_cast<json::number_integer_t>(t.mem_available_mb)},
            {"disk_available_gb", static_cast<json::number_integer_t>(t.disk_available_gb)}};
}
} // namespace

// =============================================================================
// EventBus
// =============================================================================

void EventBus::publish(std::string method, std::string params_json) {
    std::lock_guard<std::mutex> lock(mutex_);
    json event;
    event["method"] = method;
    try { event["params"] = json::parse(params_json); } catch (...) { event["params"] = json::object(); }
    std::string payload = event.dump();
    for (auto& [id, h] : subscribers_) {
        try { h(method, payload); } catch (...) {}
    }
}

std::size_t EventBus::subscribe(Handler h) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t id = next_id_++;
    subscribers_[id] = std::move(h);
    return id;
}

// =============================================================================
// Daemon
// =============================================================================

Daemon::Daemon(std::filesystem::path data_dir,
               std::filesystem::path workspace)
    : data_dir_(std::move(data_dir)),
      workspace_(std::move(workspace)) {
    std::error_code ec;
    std::filesystem::create_directories(data_dir_, ec);
    std::filesystem::create_directories(workspace_, ec);
    journal_ = feather::synapsed::SemanticJournal::load(data_dir_ / "journal.json");
    snapshot_store_ = feather::snapshot::SnapshotStore(data_dir_ / "snapshots",
                                                        {workspace_});
}

std::string Daemon::gen_id() {
    return gen_random_id();
}

std::string Daemon::cwd_for(std::string session, const std::string& params_json) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session);
    std::filesystem::path current = (it != sessions_.end()) ? it->second : workspace_;
    std::filesystem::path result = current;
    try {
        auto p = json::parse(params_json);
        if (p.contains("working_dir") && p["working_dir"].is_string()) {
            result = (current / p["working_dir"].get<std::string>()).lexically_normal();
        }
    } catch (...) {}
    if (!std::filesystem::is_directory(result)) result = current;
    sessions_[session] = result;
    return result.string();
}

void Daemon::emit(std::string method, std::string params_json) {
    std::lock_guard<std::mutex> lock(logs_mutex_);
    json event;
    event["method"] = method;
    try { event["params"] = json::parse(params_json); } catch (...) { event["params"] = json::object(); }
    if (event["params"].contains("result") && event["params"]["result"].is_object())
        event["params"]["result"].erase("stdout");
    logs_.push_back(event.dump());
    if (logs_.size() > max_logs_) logs_.pop_front();
    events_.publish(method, params_json);
}

std::string Daemon::dispatch(std::string session, std::string method,
                              std::string params_json) {
    if (stopping_.load())
        return json{{"error", "Daemon is shutting down"}}.dump();

    json params;
    try { params = json::parse(params_json); } catch (...) { params = json::object(); }

    if (method == "initialize") {
        return json{{"jsonrpc", "2.0"},
                    {"protocolVersion", "2024-11-05"},
                    {"capabilities", {{"tools", {}}}},
                    {"serverInfo", {{"name", "featherd"}, {"version", "0.2.0"}}}}}.dump();
    }
    if (method == "ping") return json{{}}.dump();

    if (method == "session.open") {
        std::string cwd = cwd_for(session, params_json);
        {
            std::lock_guard<std::mutex> wl(watcher_mutex_);
            if (watcher_) watcher_->watch(cwd);
        }
        return json{{"session_id", session}, {"working_dir", cwd}}.dump();
    }

    if (method == "daemon.status") {
        std::lock_guard<std::mutex> sl(sessions_mutex_);
        std::lock_guard<std::mutex> jl(journal_mutex_);
        std::lock_guard<std::mutex> tl(telemetry_mutex_);
        return json{{"sessions", static_cast<json::number_integer_t>(sessions_.size())},
                    {"entries", static_cast<json::number_integer_t>(journal_.total_entries())},
                    {"workspace", workspace_.string()},
                    {"telemetry", telemetry_to_json(telemetry_)},
                    {"sandbox", "Portal restricted spawn; capability-based isolation"}}.dump();
    }

    if (method == "daemon.logs") {
        std::lock_guard<std::mutex> l(logs_mutex_);
        return json(logs_).dump();
    }

    if (method == "memory.search") {
        std::string query = params.value("query", "");
        std::lock_guard<std::mutex> jl(journal_mutex_);
        auto results = journal_.search(query, 10);
        json arr = json::array();
        for (const auto& e : results)
            arr.push_back({{"id", e.id}, {"type", e.type},
                           {"path", e.path}, {"content", e.content}});
        return arr.dump();
    }

    if (method == "snapshot.list" || method == "snapshot.rollback" ||
        method == "snapshot.diff" || method == "snapshot.recover") {
        std::string cwd = cwd_for(session, params_json);
        std::lock_guard<std::mutex> sl(snapshots_mutex_);
        auto snaps = snapshot_store_.list(cwd);
        json arr = json::array();
        for (const auto& s : snaps) {
            json sj;
            sj["id"] = s.id;
            sj["created"] = s.timestamp;
            sj["status"] = (s.status == feather::snapshot::SnapshotStatus::Complete) ? "complete" : "in_progress";
            sj["changed_paths"] = json::array();
            for (const auto& p : s.changed_paths)
                sj["changed_paths"].push_back(p.string());
            arr.push_back(sj);
        }
        if (method == "snapshot.list") return arr.dump();

        std::string target_id = params.value("snapshot_id", std::string{});
        if (target_id.empty()) {
            for (const auto& s : snaps) {
                if (s.status == feather::snapshot::SnapshotStatus::Complete &&
                    !s.changed_paths.empty()) { target_id = s.id; break; }
            }
        }
        if (target_id.empty()) {
            return json{{"error", "No completed snapshot for this workspace"}}.dump();
        }

        if (method == "snapshot.diff") {
            std::string d = snapshot_store_.diff(target_id);
            return json{{"diff", d}}.dump();
        }

        std::lock_guard<std::mutex> jl(journal_mutex_);
        auto vfs = feather::hal::make_vfs();
        feather::snapshot::SnapshotRecord rec;
        if (method == "snapshot.recover")
            rec = snapshot_store_.finish(target_id);
        else
            rec = snapshot_store_.rollback(target_id, *vfs);
        return json{{"snapshot_id", rec.id},
                    {"status",
                     (rec.status == feather::snapshot::SnapshotStatus::Complete ?
                      "complete" : "in_progress")}}.dump();
    }

    if (method == "memory.index") {
        std::string cwd = cwd_for(session, params_json);
        std::lock_guard<std::mutex> jl(journal_mutex_);
        std::vector<std::filesystem::path> exclude{data_dir_};
        auto count = feather::synapsed::FileSystemIndexer::index_directory_excluding(
            journal_, cwd, 10000, exclude);
        journal_.persist();
        return json{{"indexed", static_cast<json::number_integer_t>(count)}}.dump();
    }

    if (method == "command.execute") {
        std::string command = params.value("command", "");
        bool confirmed = params.value("confirmed", false);
        std::string cwd = cwd_for(session, params_json);
        return execute_command(session, command, cwd, confirmed);
    }

    return json{{"error", "Unknown method: " + method}}.dump();
}

std::string Daemon::execute_command(std::string session,
                                     std::string command,
                                     std::filesystem::path cwd,
                                     bool confirmed) {
    std::string task_id = gen_id();
    emit("command.queued",
         json{{"session_id", session}, {"task_id", task_id}, {"command", command}}.dump());

    std::lock_guard<std::mutex> ag(actions_mutex_);
    if (stopping_.load()) return json{{"status", "blocked"}}.dump();

    // Safety assessment
    auto assessment = safety_engine_.assess(command);
    if (assessment.tier == feather::hal::RiskTier::Critical) {
        emit("command.blocked",
             json{{"session_id", session}, {"task_id", task_id},
                  {"command", command}, {"reason", assessment.explanation}}.dump());
        return json{{"status", "blocked"},
                    {"task_id", task_id},
                    {"reason", assessment.explanation}}.dump();
    }
    if (assessment.tier == feather::hal::RiskTier::High && !confirmed) {
        emit("command.awaiting_confirmation",
             json{{"session_id", session}, {"task_id", task_id},
                  {"command", command}, {"tier", "high"}}.dump());
        return json{{"status", "awaiting_confirmation"},
                    {"task_id", task_id},
                    {"tier", "high"},
                    {"explanation", assessment.explanation}}.dump();
    }

    // Pre-execution snapshot
    {
        std::lock_guard<std::mutex> sl(snapshots_mutex_);
        auto vfs = feather::hal::make_vfs();
        snapshot_store_.capture(cwd, command, *vfs);
    }

    feather::safety::SafeExecutor executor;
    auto result = executor.execute(command);

    json rj;
    rj["exit_code"] = result.exit_code;
    rj["stdout"] = result.stdout_data;
    rj["stderr"] = result.stderr_data;
    rj["duration_ms"] = static_cast<json::number_integer_t>(result.duration_ms);

    {
        std::lock_guard<std::mutex> jl(journal_mutex_);
        journal_.record("shell_command", command,
                        result.stdout_data + "\n" + result.stderr_data,
                        {"session:" + session});
    }
    emit("command.finished",
         json{{"session_id", session}, {"task_id", task_id},
               {"command", command}, {"result", rj}}.dump());

    std::string snapshot_id;
    {
        std::lock_guard<std::mutex> sl(snapshots_mutex_);
        snapshot_id = gen_id().substr(0, 8);
    }
    {
        std::lock_guard<std::mutex> jl(journal_mutex_);
        journal_.persist();
    }

    return json{{"status", result.exit_code == 0 ? "completed" : "failed"},
                {"task_id", task_id},
                {"result", rj},
                {"snapshot_id", snapshot_id}}.dump();
}

// =============================================================================
// Connection handling — uses feather::ipc::PortalTransport
// =============================================================================

void Daemon::connection_loop(feather::ipc::PortalTransport transport) {
    std::string session = gen_id();
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_[session] = workspace_;
    }

    while (!stopping_.load()) {
        std::string request = transport.read_frame();
        if (request.empty()) break; // Client disconnected

        json req;
        try { req = json::parse(request); } catch (...) {
            json err = json{{"jsonrpc", "2.0"}, {"id", nullptr},
                            {"error", {{"code", -32700}, {"message", "Parse error"}}}};
            transport.write_frame(err.dump());
            continue;
        }

        std::string method = req.value("method", "");
        json params = req.value("params", json::object());
        json id_val = req.value("id", json::value_t::null);

        std::string result_json = dispatch(session, method, params.dump());
        json resp;
        resp["jsonrpc"] = "2.0";
        resp["result"] = json::parse(result_json, nullptr, false);
        if (!id_val.is_null()) resp["id"] = id_val;
        std::string out = resp.dump();
        
        if (!transport.write_frame(out)) break;
    }

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.erase(session);
    }
}

// =============================================================================
// Serve loop — listens on Portal IPC
// =============================================================================

int Daemon::serve(std::string endpoint) {
    std::error_code ec;
    std::filesystem::create_directories(data_dir_, ec);

    std::fprintf(stderr, "featherd listening on portal: %s\n", endpoint.c_str());

    // Start perception background thread (telemetry sampling)
    std::thread perception_thread([this] {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        while (!stopping_.load()) {
            {
                std::lock_guard<std::mutex> tl(telemetry_mutex_);
                telemetry_ = feather::watch::Telemetry().sample(workspace_);
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    });

    // For PORTAL kernel, we accept connections via portal_connect
    // This is a simplified single-threaded accept loop
    // Real implementation would use portal_listen/portal_accept or a thread pool
    portal_handle_t listen_portal = PORTAL_HANDLE_INVALID;
    int ret = portal_create(endpoint.c_str(), PORTAL_CAP_IPC_LISTEN, &listen_portal);
    if (ret < 0) {
        std::fprintf(stderr, "portal_create failed: %s\n", std::strerror(-ret));
        return 1;
    }

    while (!stopping_.load()) {
        // Accept new connection (portal_accept would be the syscall)
        // For MVP: simulate with portal_recv on the listening portal
        portal_message_t msg_buf{};
        size_t cap_count = 0;
        ret = portal_recv(listen_portal, &msg_buf, nullptr, 0, &cap_count);
        if (ret < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Got a new client connection - the capability handle is the client portal
        if (cap_count > 0) {
            portal_handle_t client_portal = cap_count > 0 ? 
                reinterpret_cast<portal_handle_t*>(msg_buf.data)[0] : PORTAL_HANDLE_INVALID;
            if (client_portal != PORTAL_HANDLE_INVALID) {
                feather::ipc::PortalTransport transport(client_portal);
                std::thread([this, transport = std::move(transport)]() mutable {
                    connection_loop(std::move(transport));
                }).detach();
            }
        }
    }

    stopping_.store(true);
    perception_thread.join();
    portal_close(listen_portal);
    return 0;
}

} // namespace featherd