use crate::Daemon;
use anyhow::Result;
use feather_watch::{TelemetrySampler, WorkspaceWatcher};
use serde_json::json;
use std::{collections::HashSet, sync::{atomic::Ordering, Arc}, time::Duration};
use synapsed::FileSystemIndexer;
use tokio::task::JoinHandle;

impl Daemon {
    pub async fn start_perception(self: &Arc<Self>) -> Result<JoinHandle<()>> {
        let (mut watcher, mut changes) = WorkspaceWatcher::new()?;
        watcher.watch(&self.workspace)?;
        *self.watcher.lock().await = Some(watcher);
        let weak = Arc::downgrade(self);
        Ok(tokio::spawn(async move {
            let mut debounce = tokio::time::interval(Duration::from_millis(500));
            let mut sampling = tokio::time::interval(Duration::from_secs(5));
            let mut reconcile = tokio::time::interval(Duration::from_secs(60));
            let mut paths = HashSet::new();
            let mut sampler = TelemetrySampler::default();
            let mut previous_alerts = Vec::new();
            loop {
                let Some(daemon) = weak.upgrade() else { break; };
                tokio::select! {
                    path = changes.recv() => {
                        let Some(path) = path else { break; };
                        if paths.len() < 2048 { paths.insert(path); }
                        else if let Some(watcher) = daemon.watcher.lock().await.as_ref() {
                            watcher.rescan.store(true, Ordering::Relaxed);
                        }
                    }
                    _ = debounce.tick() => {
                        let (roots, rescan) = {
                            let watcher = daemon.watcher.lock().await;
                            let watcher = watcher.as_ref().unwrap();
                            (watcher.roots(), watcher.rescan.swap(false, Ordering::Relaxed))
                        };
                        let mut journal = daemon.journal.lock().await;
                        let mut errors = Vec::new();
                        let mut changed = false;
                        if rescan {
                            for root in &roots {
                                if root.starts_with(&daemon.data_dir) { continue; }
                                match FileSystemIndexer::index_directory_excluding(&mut journal, root, 10_000, std::slice::from_ref(&daemon.data_dir)) {
                                    Ok(_) => changed = true,
                                    Err(e) => errors.push(e.to_string()),
                                }
                            }
                        }
                        for path in paths.drain() {
                            if path.starts_with(&daemon.data_dir) || !roots.iter().any(|r| FileSystemIndexer::eligible(r, &path)) { continue; }
                            let result = if !path.exists() {
                                journal.remove_files_under(&path);
                                Ok(0)
                            } else if path.is_dir() {
                                FileSystemIndexer::index_directory_excluding(&mut journal, &path, 10_000, std::slice::from_ref(&daemon.data_dir))
                            } else {
                                FileSystemIndexer::index_file(&mut journal, &path).map(usize::from)
                            };
                            match result { Ok(_) => changed = true, Err(e) => errors.push(e.to_string()) }
                        }
                        if changed {
                            if let Err(e) = journal.persist() { errors.push(e.to_string()); }
                        }
                        let count = journal.total_entries();
                        drop(journal);
                        if changed { daemon.emit("perception.index_updated", json!({"entries":count})).await; }
                        for error in errors { daemon.emit("perception.error", json!({"message":error})).await; }
                    }
                    _ = sampling.tick() => {
                        let sample = sampler.sample(&daemon.workspace);
                        if sample.alerts != previous_alerts && !sample.alerts.is_empty() {
                            daemon.emit("perception.alert", json!({"alerts":sample.alerts})).await;
                        }
                        previous_alerts = sample.alerts.clone();
                        daemon.emit("telemetry.updated", json!(sample)).await;
                        *daemon.telemetry.lock().await = sample;
                    }
                    _ = reconcile.tick() => {
                        if let Some(watcher) = daemon.watcher.lock().await.as_ref() { watcher.rescan.store(true, Ordering::Relaxed); }
                    }
                }
            }
        }))
    }
}
