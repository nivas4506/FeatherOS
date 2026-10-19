#pragma once
// FeatherOS Aegis Snapshot — Atomic Rollback Engine (C++20)
// Captures workspace state before/after execution; supports list/rollback/diff/recover.

#include "feather/hal.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace feather::snapshot {

struct SnapshotEntry {
    std::filesystem::path original;
    std::filesystem::path backup;
};

struct Snapshot {
    std::string                    id;
    std::vector<SnapshotEntry>     entries;
    std::string                    timestamp;
    std::filesystem::path          root;
};

enum class SnapshotStatus : uint8_t { InProgress, Complete, Discarded };

struct SnapshotRecord {
    std::string            id;
    SnapshotStatus         status   = SnapshotStatus::InProgress;
    std::string            timestamp;
    std::filesystem::path  root;
    std::vector<std::filesystem::path> changed_paths;
    std::vector<std::string> excluded_directories;
};

// Take a snapshot of `paths` under vfs data_dir/snapshots/<id>.
Snapshot take(const std::vector<std::filesystem::path>& paths,
              feather::hal::IVirtualFileSystem& vfs);

// Restore a snapshot's files via vfs (write backup -> original).
bool rollback(const Snapshot& snap, feather::hal::IVirtualFileSystem& vfs);

// ─── SnapshotStore ─────────────────────────────────────────────────────────────
// Persists snapshot metadata to data_dir/snapshots/ and tracks per-workspace state.

class SnapshotStore {
public:
    explicit SnapshotStore(std::filesystem::path store_dir,
                           std::vector<std::filesystem::path> watch_roots = {});

    // List snapshots for a workspace (root).
    std::vector<SnapshotRecord> list(const std::filesystem::path& root) const;

    // Load a snapshot record + entries by id.
    Snapshot load(const std::string& id) const;

    // Capture a new snapshot for `root` with `label`.
    SnapshotRecord capture(const std::filesystem::path& root,
                           std::string_view label,
                           feather::hal::IVirtualFileSystem& vfs);

    // Finalize an in-progress snapshot; returns the completed record.
    SnapshotRecord finish(const std::string& id);

    // Rollback to a snapshot; returns the restored record.
    SnapshotRecord rollback(const std::string& id,
                            feather::hal::IVirtualFileSystem& vfs);

    // Produce a unified-diff string for a snapshot id.
    std::string diff(const std::string& id) const;

    // Discard a snapshot whose changed_paths is empty.
    bool discard_unchanged(const SnapshotRecord& rec);

    // Excluded directories (common build/cache dirs).
    static const std::vector<std::string>& EXCLUDED_DIRS();

private:
    std::filesystem::path store_dir_;
    std::vector<std::filesystem::path> watch_roots_;
    mutable std::mutex mutex_;

    std::filesystem::path meta_path(const std::string& id) const;
    std::filesystem::path snap_dir(const std::string& id) const;
    void persist_meta(const SnapshotRecord& rec) const;
    SnapshotRecord read_meta(const std::string& id) const;
};

} // namespace feather::snapshot
