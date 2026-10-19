use regex::Regex;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
pub enum RiskLevel {
    Safe = 0,       // Read-only operations (ls, cat, dir, git status, etc.)
    Moderate = 1,   // Non-destructive writes (touch, mkdir, git commit, etc.)
    High = 2,       // Mutative actions (rm, del, kill, Stop-Process, package installs)
    Critical = 3,   // System-destructive or irreversible (rm -rf /, format, del /s /q C:\)
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SafetyAssessment {
    pub risk_level: RiskLevel,
    pub warnings: Vec<String>,
    pub requires_confirmation: bool,
    pub allowed: bool,
}

pub struct SafetyEngine {
    critical_patterns: Vec<(Regex, &'static str)>,
    high_patterns: Vec<(Regex, &'static str)>,
    moderate_patterns: Vec<(Regex, &'static str)>,
}

impl Default for SafetyEngine {
    fn default() -> Self {
        Self::new()
    }
}

impl SafetyEngine {
    pub fn new() -> Self {
        let critical_patterns = vec![
            // Unix critical
            (Regex::new(r"rm\s+(-[a-zA-Z]*r[a-zA-Z]*\s+|-[a-zA-Z]*f[a-zA-Z]*\s+)*(/\s*$|/\*|~|\$HOME)").unwrap(), "Recursive deletion of root or home directory"),
            (Regex::new(r">\s*/dev/(sd[a-z]|nvme[0-9]|hd[a-z])").unwrap(), "Direct block device write / destruction"),
            (Regex::new(r"mkfs(\.[a-z0-9]+)?\s+").unwrap(), "Filesystem formatting"),
            (Regex::new(r":\(\)\s*\{\s*:\s*\|\s*:\s*&\s*\}\s*;\s*:").unwrap(), "Fork bomb detected"),
            (Regex::new(r"chmod\s+(-[a-zA-Z]*R[a-zA-Z]*\s+)?777\s+/").unwrap(), "Global world-writable permissions on root"),
            (Regex::new(r"curl.*\|\s*(sudo\s+)?(bash|sh|zsh)").unwrap(), "Piping remote internet script directly to shell"),
            // Windows critical
            (Regex::new(r"(?i)\bdel\s+/[sS]\s+/[qQ]\s+[c-zC-Z]:\\").unwrap(), "Silent recursive deletion of system drive"),
            (Regex::new(r"(?i)\brd\s+/[sS]\s+/[qQ]\s+[c-zC-Z]:\\").unwrap(), "Recursive removal of root system directory"),
            (Regex::new(r"(?i)\bformat\s+[c-zC-Z]:").unwrap(), "Disk volume format command"),
            (Regex::new(r"(?i)\bFormat-Volume\b").unwrap(), "PowerShell format volume command"),
            (Regex::new(r"(?i)\bRemove-Item\s+.*-[rR]ecurse.*[c-zC-Z]:\\").unwrap(), "PowerShell recursive root deletion"),
        ];

        let high_patterns = vec![
            // Unix high
            (Regex::new(r"\brm\s+").unwrap(), "File deletion command"),
            (Regex::new(r"\b(kill|pkill|killall)\s+").unwrap(), "Process termination"),
            (Regex::new(r"\b(apt|apt-get|dpkg|pacman|dnf|yum|brew)\s+(install|remove|purge)").unwrap(), "Package management modification"),
            (Regex::new(r"\bsudo\s+").unwrap(), "Elevated administrative privileges requested"),
            (Regex::new(r"\bchmod\s+").unwrap(), "File permission modification"),
            (Regex::new(r"\bchown\s+").unwrap(), "File ownership modification"),
            (Regex::new(r">\s*/etc/").unwrap(), "Writing into system configuration directory"),
            // Windows high
            (Regex::new(r"(?i)\b(del|erase)\s+").unwrap(), "Windows file deletion"),
            (Regex::new(r"(?i)\b(Stop-Process|taskkill)\b").unwrap(), "Process termination"),
            (Regex::new(r"(?i)\b(winget|choco)\s+(install|uninstall)").unwrap(), "Windows package modification"),
            (Regex::new(r"(?i)\b(net\s+user|net\s+localgroup)\b").unwrap(), "User account/group modification"),
        ];

        let moderate_patterns = vec![
            // Unix moderate
            (Regex::new(r"\b(touch|mkdir|mv|cp)\s+").unwrap(), "Filesystem creation or move"),
            (Regex::new(r"\bgit\s+(commit|push|checkout|reset)").unwrap(), "Git repository state modification"),
            (Regex::new(r"\b(sed|awk)\s+-i").unwrap(), "In-place file modification"),
            // Windows moderate
            (Regex::new(r"(?i)\b(New-Item|Copy-Item|Move-Item|Rename-Item)\b").unwrap(), "PowerShell filesystem modification"),
            (Regex::new(r"(?i)\b(md|mkdir|move|copy|ren)\s+").unwrap(), "Command Prompt filesystem modification"),
        ];

        Self {
            critical_patterns,
            high_patterns,
            moderate_patterns,
        }
    }

    pub fn assess(&self, command: &str) -> SafetyAssessment {
        let trimmed = command.trim();
        let mut warnings = Vec::new();
        let mut current_risk = RiskLevel::Safe;

        // Check critical patterns
        for (pattern, reason) in &self.critical_patterns {
            if pattern.is_match(trimmed) {
                warnings.push(format!("CRITICAL: {}", reason));
                current_risk = RiskLevel::Critical;
            }
        }

        // Check high patterns if not already critical
        if current_risk < RiskLevel::Critical {
            for (pattern, reason) in &self.high_patterns {
                if pattern.is_match(trimmed) {
                    warnings.push(format!("HIGH RISK: {}", reason));
                    if current_risk < RiskLevel::High {
                        current_risk = RiskLevel::High;
                    }
                }
            }
        }

        // Check moderate patterns if still safe
        if current_risk < RiskLevel::High {
            for (pattern, reason) in &self.moderate_patterns {
                if pattern.is_match(trimmed) {
                    warnings.push(format!("MODERATE: {}", reason));
                    if current_risk < RiskLevel::Moderate {
                        current_risk = RiskLevel::Moderate;
                    }
                }
            }
        }

        let (requires_confirmation, allowed) = match current_risk {
            RiskLevel::Safe => (false, true),
            RiskLevel::Moderate => (false, true),
            RiskLevel::High => (true, true),
            RiskLevel::Critical => (true, false),
        };

        SafetyAssessment {
            risk_level: current_risk,
            warnings,
            requires_confirmation,
            allowed,
        }
    }
}
