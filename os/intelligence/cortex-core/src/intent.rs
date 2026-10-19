use regex::Regex;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum IntentType {
    BuiltinShell(String),      // Commands starting with ':' (e.g. :help, :status, :tools)
    DirectCommand(String),     // Exact shell command (ls, cd, git, etc.)
    NaturalLanguage(String),   // User intent to be planned by LLM
}

pub struct IntentClassifier {
    common_posix_commands: Vec<&'static str>,
    question_indicator: Regex,
}

impl Default for IntentClassifier {
    fn default() -> Self {
        Self::new()
    }
}

impl IntentClassifier {
    pub fn new() -> Self {
        let common_posix_commands = vec![
            "ls", "cd", "pwd", "cp", "mv", "rm", "mkdir", "rmdir", "touch", "cat",
            "less", "more", "head", "tail", "grep", "find", "sed", "awk", "tar",
            "gzip", "zip", "unzip", "curl", "wget", "git", "cargo", "rustc", "python",
            "python3", "node", "npm", "docker", "make", "clear", "echo", "export",
            "source", "env", "ps", "top", "htop", "kill", "pkill", "chmod", "chown",
            "df", "du", "uname", "whoami", "ssh", "scp", "which", "whereis",
        ];

        let question_indicator = Regex::new(
            r"^(how|what|why|where|who|when|can you|please|find|search|show me|list all|clean|check|monitor|analyze)\b"
        ).unwrap();

        Self {
            common_posix_commands,
            question_indicator,
        }
    }

    pub fn classify(&self, input: &str) -> IntentType {
        let trimmed = input.trim();
        if trimmed.is_empty() {
            return IntentType::DirectCommand(String::new());
        }

        // 1. Built-in shell commands start with ':'
        if let Some(rest) = trimmed.strip_prefix(':') {
            return IntentType::BuiltinShell(rest.trim().to_string());
        }

        // 2. Explicit NL prefix '?'
        if let Some(rest) = trimmed.strip_prefix('?') {
            return IntentType::NaturalLanguage(rest.trim().to_string());
        }

        // 3. Check for obvious natural language indicators
        let lower = trimmed.to_lowercase();
        if self.question_indicator.is_match(&lower) && trimmed.split_whitespace().count() > 3 {
            return IntentType::NaturalLanguage(trimmed.to_string());
        }

        // 4. Check if first word is a standard POSIX command without complex English phrasing
        let first_token = trimmed.split_whitespace().next().unwrap_or("");
        if self.common_posix_commands.contains(&first_token) {
            // If it has conversational phrases, it's likely NL
            if lower.contains(" please ") || lower.contains(" then ") || lower.contains(" and also ") {
                return IntentType::NaturalLanguage(trimmed.to_string());
            }
            return IntentType::DirectCommand(trimmed.to_string());
        }

        // 5. If it contains pipes, redirections, or paths, treat as direct command
        if trimmed.contains('|') || trimmed.contains('>') || trimmed.contains('<') || trimmed.starts_with("./") || trimmed.starts_with('/') {
            return IntentType::DirectCommand(trimmed.to_string());
        }

        // Default: If multiple words and not known command, treat as NL intent
        if trimmed.split_whitespace().count() > 1 {
            IntentType::NaturalLanguage(trimmed.to_string())
        } else {
            IntentType::DirectCommand(trimmed.to_string())
        }
    }
}
