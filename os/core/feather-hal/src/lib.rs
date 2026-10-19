//! FeatherOS Hardware & Kernel Abstraction Layer (HAL)
//!
//! Provides platform-independent contracts and backends for processes,
//! filesystems, sandboxing, and hardware telemetry.

pub mod fs;
pub mod process;
pub mod sandbox;
pub mod telemetry;
pub mod traits;

pub use fs::StandardVfs;
pub use process::StandardProcessManager;
pub use sandbox::PlatformSandboxDriver;
pub use telemetry::StandardTelemetryProvider;
pub use traits::{
    HardwareTelemetry, ProcessManager, ProcessOutput, SandboxDriver, SandboxKind,
    SystemTelemetryProvider, VirtualFileSystem,
};
