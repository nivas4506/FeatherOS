// FeatherOS Aegis Snapshot — Atomic Rollback Engine (C++20)
#include "feather/snapshot.hpp"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <mutex>
#include <random>
#include <sstream>

namespace feather::snapshot {

static std::string gen_id() {
    static std::mt19937_64 rng{std::random_device{}()};
    std::ostringstream oss;
    oss << std::hex << rng();
    return oss.str();
}

static std::string now_iso() {
    auto now = std::time(nullptr);
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y%m%dT%H%M%S", std::gmtime(&now));
    return ts;
}

// ─── take / rollback ──────────────────────────────────────────────────────────

Snapshot take(const std::vector<std::filesystem::path>& paths,
              feather::hal::IVirtualFileSystem& vfs) {
    Snapshot snap;
    snap.id      = gen_id();
    snap.timestamp = now_iso();
    auto snap_root = vfs.data_dir() / "snapshots" / snap.id;
    std::error_code ec;
    std::filesystem::create_directories(snap_root, ec);

    for (const auto& p : paths) {
        if (!vfs.exists(p)) continue;
        auto backup = snap_root / p.filename();
        try {
            vfs.write_atomic(backup, vfs.read_to_string(p));
            snap.entries.push_back({p, backup});
        } catch (...) {}
    }
    return snap;
}

bool rollback(const Snapshot& snap, feather::hal::IVirtualFileSystem& vfs) {
    bool ok = true;
    for (const auto& e : snap.entries) {
        try {
            vfs.write_atomic(e.original, vfs.read_to_string(e.backup));
        } catch (...) { ok = false; }
    }
    return ok;
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

static std::string json_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

static bool is_excluded_name(const std::filesystem::path& p) {
    const std::string n = p.filename().string();
    return n == ".git" || n == ".feather" || n == "target" ||
           n == "node_modules" || n == "__pycache__" || n == ".cache";
}

// ─── SnapshotStore ────────────────────────────────────────────────────────────

namespace {
const std::vector<std::string> kExcludedDirs = {
    ".git", ".feather", "target", "node_modules", "__pycache__"
};
} // namespace

const std::vector<std::string>& SnapshotStore::EXCLUDED_DIRS() {
    return kExcludedDirs;
}

SnapshotStore::SnapshotStore(std::filesystem::path store_dir,
                              std::vector<std::filesystem::path> watch_roots)
    : store_dir_(std::move(store_dir)), watch_roots_(std::move(watch_roots)) {
    std::error_code ec;
    std::filesystem::create_directories(store_dir_, ec);
}

std::filesystem::path SnapshotStore::meta_path(const std::string& id) const {
    return store_dir_ / (id + ".json");
}

std::filesystem::path SnapshotStore::snap_dir(const std::string& id) const {
    return store_dir_ / id;
}

void SnapshotStore::persist_meta(const SnapshotRecord& rec) const {
    std::error_code ec;
    std::filesystem::create_directories(store_dir_, ec);
    std::ofstream f(meta_path(rec.id));
    if (!f) return;
    f << "{\n";
    f << "  \"id\": \"" << json_escape(rec.id) << "\",\n";
    f << "  \"status\": \"" << (rec.status == SnapshotStatus::Complete ? "complete"
                  : rec.status == SnapshotStatus::Discarded ? "discarded" : "in_progress") << "\",\n";
    f << "  \"timestamp\": \"" << json_escape(rec.timestamp) << "\",\n";
    f << "  \"root\": \"" << json_escape(rec.root.string()) << "\",\n";
    f << "  \"changed_paths\": [";
    for (std::size_t i = 0; i < rec.changed_paths.size(); ++i) {
        if (i) f << ", ";
        f << "\"" << json_escape(rec.changed_paths[i].string()) << "\"";
    }
    f << "],\n";
    f << "  \"excluded_directories\": [";
    for (std::size_t i = 0; i < rec.excluded_directories.size(); ++i) {
        if (i) f << ", ";
        f << "\"" << json_escape(rec.excluded_directories[i]) << "\"";
    }
    f << "]\n";
    f << "}\n";
}

SnapshotRecord SnapshotStore::read_meta(const std::string& id) const {
    SnapshotRecord rec;
    rec.id = id;
    std::ifstream f(meta_path(id));
    if (!f) return rec;
    std::string content((std::istreambuf_iterator<char>(f)), {});
    // Very crude parse: extract known fields
    auto find_str = [&](std::string_view hay, std::string_view key) -> std::string {
        std::string k = "\"" + std::string(key) + "\":\"";
        auto pos = hay.find(k);
        if (pos == std::string::npos) return {};
        auto start = pos + k.size();
        auto end = hay.find('"', start);
        if (end == std::string::npos) return {};
        return std::string(hay.substr(start, end - start));
    };
    rec.timestamp = find_str(content, "timestamp");
    rec.root = find_str(content, "root");
    std::string status = find_str(content, "status");
    if (status == "complete") rec.status = SnapshotStatus::Complete;
    else if (status == "discarded") rec.status = SnapshotStatus::Discarded;
    else rec.status = SnapshotStatus::InProgress;
    return rec;
}

std::vector<SnapshotRecord> SnapshotStore::list(const std::filesystem::path& root) const {
    std::vector<SnapshotRecord> out;
    std::error_code ec;
    if (!std::filesystem::exists(store_dir_, ec)) return out;
    for (const auto& entry : std::filesystem::directory_iterator(store_dir_, ec)) {
        if (ec) break;
        if (entry.path().extension() != ".json") continue;
        auto id = entry.path().stem().string();
        auto rec = read_meta(id);
        if (rec.root == root) out.push_back(std::move(rec));
    }
    std::sort(out.begin(), out.end(),
              [](const SnapshotRecord& a, const SnapshotRecord& b) {
                  return a.timestamp > b.timestamp;
              });
    return out;
}

Snapshot SnapshotStore::load(const std::string& id) const {
    Snapshot snap;
    snap.id = id;
    auto rec = read_meta(id);
    snap.timestamp = rec.timestamp;
    snap.root = rec.root;
    // Read entries from snapshot dir
    auto dir = snap_dir(id);
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) return snap;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (entry.path().extension() == ".feather_tmp") continue;
        SnapshotEntry e;
        e.backup = entry.path();
        // original path stored in a sidecar .orig file
        auto orig = entry.path();
        orig.replace_extension(".orig");
        std::ifstream of(orig);
        if (of) {
            std::string orig_path((std::istreambuf_iterator<char>(of)), {});
            if (!orig_path.empty() && orig_path.back() == '\n')
                orig_path.pop_back();
            e.original = orig_path;
        }
        snap.entries.push_back(std::move(e));
    }
    return snap;
}

SnapshotRecord SnapshotStore::capture(const std::filesystem::path& root,
                                       std::string_view label,
                                       feather::hal::IVirtualFileSystem& vfs) {
    SnapshotRecord rec;
    rec.id = gen_id();
    rec.timestamp = now_iso();
    rec.root = root;
    rec.status = SnapshotStatus::InProgress;
    rec.excluded_directories = kExcludedDirs;

    // Collect paths to snapshot: all regular files under root (excluding excluded)
    std::vector<std::filesystem::path> paths;
    std::error_code ec;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(root,
             std::filesystem::directory_options::skip_permission_denied, ec)) {
        if (ec) break;
        const auto& p = entry.path();
        if (is_excluded_name(p)) continue;
        if (std::filesystem::is_regular_file(p, ec))
            paths.push_back(p);
    }

    auto snap = take(paths, vfs);
    // Write original-path sidecars
    auto dir = snap_dir(rec.id);
    std::error_code ec2;
    std::filesystem::create_directories(dir, ec2);
    for (const auto& e : snap.entries) {
        auto backup = dir / e.backup.filename();
        std::ofstream sidecar(backup.string() + ".orig");
        sidecar << e.original.string() << "\n";
    }

    // Compute changed paths (all snapshotted paths for initial capture)
    rec.changed_paths = paths;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        persist_meta(rec);
    }
    return rec;
}

SnapshotRecord SnapshotStore::finish(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto rec = read_meta(id);
    rec.status = SnapshotStatus::Complete;
    persist_meta(rec);
    return rec;
}

SnapshotRecord SnapshotStore::rollback(const std::string& id,
                                        feather::hal::IVirtualFileSystem& vfs) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto snap = load(id);
    bool ok = rollback(snap, vfs);
    auto rec = read_meta(id);
    if (ok) rec.status = SnapshotStatus::Complete;
    persist_meta(rec);
    return rec;
}

std::string SnapshotStore::diff(const std::string& id) const {
    (void)id;
    return ""; // placeholder: full diff would compare backup vs original content
}

bool SnapshotStore::discard_unchanged(const SnapshotRecord& rec) {
    if (rec.changed_paths.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::error_code ec;
        std::filesystem::remove(meta_path(rec.id), ec);
        std::filesystem::remove_all(snap_dir(rec.id), ec);
        return true;
    }
    return false;
}

} // namespace feather::snapshot
