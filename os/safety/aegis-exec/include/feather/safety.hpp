#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// FeatherOS Aegis Safety Execution Engine — C++20
// Multi-tier command safety classification + sandboxed execution
// ─────────────────────────────────────────────────────────────────────────────
#include "feather/hal.hpp"

#include <functional>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace feather::safety {

using RiskTier = feather::hal::RiskTier;

struct SafetyVerdict {
    RiskTier    tier;
    std::string rule_name;   // which rule triggered
    std::string explanation;
};

// Classifies a shell command into a risk tier
SafetyVerdict classify(std::string_view command);

// Returns human-readable tier label
std::string_view tier_label(RiskTier t);

// Execution configuration for SafeExecutor.
struct ExecutionConfig {
    std::filesystem::path working_dir;
    std::optional<uint32_t> timeout_s;
    bool sandboxed = true;
};

// SafetyEngine — holds the rule table and assesses commands.
class SafetyEngine {
public:
    SafetyEngine();
    SafetyVerdict assess(std::string_view command) const;
};

// SafeExecutor — runs a single command through the Aegis policy using the HAL.
class SafeExecutor {
public:
    explicit SafeExecutor(const ExecutionConfig& cfg = ExecutionConfig{});

    feather::hal::ProcessOutput execute(std::string_view command);

    const ExecutionConfig& config() const noexcept { return config_; }

private:
    ExecutionConfig config_;
};

// Executes using aegis policy:
//   Safe/Moderate → run normally
//   High          → confirm_fn must return true
//   Critical      → never executed, returns blocked error
feather::hal::ProcessOutput aegis_execute(
    std::string_view command,
    const std::filesystem::path& working_dir,
    feather::hal::IProcessManager& pm,
    feather::hal::ISandboxDriver&  sandbox,
    std::function<bool(const SafetyVerdict&)> confirm_fn
);

} // namespace feather::safety
