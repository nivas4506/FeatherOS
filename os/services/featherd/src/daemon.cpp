// FeatherOS featherd — Supervisor Daemon (C++20)
// Native OS layer only.
#include "featherd/featherd.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <sys/select.h>
#include <thread>
#include <unistd.h>

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
                    {"sandbox", "Aegis command policy; host-user execution"}}.dump();
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
// Connection handling (one thread per client)
// =============================================================================

void Daemon::connection_loop(int client_fd) {
    std::string session = gen_id();
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_[session] = workspace_;
    }

    while (!stopping_.load()) {
        // Read 4-byte big-endian length header
        std::array<std::uint8_t, 4> header{};
        std::size_t got = 0;
        while (got < 4) {
            ssize_t n = ::recv(client_fd, reinterpret_cast<char*>(header.data()) + got,
                               4 - got, 0);
            if (n <= 0) goto done;
            got += static_cast<std::size_t>(n);
        }
        std::uint32_t len = (static_cast<std::uint32_t>(header[0]) << 24) |
                            (static_cast<std::uint32_t>(header[1]) << 16) |
                            (static_cast<std::uint32_t>(header[2]) << 8) |
                            (static_cast<std::uint32_t>(header[3]));
        if (len == 0 || len > 8 * 1024 * 1024) goto done;

        std::string body;
        body.resize(len);
        std::size_t bread = 0;
        while (bread < len) {
            ssize_t n = ::recv(client_fd, body.data() + bread, len - bread, 0);
            if (n <= 0) goto done;
            bread += static_cast<std::size_t>(n);
        }

        json req;
        try { req = json::parse(body); } catch (...) {
            std::string err = feather::ipc::make_error(0, -32700, "Parse error");
            ::send(client_fd, err.data(), static_cast<int>(err.size()), 0);
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
        feather::ipc::write_frame(client_fd, out);
    }

done:
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.erase(session);
    }
    ::close(client_fd);
}

// =============================================================================
// Serve loop
// =============================================================================

int Daemon::serve(std::string endpoint) {
    std::error_code ec;
    std::filesystem::create_directories(data_dir_, ec);

#if !defined(_WIN32)
    std::filesystem::path sock_path(endpoint);
    if (std::filesystem::exists(sock_path)) {
        if (std::filesystem::is_symlink(sock_path) ||
            std::filesystem::is_regular_file(sock_path))
            std::filesystem::remove(sock_path, ec);
    }
    int listen_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        std::fprintf(stderr, "socket failed\n");
        return 1;
    }
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, endpoint.c_str(), sizeof(addr.sun_path) - 1);
    if (::bind(listen_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::fprintf(stderr, "bind failed\n");
        ::close(listen_fd);
        return 1;
    }
    ::chmod(endpoint.c_str(), 0600);
    if (::listen(listen_fd, 16) != 0) {
        ::close(listen_fd);
        return 1;
    }
    std::fprintf(stderr, "featherd listening on %s\n", endpoint.c_str());

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

    while (!stopping_.load()) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(listen_fd, &rfds);
        struct timeval tv{2, 0};
        int rv = ::select(listen_fd + 1, &rfds, nullptr, nullptr, &tv);
        if (rv < 0) break;
        if (rv == 0) continue;
        int client_fd = ::accept(listen_fd, nullptr, nullptr);
        if (client_fd < 0) continue;
        std::thread([this, client_fd] { connection_loop(client_fd); }).detach();
    }

    stopping_.store(true);
    perception_thread.join();
    ::close(listen_fd);
    std::filesystem::remove(endpoint, ec);
#else
    std::fprintf(stderr, "Named-pipe server not implemented in this build.\n");
    return 1;
#endif
    return 0;
}

} // namespace featherd
