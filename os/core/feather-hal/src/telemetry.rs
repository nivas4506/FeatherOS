//! Platform-Independent System Telemetry

use crate::traits::{HardwareTelemetry, SystemTelemetryProvider};
use anyhow::Result;
use async_trait::async_trait;

#[derive(Debug, Default, Clone)]
pub struct StandardTelemetryProvider;

impl StandardTelemetryProvider {
    pub fn new() -> Self {
        Self
    }
}

#[async_trait]
impl SystemTelemetryProvider for StandardTelemetryProvider {
    async fn sample_telemetry(&self) -> Result<HardwareTelemetry> {
        Ok(HardwareTelemetry {
            arch: std::env::consts::ARCH.to_string(),
            kernel_version: std::env::consts::OS.to_string(),
            cpu_cores: std::thread::available_parallelism()
                .map(|p| p.get())
                .unwrap_or(1),
            cpu_usage_pct: 12.5,
            mem_total_mb: 8192,
            mem_available_mb: 4096,
            disk_available_gb: 128,
        })
    }

    fn kernel_version(&self) -> String {
        std::env::consts::OS.to_string()
    }

    fn architecture(&self) -> String {
        std::env::consts::ARCH.to_string()
    }
}
