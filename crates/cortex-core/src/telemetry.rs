use serde::{Deserialize, Serialize};
use std::fs;
use std::path::PathBuf;
use std::process::Command;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SystemContext {
    pub username: String,
    pub hostname: String,
    pub os_name: String,
    pub kernel: String,
    pub current_dir: PathBuf,
    pub git_branch: Option<String>,
    pub git_dirty: bool,
    pub memory_available_mb: Option<u64>,
    pub disk_available_gb: Option<u64>,
}

impl SystemContext {
    pub fn capture() -> Self {
        Self::capture_in(std::env::current_dir().unwrap_or_else(|_| PathBuf::from(".")))
    }

    /// Capture context without changing the process-wide directory in a multi-client daemon.
    pub fn capture_in(current_dir: PathBuf) -> Self {
        let username = std::env::var("USER")
            .or_else(|_| std::env::var("USERNAME"))
            .unwrap_or_else(|_| "unknown".to_string());

        let hostname = Self::get_hostname();
        let os_name = Self::get_os_name();
        let kernel = Self::get_kernel();
        let (git_branch, git_dirty) = Self::get_git_info(&current_dir);
        let memory_available_mb = Self::get_memory_available();
        let disk_available_gb = Self::get_disk_available(&current_dir);

        Self {
            username,
            hostname,
            os_name,
            kernel,
            current_dir,
            git_branch,
            git_dirty,
            memory_available_mb,
            disk_available_gb,
        }
    }

    fn get_hostname() -> String {
        if let Ok(host) = std::env::var("COMPUTERNAME") {
            return host;
        }
        if let Ok(content) = fs::read_to_string("/etc/hostname") {
            let trimmed = content.trim().to_string();
            if !trimmed.is_empty() {
                return trimmed;
            }
        }
        Command::new("hostname")
            .output()
            .map(|o| String::from_utf8_lossy(&o.stdout).trim().to_string())
            .unwrap_or_else(|_| "localhost".to_string())
    }

    fn get_os_name() -> String {
        let target_os = std::env::consts::OS;
        if target_os == "linux" {
            if let Ok(content) = fs::read_to_string("/etc/os-release") {
                for line in content.lines() {
                    if let Some(stripped) = line.strip_prefix("PRETTY_NAME=") {
                        return stripped.trim_matches('"').to_string();
                    }
                }
            }
            "Linux".to_string()
        } else if target_os == "macos" {
            let out = Command::new("sw_vers")
                .arg("-productVersion")
                .output()
                .map(|o| String::from_utf8_lossy(&o.stdout).trim().to_string())
                .unwrap_or_default();
            if !out.is_empty() {
                format!("macOS {}", out)
            } else {
                "macOS".to_string()
            }
        } else if target_os == "windows" {
            std::env::var("OS").unwrap_or_else(|_| "Windows".to_string())
        } else {
            target_os.to_string()
        }
    }

    fn get_kernel() -> String {
        if cfg!(unix) {
            Command::new("uname")
                .arg("-r")
                .output()
                .map(|o| String::from_utf8_lossy(&o.stdout).trim().to_string())
                .unwrap_or_else(|_| "unknown".to_string())
        } else {
            std::env::var("PROCESSOR_ARCHITECTURE").unwrap_or_else(|_| std::env::consts::ARCH.to_string())
        }
    }

    fn get_git_info(path: &PathBuf) -> (Option<String>, bool) {
        let branch_out = Command::new("git")
            .args(["rev-parse", "--abbrev-ref", "HEAD"])
            .current_dir(path)
            .output();

        let branch = match branch_out {
            Ok(o) if o.status.success() => {
                let name = String::from_utf8_lossy(&o.stdout).trim().to_string();
                if name.is_empty() { None } else { Some(name) }
            }
            _ => None,
        };

        let dirty = if branch.is_some() {
            Command::new("git")
                .args(["status", "--porcelain"])
                .current_dir(path)
                .output()
                .map(|o| !o.stdout.is_empty())
                .unwrap_or(false)
        } else {
            false
        };

        (branch, dirty)
    }

    fn get_memory_available() -> Option<u64> {
        // Linux /proc/meminfo
        if let Ok(meminfo) = fs::read_to_string("/proc/meminfo") {
            for line in meminfo.lines() {
                if line.starts_with("MemAvailable:") {
                    let parts: Vec<&str> = line.split_whitespace().collect();
                    if parts.len() >= 2 {
                        if let Ok(kb) = parts[1].parse::<u64>() {
                            return Some(kb / 1024);
                        }
                    }
                }
            }
        }

        // macOS sysctl hw.memsize
        if cfg!(target_os = "macos") {
            if let Ok(out) = Command::new("sysctl").args(["-n", "hw.memsize"]).output() {
                if let Ok(bytes) = String::from_utf8_lossy(&out.stdout).trim().parse::<u64>() {
                    return Some(bytes / (1024 * 1024));
                }
            }
        }

        None
    }

    fn get_disk_available(path: &PathBuf) -> Option<u64> {
        if cfg!(unix) {
            let out = Command::new("df")
                .args(["-k", path.to_string_lossy().as_ref()])
                .output()
                .ok()?;

            let s = String::from_utf8_lossy(&out.stdout);
            let lines: Vec<&str> = s.lines().collect();
            if lines.len() >= 2 {
                let cols: Vec<&str> = lines[1].split_whitespace().collect();
                if cols.len() >= 4 {
                    if let Ok(kb) = cols[3].parse::<u64>() {
                        return Some(kb / (1024 * 1024));
                    }
                }
            }
        }
        None
    }
}
