use anyhow::{Context, Result};
use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use std::process::Stdio;
use std::time::{Duration, Instant};
use tokio::process::Command;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ExecutionResult {
    pub command: String,
    pub working_dir: PathBuf,
    pub exit_code: Option<i32>,
    pub stdout: String,
    pub stderr: String,
    pub duration_ms: u64,
    pub executed_at: DateTime<Utc>,
    pub timed_out: bool,
}

pub struct ExecutionConfig {
    pub timeout: Duration,
    pub working_dir: PathBuf,
    pub inherit_env: bool,
    pub env_vars: Vec<(String, String)>,
}

impl Default for ExecutionConfig {
    fn default() -> Self {
        Self {
            timeout: Duration::from_secs(60),
            working_dir: std::env::current_dir().unwrap_or_else(|_| PathBuf::from(".")),
            inherit_env: true,
            env_vars: Vec::new(),
        }
    }
}

pub struct SafeExecutor {
    config: ExecutionConfig,
}

impl SafeExecutor {
    pub fn new(config: ExecutionConfig) -> Self {
        Self { config }
    }

    fn create_command(&self, command_str: &str) -> Command {
        if cfg!(target_os = "windows") {
            let mut cmd = Command::new("powershell");
            cmd.args(["-NoProfile", "-NonInteractive", "-Command", command_str]);
            cmd
        } else {
            let shell = std::env::var("SHELL").unwrap_or_else(|_| "/bin/sh".to_string());
            let mut cmd = Command::new(shell);
            cmd.arg("-c").arg(command_str);
            cmd
        }
    }

    pub async fn execute(&self, command_str: &str) -> Result<ExecutionResult> {
        anyhow::ensure!(crate::SafetyEngine::new().assess(command_str).allowed, "Critical command blocked by Aegis");
        let start_time = Instant::now();
        let executed_at = Utc::now();

        let mut cmd = self.create_command(command_str);
        cmd.current_dir(&self.config.working_dir)
            .kill_on_drop(true)
            .stdin(Stdio::null())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped());

        if !self.config.inherit_env {
            cmd.env_clear();
            if cfg!(target_os = "windows") {
                cmd.env("PATH", "C:\\Windows\\System32;C:\\Windows;C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\");
            } else {
                cmd.env("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin");
            }
        }

        for (k, v) in &self.config.env_vars {
            cmd.env(k, v);
        }

        #[cfg(unix)]
        cmd.process_group(0);
        let mut child = cmd.spawn().with_context(|| format!("Failed to spawn command: {}", command_str))?;
        #[cfg(unix)]
        let _group = ProcessGroup(child.id().unwrap() as i32);
        let stdout = child.stdout.take().unwrap();
        let stderr = child.stderr.take().unwrap();
        let timeout_fut = tokio::time::timeout(self.config.timeout, async {
            tokio::try_join!(child.wait(), read_bounded(stdout), read_bounded(stderr))
        });

        match timeout_fut.await {
            Ok(Ok((status, stdout, stderr))) => {
                let duration_ms = start_time.elapsed().as_millis() as u64;

                Ok(ExecutionResult {
                    command: command_str.to_string(),
                    working_dir: self.config.working_dir.clone(),
                    exit_code: status.code(),
                    stdout,
                    stderr,
                    duration_ms,
                    executed_at,
                    timed_out: false,
                })
            }
            Ok(Err(e)) => Err(anyhow::anyhow!("Execution error: {}", e)),
            Err(_) => {
                #[cfg(windows)]
                if let Some(pid) = child.id() {
                    let _ = Command::new("taskkill").args(["/PID", &pid.to_string(), "/T", "/F"]).output().await;
                }
                let _ = child.kill().await;
                let duration_ms = start_time.elapsed().as_millis() as u64;
                Ok(ExecutionResult {
                    command: command_str.to_string(),
                    working_dir: self.config.working_dir.clone(),
                    exit_code: None,
                    stdout: String::new(),
                    stderr: "Process timed out and was terminated.".to_string(),
                    duration_ms,
                    executed_at,
                    timed_out: true,
                })
            }
        }
    }
}

async fn read_bounded(mut reader: impl tokio::io::AsyncRead + Unpin) -> std::io::Result<String> {
    use tokio::io::AsyncReadExt;
    const LIMIT: usize = 64 * 1024;
    let mut bytes = Vec::new();
    let mut buffer = [0; 8192];
    let mut truncated = false;
    loop {
        let count = reader.read(&mut buffer).await?;
        if count == 0 { break; }
        let keep = count.min(LIMIT.saturating_sub(bytes.len()));
        bytes.extend_from_slice(&buffer[..keep]);
        truncated |= keep < count;
    }
    let mut output = String::from_utf8_lossy(&bytes).into_owned();
    if truncated { output.push_str("\n[output truncated at 64 KiB]\n"); }
    Ok(output)
}

#[cfg(unix)]
struct ProcessGroup(i32);

#[cfg(unix)]
impl Drop for ProcessGroup {
    fn drop(&mut self) {
        // This process group was created exclusively for this command.
        unsafe { libc::kill(-self.0, libc::SIGKILL); }
    }
}
