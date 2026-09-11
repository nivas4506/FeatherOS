use crate::planner::ExecutionPlan;
use crate::telemetry::SystemContext;
use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ModelConfig {
    pub provider_type: String, // "heuristic", "ollama", "openai"
    pub endpoint: String,
    pub model_name: String,
    pub api_key: Option<String>,
}

impl Default for ModelConfig {
    fn default() -> Self {
        Self {
            provider_type: "heuristic".to_string(),
            endpoint: "http://localhost:11434".to_string(),
            model_name: "llama3.2:3b".to_string(),
            api_key: None,
        }
    }
}

#[async_trait::async_trait]
pub trait LlmProvider: Send + Sync {
    async fn generate_plan(&self, intent: &str, context: &SystemContext) -> Result<ExecutionPlan>;
}

/// Offline cross-platform semantic planner that generates plans without external APIs
pub struct HeuristicProvider;

#[async_trait::async_trait]
impl LlmProvider for HeuristicProvider {
    async fn generate_plan(&self, intent: &str, _context: &SystemContext) -> Result<ExecutionPlan> {
        let lower = intent.to_lowercase();
        let is_windows = cfg!(target_os = "windows");
        let mut steps = Vec::new();
        let summary: String;

        if lower.contains("disk") || lower.contains("space") || lower.contains("large file") || lower.contains("storage") {
            summary = "Inspect storage utilization and identify largest files in the current tree.".to_string();
            if is_windows {
                steps.push(("Check filesystem disk usage".to_string(), "Get-PSDrive -PSProvider 'FileSystem'".to_string()));
                steps.push(("Find 15 largest files".to_string(), "Get-ChildItem -Path . -Recurse -File -ErrorAction SilentlyContinue | Sort-Object Length -Descending | Select-Object -First 15 Name, Length".to_string()));
            } else {
                steps.push(("Check filesystem disk usage".to_string(), "df -h .".to_string()));
                steps.push(("Find 15 largest files/directories".to_string(), "du -ah . 2>/dev/null | sort -rh | head -n 15".to_string()));
            }
        } else if lower.contains("memory") || lower.contains("ram") || lower.contains("cpu") || lower.contains("load") {
            summary = "Analyze system resources, memory allocation, and processor load.".to_string();
            if is_windows {
                steps.push(("List top processes by memory consumption".to_string(), "Get-Process | Sort-Object WorkingSet64 -Descending | Select-Object -First 6 ProcessName, Id, WS".to_string()));
            } else {
                steps.push(("Check RAM and swap usage".to_string(), "free -h 2>/dev/null || vm_stat".to_string()));
                steps.push(("Inspect system uptime and load average".to_string(), "uptime".to_string()));
                steps.push(("List top 5 processes by memory".to_string(), "ps aux --sort=-%mem 2>/dev/null | head -n 6 || ps -eo pid,pcpu,pmem,comm -r | head -n 6".to_string()));
            }
        } else if lower.contains("git") || lower.contains("commit") || lower.contains("branch") || lower.contains("repo") {
            summary = "Inspect repository status, recent commits, and working tree diff.".to_string();
            steps.push(("Check git working tree status".to_string(), "git status -s".to_string()));
            steps.push(("Display recent commit history".to_string(), "git log --oneline -n 8".to_string()));
        } else if lower.contains("process") || lower.contains("port") || lower.contains("listen") {
            summary = "Inspect active networking sockets and running processes.".to_string();
            if is_windows {
                steps.push(("List listening ports".to_string(), "Get-NetTCPConnection -State Listen | Select-Object -First 15 LocalAddress, LocalPort, OwningProcess".to_string()));
            } else {
                steps.push(("List listening ports".to_string(), "ss -tulpn 2>/dev/null || netstat -tulpn 2>/dev/null || lsof -i -P -n".to_string()));
            }
        } else if lower.contains("clean") || lower.contains("cache") {
            summary = "Audit potential cache and temporary build artifacts.".to_string();
            if is_windows {
                steps.push(("Identify build or cache folders".to_string(), "Get-ChildItem -Path . -Directory -Recurse -Depth 3 | Where-Object { $_.Name -match '^(target|node_modules|\\.cache)$' }".to_string()));
            } else {
                steps.push(("Identify target/build or temporary directories".to_string(), "find . -maxdepth 3 -type d \\( -name 'target' -o -name 'node_modules' -o -name '.cache' \\) -exec du -sh {} + 2>/dev/null".to_string()));
            }
        } else if lower.starts_with("find ") || lower.starts_with("search ") {
            let query = intent.trim_start_matches("find ").trim_start_matches("search ");
            summary = format!("Search files matching query '{}'.", query);
            if is_windows {
                steps.push(("Search filesystem".to_string(), format!("Get-ChildItem -Path . -Recurse -Filter '*{}*' -File -ErrorAction SilentlyContinue | Select-Object -First 25 FullName", query.trim())));
            } else {
                steps.push(("Search filesystem".to_string(), format!("find . -iname '*{}*' -not -path '*/.*' | head -n 25", query.trim())));
            }
        } else {
            summary = format!("Synthesized execution plan for intent: {}", intent);
            steps.push(("Execute command via cognitive shell".to_string(), intent.to_string()));
        }

        Ok(ExecutionPlan::new(intent.to_string(), summary, steps))
    }
}

/// Local Ollama provider
pub struct OllamaProvider {
    endpoint: String,
    model: String,
    client: reqwest::Client,
}

impl OllamaProvider {
    pub fn new(endpoint: String, model: String) -> Self {
        Self {
            endpoint,
            model,
            client: reqwest::Client::new(),
        }
    }
}

#[derive(Serialize)]
struct OllamaGenerateRequest<'a> {
    model: &'a str,
    prompt: &'a str,
    stream: bool,
    format: &'a str,
}

#[derive(Deserialize)]
struct OllamaGenerateResponse {
    response: String,
}

#[async_trait::async_trait]
impl LlmProvider for OllamaProvider {
    async fn generate_plan(&self, intent: &str, context: &SystemContext) -> Result<ExecutionPlan> {
        let system_prompt = format!(
            "You are the cognitive kernel of FeatherOS (AI-Native OS).\nUser: {}\nOS: {}\nKernel: {}\nDirectory: {}\n\nGenerate a JSON plan for the user intent.\nOutput schema:\n{{\n  \"summary\": \"...\",\n  \"steps\": [\n    {{\"description\": \"...\", \"command\": \"...\"}}\n  ]\n}}",
            context.username, context.os_name, context.kernel, context.current_dir.display()
        );

        let full_prompt = format!("{}\n\nUser Request: {}", system_prompt, intent);
        let url = format!("{}/api/generate", self.endpoint.trim_end_matches('/'));

        let req = OllamaGenerateRequest {
            model: &self.model,
            prompt: &full_prompt,
            stream: false,
            format: "json",
        };

        let res = self.client.post(&url)
            .json(&req)
            .send()
            .await
            .context("Failed to connect to local Ollama daemon")?;

        let data: OllamaGenerateResponse = res.json().await.context("Failed to parse Ollama JSON response")?;

        #[derive(Deserialize)]
        struct RawPlan {
            summary: String,
            steps: Vec<RawStep>,
        }
        #[derive(Deserialize)]
        struct RawStep {
            description: String,
            command: String,
        }

        let parsed: RawPlan = serde_json::from_str(&data.response)
            .context("Failed to deserialize model plan")?;

        let raw_steps = parsed.steps.into_iter().map(|s| (s.description, s.command)).collect();
        Ok(ExecutionPlan::new(intent.to_string(), parsed.summary, raw_steps))
    }
}
