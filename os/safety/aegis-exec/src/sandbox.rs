use std::path::Path;
use std::process::Command;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SandboxTechnology {
    Bubblewrap,        // Linux
    UserNamespace,     // Linux
    MacSeatbelt,       // macOS sandbox-exec
    WindowsJob,        // Windows Job Object / AppContainer
    RestrictedProcess, // Universal fallback
}

#[derive(Debug, Clone)]
pub struct SandboxCapability {
    pub platform: String,
    pub has_bubblewrap: bool,
    pub has_user_namespaces: bool,
    pub active_tech: SandboxTechnology,
}

impl Default for SandboxCapability {
    fn default() -> Self {
        Self::detect()
    }
}

impl SandboxCapability {
    pub fn detect() -> Self {
        let os = std::env::consts::OS;

        if os == "linux" {
            let has_bwrap = Command::new("bwrap")
                .arg("--version")
                .output()
                .map(|o| o.status.success())
                .unwrap_or(false);

            let has_unshare = Command::new("unshare")
                .args(["--user", "--map-root-user", "true"])
                .output()
                .map(|o| o.status.success())
                .unwrap_or(false);

            let active_tech = if has_bwrap {
                SandboxTechnology::Bubblewrap
            } else if has_unshare {
                SandboxTechnology::UserNamespace
            } else {
                SandboxTechnology::RestrictedProcess
            };

            Self {
                platform: "linux".to_string(),
                has_bubblewrap: has_bwrap,
                has_user_namespaces: has_unshare,
                active_tech,
            }
        } else if os == "macos" {
            let has_sandbox = Command::new("sandbox-exec")
                .arg("-n")
                .arg("no-network")
                .arg("true")
                .output()
                .map(|o| o.status.success())
                .unwrap_or(false);

            let active_tech = if has_sandbox {
                SandboxTechnology::MacSeatbelt
            } else {
                SandboxTechnology::RestrictedProcess
            };

            Self {
                platform: "macos".to_string(),
                has_bubblewrap: false,
                has_user_namespaces: false,
                active_tech,
            }
        } else if os == "windows" {
            Self {
                platform: "windows".to_string(),
                has_bubblewrap: false,
                has_user_namespaces: false,
                active_tech: SandboxTechnology::WindowsJob,
            }
        } else {
            Self {
                platform: os.to_string(),
                has_bubblewrap: false,
                has_user_namespaces: false,
                active_tech: SandboxTechnology::RestrictedProcess,
            }
        }
    }

    pub fn wrap_command(&self, original_cmd: &str, working_dir: &Path) -> String {
        match self.active_tech {
            SandboxTechnology::Bubblewrap => {
                let dir_str = working_dir.to_string_lossy();
                format!(
                    "bwrap --ro-bind / / --dev /dev --proc /proc --bind {} {} --unshare-all bash -c {:?}",
                    dir_str, dir_str, original_cmd
                )
            }
            SandboxTechnology::UserNamespace => {
                format!("unshare --user --map-root-user bash -c {:?}", original_cmd)
            }
            SandboxTechnology::MacSeatbelt => {
                format!("sandbox-exec -p '(version 1)(allow default)' sh -c {:?}", original_cmd)
            }
            SandboxTechnology::WindowsJob | SandboxTechnology::RestrictedProcess => {
                original_cmd.to_string()
            }
        }
    }
}
