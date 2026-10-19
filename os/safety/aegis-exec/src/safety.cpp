// FeatherOS Aegis Safety Execution Engine — C++20
#include "feather/safety.hpp"
#include <array>
#include <algorithm>

namespace feather::safety {

// ─── Rule Table ──────────────────────────────────────────────────────────────
struct Rule {
    std::regex  pattern;
    RiskTier    tier;
    const char* name;
    const char* explanation;
};

static const std::vector<Rule>& rules() {
    static std::vector<Rule> table = [] {
        using namespace std::string_literals;
        std::vector<Rule> r;
        auto add = [&](const char* pat, RiskTier t, const char* name, const char* expl) {
            r.push_back({std::regex(pat, std::regex::ECMAScript | std::regex::icase), t, name, expl});
        };
        // ── CRITICAL ────────────────────────────────────────────────────────
        add(R"(rm\s+-[a-zA-Z]*r[a-zA-Z]*f\s+/($|\s))",
            RiskTier::Critical, "recursive-root-delete",
            "Recursive deletion of root directory — catastrophic");
        add(R"(mkfs\b)", RiskTier::Critical, "mkfs", "Formats a block device — data destruction");
        add(R"(dd\s+.*of=/dev/(sd|nvme|hd|vd))",
            RiskTier::Critical, "dd-device", "Writing directly to block device — data loss");
        add(R"(:(){ :|:& };:)", RiskTier::Critical, "fork-bomb", "Fork bomb — system freeze");
        add(R"(>(>?)\s*/dev/sda)", RiskTier::Critical, "overwrite-disk", "Overwriting disk device");
        add(R"(chmod\s+-R\s+777\s+/)", RiskTier::Critical, "chmod-777-root", "World-writeable root");

        // ── HIGH ────────────────────────────────────────────────────────────
        add(R"(\brm\b.*-[a-zA-Z]*r)", RiskTier::High, "recursive-remove", "Recursive file removal");
        add(R"(\brmdir\b)",           RiskTier::High, "rmdir",            "Directory removal");
        add(R"(\bmv\b)",              RiskTier::High, "move",             "File/dir move — data change");
        add(R"(\bchmod\b)",           RiskTier::High, "chmod",            "Permission change");
        add(R"(\bkill\b)",            RiskTier::High, "kill",             "Process termination");
        add(R"(\bkillall\b)",         RiskTier::High, "killall",          "Process group termination");
        add(R"(\bsystemctl\b)",       RiskTier::High, "systemctl",        "Service management");
        add(R"(\bsudo\b)",            RiskTier::High, "sudo",             "Privilege escalation");

        // ── MODERATE ────────────────────────────────────────────────────────
        add(R"(\btouch\b)",   RiskTier::Moderate, "touch",  "File creation");
        add(R"(\bmkdir\b)",   RiskTier::Moderate, "mkdir",  "Directory creation");
        add(R"(\bcp\b)",      RiskTier::Moderate, "copy",   "File copy");
        add(R"(\btee\b)",     RiskTier::Moderate, "tee",    "Write to file via tee");
        add(R"(>(?!>))",      RiskTier::Moderate, "redirect-overwrite", "Output redirection — overwrite");
        add(R"(>>)",          RiskTier::Moderate, "redirect-append",    "Output redirection — append");

        return r;
    }();
    return table;
}

// ─── Classifier ───────────────────────────────────────────────────────────────
SafetyVerdict classify(std::string_view command) {
    std::string cmd(command);
    for (const auto& rule : rules()) {
        if (std::regex_search(cmd, rule.pattern))
            return {rule.tier, rule.name, rule.explanation};
    }
    return {RiskTier::Safe, "safe", "No dangerous patterns detected"};
}

std::string_view tier_label(RiskTier t) {
    switch (t) {
    case RiskTier::Safe:     return "SAFE";
    case RiskTier::Moderate: return "MODERATE";
    case RiskTier::High:     return "HIGH RISK";
    case RiskTier::Critical: return "CRITICAL";
    }
    return "UNKNOWN";
}

// ─── SafetyEngine ────────────────────────────────────────────────────────────
SafetyEngine::SafetyEngine() = default;

SafetyVerdict SafetyEngine::assess(std::string_view command) const {
    return classify(command);
}

// ─── SafeExecutor ────────────────────────────────────────────────────────────
SafeExecutor::SafeExecutor(const ExecutionConfig& cfg) : config_(cfg) {}

feather::hal::ProcessOutput SafeExecutor::execute(std::string_view command) {
    // Default confirm_fn: auto-approve Safe/Moderate, deny High/Critical.
    auto confirm_fn = [](const SafetyVerdict& v) {
        return v.tier != RiskTier::High && v.tier != RiskTier::Critical;
    };
    auto pm = feather::hal::make_process_manager();
    auto sandbox = feather::hal::make_sandbox_driver();
    return aegis_execute(command, config_.working_dir, *pm, *sandbox, confirm_fn);
}

// ─── Aegis Execute ───────────────────────────────────────────────────────────
feather::hal::ProcessOutput aegis_execute(
    std::string_view command,
    const std::filesystem::path& working_dir,
    feather::hal::IProcessManager& pm,
    feather::hal::ISandboxDriver&  sandbox,
    std::function<bool(const SafetyVerdict&)> confirm_fn)
{
    auto verdict = classify(command);

    if (verdict.tier == RiskTier::Critical) {
        feather::hal::ProcessOutput blocked;
        blocked.exit_code    = 126;
        blocked.stderr_data  = "[Aegis] BLOCKED — " + verdict.rule_name +
                               ": " + verdict.explanation;
        return blocked;
    }

    if (verdict.tier == RiskTier::High) {
        if (!confirm_fn(verdict)) {
            feather::hal::ProcessOutput denied;
            denied.exit_code   = 125;
            denied.stderr_data = "[Aegis] Execution denied by user.";
            return denied;
        }
    }

    auto sbx_kind = (verdict.tier == RiskTier::Safe || verdict.tier == RiskTier::Moderate)
                    ? feather::hal::SandboxKind::UniversalRestricted
                    : sandbox.detect_capability();

    auto wrapped = sandbox.wrap_command(command, working_dir, sbx_kind);
    return pm.execute(wrapped, working_dir, sbx_kind);
}

} // namespace feather::safety
