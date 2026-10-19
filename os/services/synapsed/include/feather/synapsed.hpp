#pragma once
// FeatherOS synapsed — Semantic Memory Journal + Filesystem Indexer (C++20)
// Persists a JSON journal of indexed file contents and shell command history,
// supports text-based search. (No embedding/AI dependency — native OS only.)

#include "feather/hal.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace feather::synapsed {

struct JournalEntry {
    std::string  id;
    std::string  type;          // "file" | "shell_command" | "perception"
    std::string  content;       // indexed text content
    std::string  path;          // source path (file) or command
    std::vector<std::string> tags;
    std::int64_t   indexed_at;  // unix seconds
};

struct IndexStats {
    std::size_t total_entries   = 0;
    std::size_t indexed_files   = 0;
    std::size_t excluded_paths  = 0;
};

// Semantic journal: append entries, search by text match, persist to JSON.
class SemanticJournal {
public:
    SemanticJournal();
    explicit SemanticJournal(const std::filesystem::path& journal_path);

    // Append a new entry; assigns an id.
    void record(std::string_view type, std::string_view path,
                std::string_view content,
                std::vector<std::string> tags = {});

    // Search: returns up to `limit` entries whose content contains `query`.
    std::vector<JournalEntry> search(std::string_view query,
                                     std::size_t limit = 10) const;

    std::size_t total_entries() const noexcept { return entries_.size(); }

    // Remove all entries whose path is under `prefix`.
    void remove_files_under(const std::filesystem::path& prefix);

    // Load / persist the journal as JSON.
    static SemanticJournal load(const std::filesystem::path& path);
    void persist() const;

private:
    std::filesystem::path journal_path_;
    std::vector<JournalEntry> entries_;
    std::uint64_t next_id_ = 1;
};

// Filesystem indexer: walks a directory tree and indexes eligible files into the journal.
class FileSystemIndexer {
public:
    // Index every eligible file under `root` (max `max_files`), skipping paths in
    // `exclude_roots`. Returns number of newly indexed entries.
    static std::size_t index_directory_excluding(SemanticJournal& journal,
        const std::filesystem::path& root, std::size_t max_files,
        const std::vector<std::filesystem::path>& exclude_roots);

    // Index a single file.
    static std::size_t index_file(SemanticJournal& journal,
                                  const std::filesystem::path& path);

    // True if `path` is eligible for indexing (not hidden, not binary, size < cap).
    static bool eligible(const std::filesystem::path& /*root*/,
                         const std::filesystem::path& path);

    static IndexStats stats() noexcept { return stats_; }

private:
    static IndexStats stats_;
    static bool is_excluded(const std::filesystem::path& path,
                            const std::vector<std::filesystem::path>& exclude_roots);
};

} // namespace feather::synapsed
