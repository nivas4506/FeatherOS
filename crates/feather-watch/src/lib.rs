use anyhow::Result;
use notify::{EventKind, RecommendedWatcher, RecursiveMode, Watcher};
use serde::Serialize;
use std::{collections::HashSet, path::{Path, PathBuf}, sync::{atomic::{AtomicBool, Ordering}, Arc}};
use tokio::sync::mpsc;

pub struct WorkspaceWatcher {
    watcher: RecommendedWatcher,
    roots: HashSet<PathBuf>,
    pub rescan: Arc<AtomicBool>,
}

impl WorkspaceWatcher {
    pub fn new() -> Result<(Self, mpsc::Receiver<PathBuf>)> {
        let (tx, rx) = mpsc::channel(2048);
        let rescan = Arc::new(AtomicBool::new(false));
        let flag = rescan.clone();
        let watcher = notify::recommended_watcher(move |event: notify::Result<notify::Event>| {
            match event {
                Ok(event) if matches!(event.kind, EventKind::Create(_) | EventKind::Modify(_) | EventKind::Remove(_)) => {
                    for path in event.paths {
                        if tx.try_send(path).is_err() { flag.store(true, Ordering::Relaxed); }
                    }
                }
                Err(_) => flag.store(true, Ordering::Relaxed),
                _ => {}
            }
        })?;
        Ok((Self { watcher, roots:HashSet::new(), rescan }, rx))
    }

    pub fn watch(&mut self, root: &Path) -> Result<bool> {
        let root = root.canonicalize()?;
        if self.roots.iter().any(|r| root.starts_with(r)) { return Ok(false); }
        anyhow::ensure!(self.roots.len() < 32, "At most 32 workspace watches are supported");
        self.watcher.watch(&root, RecursiveMode::Recursive)?;
        self.roots.insert(root);
        self.rescan.store(true, Ordering::Relaxed);
        Ok(true)
    }

    pub fn roots(&self) -> Vec<PathBuf> { self.roots.iter().cloned().collect() }
}

#[derive(Debug, Default, Clone, Serialize)]
pub struct Telemetry {
    pub cpu_percent: f32,
    pub memory_available_mb: u64,
    pub memory_total_mb: u64,
    pub disk_available_gb: u64,
    pub alerts: Vec<String>,
}

pub struct TelemetrySampler(sysinfo::System);

impl Default for TelemetrySampler {
    fn default() -> Self {
        let mut system = sysinfo::System::new();
        system.refresh_cpu_usage();
        Self(system)
    }
}

impl TelemetrySampler {
    pub fn sample(&mut self, workspace: &Path) -> Telemetry {
        self.0.refresh_cpu_usage();
        self.0.refresh_memory();
        let disks = sysinfo::Disks::new_with_refreshed_list();
        let available = disks.iter().filter(|d| workspace.starts_with(d.mount_point()))
            .max_by_key(|d| d.mount_point().components().count()).map(|d| d.available_space());
        let mut sample = Telemetry {
            cpu_percent:self.0.global_cpu_usage(),
            memory_available_mb:self.0.available_memory() / (1024 * 1024),
            memory_total_mb:self.0.total_memory() / (1024 * 1024),
            disk_available_gb:available.unwrap_or(0) / (1024 * 1024 * 1024),
            alerts:vec![],
        };
        if sample.cpu_percent > 95.0 { sample.alerts.push("CPU usage exceeds 95%".into()); }
        if sample.memory_total_mb > 0 && sample.memory_available_mb < sample.memory_total_mb / 10 {
            sample.alerts.push("Less than 10% memory available".into());
        }
        if available.is_some_and(|bytes| bytes < 1024 * 1024 * 1024) { sample.alerts.push("Less than 1 GiB disk space available".into()); }
        sample
    }
}

#[derive(Debug, Serialize)]
pub struct Diagnostic {
    pub cause: &'static str,
    pub suggestion: &'static str,
}

pub fn diagnose(stderr: &str) -> Option<Diagnostic> {
    let text = stderr.to_lowercase();
    let (cause, suggestion) = if text.contains("command not found") || text.contains("not recognized") || text.contains("no such file or directory") {
        ("Missing command or file", "Check the command path, workspace, and required dependencies.")
    } else if text.contains("address already in use") || text.contains("eaddrinuse") {
        ("Port collision", "Inspect listening ports and choose a free port or stop the owning process after review.")
    } else if text.contains("permission denied") || text.contains("access is denied") {
        ("Insufficient file permissions", "Inspect file ownership and permissions before changing access.")
    } else if text.contains("conflict") && (text.contains("merge") || text.contains("git")) {
        ("Git merge conflict", "Inspect git status and resolve the conflicting file hunks.")
    } else if text.contains("connection refused") || text.contains("timed out") {
        ("Unavailable service or network", "Check the service status, endpoint, and listening ports.")
    } else { return None; };
    Some(Diagnostic { cause, suggestion })
}
