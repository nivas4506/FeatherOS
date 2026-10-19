//! Platform-Independent Sandboxing Driver

use crate::traits::{SandboxDriver, SandboxKind};
use std::path::Path;

#[derive(Debug, Default, Clone)]
pub struct PlatformSandboxDriver;

impl PlatformSandboxDriver {
    pub fn new() -> Self {
        Self
    }
}

impl SandboxDriver for PlatformSandboxDriver {
    fn wrap_command(&self, original_cmd: &str, working_dir: &Path, kind: SandboxKind) -> String {
        match kind {
            SandboxKind::Bubblewrap => {
                let dir_str = working_dir.to_string_lossy();
                format!(
                    "bwrap --ro-bind / / --dev /dev --proc /proc --bind \"{}\" \"{}\" --tmpfs /tmp --unshare-all --share-net --die-with-parent /bin/sh -c {}",
                    dir_str,
                    dir_str,
                    shell_escape(original_cmd)
                )
            }
            SandboxKind::LinuxNamespace => {
                format!(
                    "unshare --user --map-root-user --pid --mount --fork /bin/sh -c {}",
                    shell_escape(original_cmd)
                )
            }
            SandboxKind::SeccompFilter | SandboxKind::UniversalRestricted => {
                original_cmd.to_string()
            }
        }
    }

    fn detect_capability(&self) -> SandboxKind {
        if cfg!(target_os = "linux") {
            SandboxKind::Bubblewrap
        } else {
            SandboxKind::UniversalRestricted
        }
    }
}

fn shell_escape(s: &str) -> String {
    format!("'{}'", s.replace('\'', "'\\''"))
}
