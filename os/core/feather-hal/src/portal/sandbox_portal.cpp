/*
 * Portal SandboxDriver Backend
 * Implements feather::hal::SandboxDriver using portal_spawn_restricted.
 */

#include "feather/hal.hpp"
#include <filesystem>
#include <string>

namespace feather::hal {

class PortalSandboxDriver final : public SandboxDriver {
public:
    PortalSandboxDriver() = default;

    std::string wrap_command(std::string_view original_cmd,
                             const std::filesystem::path& working_dir,
                             SandboxKind kind) const override
    {
        // For Portal kernel: all sandbox kinds map to restricted spawn
        // The actual isolation is enforced by kernel via capability mask
        (void)kind;

        // Escape single quotes for shell safety
        std::string escaped = shell_escape(original_cmd);

        // Construct portal_spawn_restricted command
        // Format: portal_spawn_restricted --cwd=/path --caps=read,write,create -- <command>
        std::string caps = "read,write,create"; // Default caps
        std::string cwd = working_dir.string();

        return "portal_spawn_restricted --cwd=" + shell_escape(cwd) +
               " --caps=" + caps + " -- " + escaped;
    }

    SandboxKind detect_capability() const noexcept override {
        // Portal kernel always supports restricted spawn
        return SandboxKind::UniversalRestricted;
    }

private:
    static std::string shell_escape(std::string_view s) {
        std::string result;
        result.reserve(s.size() + 2);
        result += '\'';
        for (char c : s) {
            if (c == '\'') {
                result += "'\\''"; // ' -> '\'' (end quote, escaped quote, start quote)
            } else {
                result += c;
            }
        }
        result += '\'';
        return result;
    }
};

std::unique_ptr<SandboxDriver> create_sandbox_driver() {
    return std::make_unique<PortalSandboxDriver>();
}

} // namespace feather::hal