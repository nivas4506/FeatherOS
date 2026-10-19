//! Platform-Independent OS Abstraction Traits
//!
//! Provides the contract between the Linux Kernel / Base Layer and the FeatherOS OS Services.

use anyhow::Result;
use async_trait::async_trait;
use serde::{Deserialize, Serialize};
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProcessOutput {
    pub exit_code: i32,
    pub stdout: String,
    pub stderr: String,
    pub duration_ms: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HardwareTelemetry {
    pub arch: String,
    pub kernel_version: String,
    pub cpu_cores: usize,
    pub cpu_usage_pct: f32,
    pub mem_total_mb: u64,
    pub mem_available_mb: u64,
    pub disk_available_gb: u64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum SandboxKind {
    LinuxNamespace,
    Bubblewrap,
    SeccompFilter,
    UniversalRestricted,
}

#[async_trait]
pub trait ProcessManager: Send + Sync {
    async fn execute(
        &self,
        command: &str,
        working_dir: &Path,
        sandbox: SandboxKind,
        timeout_secs: Option<u64>,
    ) -> Result<ProcessOutput>;

    async fn terminate(&self, pid: u32) -> Result<()>;
}

#[async_trait]
pub trait VirtualFileSystem: Send + Sync {
    async fn read_to_string(&self, path: &Path) -> Result<String>;
    async fn write_atomic(&self, path: &Path, content: &[u8]) -> Result<()>;
    async fn remove_file(&self, path: &Path) -> Result<()>;
    async fn list_directory(&self, path: &Path) -> Result<Vec<PathBuf>>;
    fn exists(&self, path: &Path) -> bool;
}

#[async_trait]
pub trait SystemTelemetryProvider: Send + Sync {
    async fn sample_telemetry(&self) -> Result<HardwareTelemetry>;
    fn kernel_version(&self) -> String;
    fn architecture(&self) -> String;
}

pub trait SandboxDriver: Send + Sync {
    fn wrap_command(&self, original_cmd: &str, working_dir: &Path, kind: SandboxKind) -> String;
    fn detect_capability(&self) -> SandboxKind;
}
