use aegis_exec::{ExecutionConfig, SafeExecutor};
use anyhow::{Context, Result};
use async_trait::async_trait;
use serde_json::{json, Value};
use std::fs;
use walkdir::WalkDir;

#[async_trait]
pub trait SystemTool: Send + Sync {
    fn name(&self) -> &'static str;
    fn description(&self) -> &'static str;
    fn parameters_schema(&self) -> Value;
    async fn execute(&self, arguments: Value) -> Result<Value>;
}

/// Tool: fs_find - Searches files in a directory tree
pub struct FsFindTool;

#[async_trait]
impl SystemTool for FsFindTool {
    fn name(&self) -> &'static str {
        "fs_find"
    }

    fn description(&self) -> &'static str {
        "Cross-platform search for files matching name pattern, extension, or max depth."
    }

    fn parameters_schema(&self) -> Value {
        json!({
            "type": "object",
            "properties": {
                "directory": { "type": "string", "description": "Starting directory path" },
                "pattern": { "type": "string", "description": "Substring to search for in filename" },
                "max_depth": { "type": "integer", "description": "Maximum directory depth" },
                "limit": { "type": "integer", "description": "Max results to return (default 50)" }
            },
            "required": ["pattern"]
        })
    }

    async fn execute(&self, args: Value) -> Result<Value> {
        let pattern = args.get("pattern").and_then(|v| v.as_str()).unwrap_or("");
        let dir = args.get("directory").and_then(|v| v.as_str()).unwrap_or(".");
        let max_depth = args.get("max_depth").and_then(|v| v.as_u64()).unwrap_or(5) as usize;
        let limit = args.get("limit").and_then(|v| v.as_u64()).unwrap_or(50) as usize;

        let pattern_lower = pattern.to_lowercase();
        let mut results = Vec::new();

        for entry in WalkDir::new(dir)
            .max_depth(max_depth)
            .into_iter()
            .filter_entry(|e| {
                let name = e.file_name().to_string_lossy();
                !name.starts_with('.') || name == "."
            })
            .filter_map(|e| e.ok())
        {
            let path_str = entry.path().to_string_lossy();
            if path_str.to_lowercase().contains(&pattern_lower) {
                let metadata = entry.metadata().ok();
                let size = metadata.as_ref().map(|m| m.len()).unwrap_or(0);
                let is_dir = metadata.as_ref().map(|m| m.is_dir()).unwrap_or(false);

                results.push(json!({
                    "path": path_str,
                    "is_directory": is_dir,
                    "size_bytes": size,
                }));

                if results.len() >= limit {
                    break;
                }
            }
        }

        Ok(json!({ "total": results.len(), "matches": results }))
    }
}

/// Tool: fs_read - Reads file contents safely
pub struct FsReadTool;

#[async_trait]
impl SystemTool for FsReadTool {
    fn name(&self) -> &'static str {
        "fs_read"
    }

    fn description(&self) -> &'static str {
        "Cross-platform safe read of file content with line limits."
    }

    fn parameters_schema(&self) -> Value {
        json!({
            "type": "object",
            "properties": {
                "path": { "type": "string", "description": "Absolute or relative file path" },
                "start_line": { "type": "integer", "description": "1-indexed starting line (default 1)" },
                "limit_lines": { "type": "integer", "description": "Number of lines to read (default 100)" }
            },
            "required": ["path"]
        })
    }

    async fn execute(&self, args: Value) -> Result<Value> {
        let path_str = args.get("path").and_then(|v| v.as_str()).context("path parameter is required")?;
        let start_line = args.get("start_line").and_then(|v| v.as_u64()).unwrap_or(1) as usize;
        let limit = args.get("limit_lines").and_then(|v| v.as_u64()).unwrap_or(100) as usize;

        let content = fs::read_to_string(path_str).with_context(|| format!("Failed to read file: {}", path_str))?;
        let lines: Vec<&str> = content.lines().collect();
        let total_lines = lines.len();

        let adjusted_start = if start_line == 0 { 0 } else { start_line - 1 };
        let slice = lines.into_iter().skip(adjusted_start).take(limit).collect::<Vec<&str>>();

        Ok(json!({
            "path": path_str,
            "total_lines": total_lines,
            "lines_read": slice.len(),
            "content": slice.join("\n"),
        }))
    }
}

/// Tool: proc_list - Lists running processes cross-platform
pub struct ProcListTool;

#[async_trait]
impl SystemTool for ProcListTool {
    fn name(&self) -> &'static str {
        "proc_list"
    }

    fn description(&self) -> &'static str {
        "Cross-platform process inspector (works across Linux, macOS, Windows)."
    }

    fn parameters_schema(&self) -> Value {
        json!({
            "type": "object",
            "properties": {
                "limit": { "type": "integer", "description": "Number of top processes to return (default 15)" },
                "sort_by": { "type": "string", "enum": ["mem", "cpu"], "description": "Sort criteria" }
            }
        })
    }

    async fn execute(&self, args: Value) -> Result<Value> {
        let limit = args.get("limit").and_then(|v| v.as_u64()).unwrap_or(15);
        let cmd_str = if cfg!(target_os = "windows") {
            format!("Get-Process | Sort-Object WorkingSet64 -Descending | Select-Object -First {} Id, ProcessName, CPU, WS", limit)
        } else if cfg!(target_os = "macos") {
            format!("ps -eo pid,pcpu,pmem,comm -r | head -n {}", limit + 1)
        } else {
            let sort_flag = match args.get("sort_by").and_then(|v| v.as_str()) {
                Some("cpu") => "-%cpu",
                _ => "-%mem",
            };
            format!("ps -eo pid,user,%cpu,%mem,comm --sort={} | head -n {}", sort_flag, limit + 1)
        };

        let executor = SafeExecutor::new(ExecutionConfig::default());
        let res = executor.execute(&cmd_str).await?;

        Ok(json!({
            "processes_raw": res.stdout,
            "exit_code": res.exit_code,
        }))
    }
}
