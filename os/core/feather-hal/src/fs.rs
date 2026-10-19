//! Platform-Independent Virtual File System implementation

use crate::traits::VirtualFileSystem;
use anyhow::{Context, Result};
use async_trait::async_trait;
use std::path::{Path, PathBuf};
use tokio::fs;

#[derive(Debug, Default, Clone)]
pub struct StandardVfs;

impl StandardVfs {
    pub fn new() -> Self {
        Self
    }
}

#[async_trait]
impl VirtualFileSystem for StandardVfs {
    async fn read_to_string(&self, path: &Path) -> Result<String> {
        fs::read_to_string(path)
            .await
            .with_context(|| format!("Failed to read file: {}", path.display()))
    }

    async fn write_atomic(&self, path: &Path, content: &[u8]) -> Result<()> {
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent).await?;
        }
        let temp_path = path.with_extension("feather_tmp");
        fs::write(&temp_path, content).await?;
        fs::rename(&temp_path, path).await?;
        Ok(())
    }

    async fn remove_file(&self, path: &Path) -> Result<()> {
        fs::remove_file(path).await?;
        Ok(())
    }

    async fn list_directory(&self, path: &Path) -> Result<Vec<PathBuf>> {
        let mut entries = Vec::new();
        let mut read_dir = fs::read_dir(path).await?;
        while let Some(entry) = read_dir.next_entry().await? {
            entries.push(entry.path());
        }
        Ok(entries)
    }

    fn exists(&self, path: &Path) -> bool {
        path.exists()
    }
}
