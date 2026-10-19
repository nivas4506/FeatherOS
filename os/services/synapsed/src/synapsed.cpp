// FeatherOS synapsed — Semantic Memory Journal + Filesystem Indexer (C++20)
#include "feather/synapsed.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>

namespace feather::synapsed {

IndexStats FileSystemIndexer::stats_{};

// ─── JournalEntry helpers ─────────────────────────────────────────────────────

static std::string gen_id() {
    static std::mt19937_64 rng{std::random_device{}()};
    std::ostringstream oss;
    oss << std::hex << rng();
    return oss.str();
}

static std::int64_t now_seconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// Simple JSON string escaper (no external dep needed for this module).
static std::string json_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// ─── SemanticJournal ──────────────────────────────────────────────────────────

SemanticJournal::SemanticJournal() = default;

SemanticJournal::SemanticJournal(const std::filesystem::path& journal_path)
    : journal_path_(journal_path) {}

void SemanticJournal::record(std::string_view type, std::string_view path,
                              std::string_view content,
                              std::vector<std::string> tags) {
    JournalEntry e;
    e.id        = gen_id();
    e.type      = type;
    e.path      = path;
    e.content   = content;
    e.tags      = std::move(tags);
    e.indexed_at = now_seconds();
    entries_.push_back(std::move(e));
    ++next_id_;
}

static bool contains_ci(std::string_view hay, std::string_view needle) {
    auto h = std::string(hay);
    auto n = std::string(needle);
    std::transform(h.begin(), h.end(), h.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    std::transform(n.begin(), n.end(), n.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return h.find(n) != std::string::npos;
}

std::vector<JournalEntry> SemanticJournal::search(std::string_view query,
                                                   std::size_t limit) const {
    std::vector<JournalEntry> result;
    for (const auto& e : entries_) {
        if (contains_ci(e.content, query) || contains_ci(e.path, query)) {
            result.push_back(e);
            if (result.size() >= limit) break;
        }
    }
    return result;
}

void SemanticJournal::remove_files_under(const std::filesystem::path& prefix) {
    auto abs_prefix = std::filesystem::absolute(prefix);
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
        [&](const JournalEntry& e) {
            try {
                auto p = std::filesystem::absolute(e.path);
                return p.string().find(abs_prefix.string()) == 0;
            } catch (...) { return false; }
        }),
        entries_.end());
}

SemanticJournal SemanticJournal::load(const std::filesystem::path& path) {
    SemanticJournal j(path);
    if (!std::filesystem::exists(path)) return j;
    std::ifstream f(path);
    if (!f) return j;
    std::string content((std::istreambuf_iterator<char>(f)), {});
    if (content.empty() || content.find('[') == std::string::npos) return j;
    (void)content;
    return j;
}

void SemanticJournal::persist() const {
    if (journal_path_.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(journal_path_.parent_path(), ec);
    std::ofstream f(journal_path_);
    if (!f) return;
    f << "{\n  \"version\": 1,\n  \"entries\": [\n";
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];
        f << "    {\n";
        f << "      \"id\": \"" << json_escape(e.id) << "\",\n";
        f << "      \"type\": \"" << json_escape(e.type) << "\",\n";
        f << "      \"path\": \"" << json_escape(e.path) << "\",\n";
        f << "      \"content\": \"" << json_escape(e.content) << "\",\n";
        f << "      \"tags\": [";
        for (std::size_t t = 0; t < e.tags.size(); ++t) {
            if (t) f << ", ";
            f << "\"" << json_escape(e.tags[t]) << "\"";
        }
        f << "],\n";
        f << "      \"indexed_at\": " << e.indexed_at << "\n";
        f << "    }";
        if (i + 1 < entries_.size()) f << ",";
        f << "\n";
    }
    f << "  ]\n}\n";
}

// ─── FileSystemIndexer ────────────────────────────────────────────────────────

bool FileSystemIndexer::is_excluded(const std::filesystem::path& path,
                                     const std::vector<std::filesystem::path>& exclude_roots) {
    for (const auto& root : exclude_roots) {
        try {
            auto abs_root = std::filesystem::absolute(root);
            auto abs_path = std::filesystem::absolute(path);
            if (abs_path == abs_root) return true;
            if (abs_path.string().find(abs_root.string()) == 0) return true;
        } catch (...) {}
    }
    const std::string name = path.filename().string();
    if (name == ".git" || name == ".feather" || name == "target" ||
        name == "node_modules" || name == "__pycache__" || name == ".cache")
        return true;
    return false;
}

bool FileSystemIndexer::eligible(const std::filesystem::path& /*root*/,
                                  const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return false;
    if (std::filesystem::is_directory(path, ec)) return false;
    auto fsize = std::filesystem::file_size(path, ec);
    if (ec || fsize == 0 || fsize > 2 * 1024 * 1024) return false;
    const std::string ext = path.extension().string();
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" ||
        ext == ".ico" || ext == ".bin" || ext == ".o" || ext == ".a" ||
        ext == ".so" || ext == ".dll" || ext == ".exe" || ext == ".pdf" ||
        ext == ".zip" || ext == ".tar" || ext == ".gz")
        return false;
    return true;
}

std::size_t FileSystemIndexer::index_file(SemanticJournal& journal,
                                           const std::filesystem::path& path) {
    if (!eligible(std::filesystem::path("."), path)) return 0;
    std::ifstream f(path, std::ios::binary);
    if (!f) return 0;
    std::string content((std::istreambuf_iterator<char>(f)), {});
    journal.record("file", path.string(), content,
                   {path.extension().string(), path.parent_path().filename().string()});
    ++stats_.indexed_files;
    ++stats_.total_entries;
    return 1;
}

std::size_t FileSystemIndexer::index_directory_excluding(
    SemanticJournal& journal, const std::filesystem::path& root,
    std::size_t max_files,
    const std::vector<std::filesystem::path>& exclude_roots) {
    std::size_t count = 0;
    std::error_code ec;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(root,
             std::filesystem::directory_options::skip_permission_denied, ec)) {
        if (ec) break;
        const auto& p = entry.path();
        if (is_excluded(p, exclude_roots)) {
            ++stats_.excluded_paths;
            continue;
        }
        if (!eligible(root, p)) continue;
        if (count >= max_files) break;
        count += index_file(journal, p);
    }
    return count;
}

} // namespace feather::synapsed
