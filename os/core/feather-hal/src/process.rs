//! Platform-Independent Process Execution & Lifecycle Management

use crate::traits::{ProcessManager, ProcessOutput, SandboxKind};
use anyhow::Result;
use async_trait::async_trait;
use std::path::Path;
use std::time::Instant;
use tokio::process::Command;

#[derive(Debug, Default, Clone)]
pub struct StandardProcessManager;

impl StandardProcessManager {
    pub fn new() -> Self {
        Self
    }
}

#[async_trait]
impl ProcessManager for StandardProcessManager {
    async fn execute(
        &self,
        command: &str,
        working_dir: &Path,
        _sandbox: SandboxKind,
        _timeout_secs: Option<u64>,
    ) -> Result<ProcessOutput> {
        let start = Instant::now();

        let mut cmd = if cfg!(windows) {
            let mut c = Command::new("powershell.exe");
            c.arg("-NoProfile").arg("-Command").arg(command);
            c
        } else {
            let mut c = Command::new("/bin/sh");
            c.arg("-c").arg(command);
            c
        };

        cmd.current_dir(working_dir);

        let output = cmd.output().await?;
        let duration_ms = start.elapsed().as_millis() as u64;

        Ok(ProcessOutput {
            exit_code: output.status.code().unwrap_or(-1),
            stdout: String::from_utf8_lossy(&output.stdout).to_string(),
            stderr: String::from_utf8_lossy(&output.stderr).to_string(),
            duration_ms,
        })
    }

    async fn terminate(&self, _pid: u32) -> Result<()> {
        Ok(())
    }
}
