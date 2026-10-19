//! Durable, bounded workspace snapshots. External effects are outside the rollback boundary.
use anyhow::{bail, ensure, Context, Result};
use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::{collections::{BTreeMap, BTreeSet}, fs, io::Write, path::{Component, Path, PathBuf}};
use uuid::Uuid;
use walkdir::WalkDir;

pub const EXCLUDED_DIRS: &[&str] = &[".git", ".feather", "target", "node_modules", "__pycache__"];
pub const MAX_BYTES: u64 = 128 * 1024 * 1024;
pub const MAX_FILE_BYTES: u64 = 16 * 1024 * 1024;

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Permissions {
    readonly: bool,
    #[cfg(unix)]
    mode: u32,
}

impl Permissions {
    fn capture(metadata: &fs::Metadata) -> Self {
        Self {
            readonly:metadata.permissions().readonly(),
            #[cfg(unix)]
            mode: {
                use std::os::unix::fs::PermissionsExt;
                metadata.permissions().mode()
            },
        }
    }

    fn restore(&self, path: &Path) -> Result<()> {
        let mut permissions = fs::metadata(path)?.permissions();
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            permissions.set_mode(self.mode);
        }
        #[cfg(not(unix))]
        permissions.set_readonly(self.readonly);
        fs::set_permissions(path, permissions)?;
        Ok(())
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum Entry {
    File { digest: String, permissions: Permissions },
    Directory { permissions: Permissions },
    Symlink { target: PathBuf, directory: bool },
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Snapshot {
    pub id: String,
    pub root: PathBuf,
    pub label: String,
    pub created: DateTime<Utc>,
    pub status: String,
    pub excluded: Vec<PathBuf>,
    pub before: BTreeMap<PathBuf, Entry>,
    pub after: BTreeMap<PathBuf, Entry>,
}

impl Snapshot {
    pub fn changed_paths(&self) -> Vec<PathBuf> {
        self.before.keys().chain(self.after.keys()).cloned().collect::<BTreeSet<_>>()
            .into_iter().filter(|p| self.before.get(p) != self.after.get(p)).collect()
    }
}

#[derive(Debug, Clone)]
pub struct SnapshotStore { directory: PathBuf, excluded: Vec<PathBuf> }

impl SnapshotStore {
    pub fn new(directory: PathBuf, excluded: Vec<PathBuf>) -> Result<Self> {
        fs::create_dir_all(&directory)?;
        let directory = directory.canonicalize()?;
        Ok(Self { directory, excluded })
    }

    pub fn capture(&self, root: &Path, label: &str) -> Result<Snapshot> {
        let root = root.canonicalize()?;
        ensure!(root.is_dir(), "Snapshot root must be a directory");
        ensure!(!root.starts_with(&self.directory), "Cannot snapshot the snapshot store");
        let id = Uuid::new_v4().to_string();
        let dir = self.directory.join(&id);
        fs::create_dir(&dir)?;
        fs::create_dir(dir.join("blobs"))?;
        let mut excluded = self.excluded.clone();
        excluded.push(self.directory.clone());
        let before = match self.scan(&root, &excluded, Some(&dir.join("blobs"))) {
            Ok(entries) => entries,
            Err(e) => { let _ = fs::remove_dir_all(&dir); return Err(e); }
        };
        let snapshot = Snapshot { id, root, label:label.into(), created:Utc::now(), status:"prepared".into(), excluded, before, after:BTreeMap::new() };
        self.save(&snapshot)?;
        Ok(snapshot)
    }

    pub fn finish(&self, id: &str) -> Result<Snapshot> {
        let mut snapshot = self.load(id)?;
        ensure!(snapshot.status == "prepared", "Snapshot is not awaiting completion");
        snapshot.after = self.scan(&snapshot.root, &snapshot.excluded, None)?;
        snapshot.status = "complete".into();
        self.save(&snapshot)?;
        Ok(snapshot)
    }

    pub fn discard_unchanged(&self, snapshot: &Snapshot) -> Result<()> {
        ensure!(snapshot.status == "complete" && snapshot.changed_paths().is_empty(), "Only unchanged snapshots can be discarded");
        fs::remove_dir_all(self.directory.join(Uuid::parse_str(&snapshot.id)?.to_string()))?;
        Ok(())
    }

    pub fn load(&self, id: &str) -> Result<Snapshot> {
        let id = Uuid::parse_str(id)?.to_string();
        let snapshot: Snapshot = serde_json::from_slice(&fs::read(self.directory.join(&id).join("manifest.json"))?)?;
        ensure!(snapshot.id == id && snapshot.root.is_absolute(), "Invalid snapshot manifest");
        for path in snapshot.before.keys().chain(snapshot.after.keys()) {
            ensure!(!path.as_os_str().is_empty() && path.components().all(|c| matches!(c, Component::Normal(_))), "Unsafe path in snapshot manifest");
        }
        Ok(snapshot)
    }

    pub fn list(&self, root: &Path) -> Result<Vec<Snapshot>> {
        let root = root.canonicalize()?;
        let mut snapshots = Vec::new();
        for entry in fs::read_dir(&self.directory)? {
            let entry = entry?;
            let id = entry.file_name().to_string_lossy().into_owned();
            if Uuid::parse_str(&id).is_err() { continue; }
            let snapshot = self.load(&id)?;
            if snapshot.root == root { snapshots.push(snapshot); }
        }
        snapshots.sort_by_key(|s| std::cmp::Reverse(s.created));
        Ok(snapshots)
    }

    pub fn rollback(&self, id: &str) -> Result<Snapshot> {
        let mut snapshot = self.load(id)?;
        ensure!(snapshot.status == "complete" || snapshot.status == "restoring", "Snapshot cannot be rolled back in status {}", snapshot.status);
        ensure!(snapshot.root.canonicalize()? == snapshot.root, "Workspace root moved or became a symlink");
        let current = self.scan(&snapshot.root, &snapshot.excluded, None)?;
        let changed = snapshot.changed_paths();
        // Preflight all conflicts and backup checksums before modifying anything.
        for path in &changed {
            ensure!(current.get(path) == snapshot.after.get(path) || current.get(path) == snapshot.before.get(path),
                "Rollback conflict at {}; file changed after the action", path.display());
            if matches!(snapshot.after.get(path), Some(Entry::Directory { .. })) && !matches!(snapshot.before.get(path), Some(Entry::Directory { .. })) {
                ensure!(!current.keys().any(|p| p != path && p.starts_with(path) && !changed.contains(p)),
                    "Rollback conflict: new untracked files under {}", path.display());
            }
            if let Some(Entry::File { digest, .. }) = snapshot.before.get(path) {
                self.blob(&snapshot.id, digest)?;
            }
        }
        snapshot.status = "restoring".into();
        self.save(&snapshot)?;
        let mut deepest = changed.clone();
        deepest.sort_by_key(|p| std::cmp::Reverse(p.components().count()));
        for path in &deepest {
            if current.get(path) == snapshot.before.get(path) { continue; }
            let full = snapshot.root.join(path);
            match current.get(path) {
                Some(Entry::Directory { .. }) if matches!(snapshot.before.get(path), Some(Entry::Directory { .. })) => {},
                Some(Entry::Directory { .. }) => fs::remove_dir(&full)?,
                Some(Entry::Symlink { directory:true, .. }) if cfg!(windows) => fs::remove_dir(&full)?,
                Some(_) => {
                    #[cfg(windows)]
                    if let Some(Entry::File { .. }) = current.get(path) {
                        let mut p = fs::metadata(&full)?.permissions(); p.set_readonly(false); fs::set_permissions(&full, p)?;
                    }
                    fs::remove_file(&full)?;
                }
                None => {},
            }
        }
        let mut shallow = changed.clone();
        shallow.sort_by_key(|p| p.components().count());
        for path in &shallow {
            if !snapshot.before.contains_key(path) { continue; }
            let full = snapshot.root.join(path);
            // Never traverse a symlink ancestor while restoring stored bytes.
            self.check_parents(&snapshot.root, path)?;
            match snapshot.before.get(path) {
                Some(Entry::Directory { .. }) => fs::create_dir_all(&full)?,
                Some(Entry::File { digest, permissions }) => {
                    let bytes = self.blob(&snapshot.id, digest)?;
                    atomic_write(&full, &bytes)?;
                    permissions.restore(&full)?;
                }
                Some(Entry::Symlink { target, directory }) => {
                    if fs::symlink_metadata(&full).is_ok() { continue; }
                    #[cfg(unix)]
                    { let _ = directory; std::os::unix::fs::symlink(target, &full)?; }
                    #[cfg(windows)]
                    if *directory { std::os::windows::fs::symlink_dir(target, &full)?; }
                    else { std::os::windows::fs::symlink_file(target, &full)?; }
                }
                None => {},
            }
        }
        for path in &deepest {
            if let Some(Entry::Directory { permissions }) = snapshot.before.get(path) { permissions.restore(&snapshot.root.join(path))?; }
        }
        snapshot.status = "rolled_back".into();
        self.save(&snapshot)?;
        Ok(snapshot)
    }

    pub fn diff(&self, id: &str) -> Result<String> {
        let snapshot = self.load(id)?;
        let mut output = String::new();
        for path in snapshot.changed_paths() {
            let before = match snapshot.before.get(&path) {
                Some(Entry::File { digest, .. }) => self.blob(id, digest)?,
                _ => Vec::new(),
            };
            let after = match snapshot.after.get(&path) {
                Some(Entry::File { .. }) => {
                    self.check_parents(&snapshot.root, &path)?;
                    ensure!(!fs::symlink_metadata(snapshot.root.join(&path))?.file_type().is_symlink(), "File became a symlink");
                    fs::read(snapshot.root.join(&path))?
                }
                _ => Vec::new(),
            };
            if let (Ok(before), Ok(after)) = (std::str::from_utf8(&before), std::str::from_utf8(&after)) {
                output.push_str(&preview(&path, before, after));
            } else { output.push_str(&format!("Binary change: {}\n", path.display())); }
            if output.len() > 256 * 1024 { output.push_str("[diff truncated]\n"); break; }
        }
        Ok(output)
    }

    fn check_parents(&self, root: &Path, relative: &Path) -> Result<()> {
        let mut parent = root.to_owned();
        for component in relative.parent().unwrap_or(Path::new("")).components() {
            parent.push(component);
            ensure!(fs::symlink_metadata(&parent)?.is_dir(), "Unsafe restore ancestor: {}", parent.display());
        }
        Ok(())
    }

    fn blob(&self, id: &str, digest: &str) -> Result<Vec<u8>> {
        ensure!(digest.len() == 64 && digest.chars().all(|c| c.is_ascii_hexdigit()), "Invalid blob digest");
        let bytes = fs::read(self.directory.join(id).join("blobs").join(digest))?;
        ensure!(hash(&bytes) == digest, "Snapshot backup checksum mismatch");
        Ok(bytes)
    }

    fn save(&self, snapshot: &Snapshot) -> Result<()> {
        atomic_write(&self.directory.join(&snapshot.id).join("manifest.json"), &serde_json::to_vec(snapshot)?)
    }

    fn scan(&self, root: &Path, excluded: &[PathBuf], blobs: Option<&Path>) -> Result<BTreeMap<PathBuf, Entry>> {
        let mut entries = BTreeMap::new();
        let mut bytes_total = 0;
        for item in WalkDir::new(root).follow_links(false).into_iter().filter_entry(|e| {
            e.depth() == 0 || (!excluded.iter().any(|p| e.path().starts_with(p))
                && !EXCLUDED_DIRS.iter().any(|name| e.file_name() == *name))
        }) {
            let item = item?;
            if item.depth() == 0 { continue; }
            if blobs.is_some() { ensure!(entries.len() < 10_000, "Snapshot exceeds 10000 entries; choose a narrower workspace"); }
            let path = item.path();
            let metadata = fs::symlink_metadata(path)?;
            let entry = if metadata.file_type().is_symlink() {
                Entry::Symlink { target:fs::read_link(path)?, directory:path.is_dir() }
            } else if metadata.is_dir() {
                Entry::Directory { permissions:Permissions::capture(&metadata) }
            } else if metadata.is_file() {
                let digest = if let Some(blobs) = blobs {
                    ensure!(metadata.len() <= MAX_FILE_BYTES, "{} exceeds the 16 MiB snapshot file limit", path.display());
                    bytes_total += metadata.len();
                    ensure!(bytes_total <= MAX_BYTES, "Workspace exceeds the 128 MiB snapshot limit");
                    use std::io::Read;
                    let mut bytes = Vec::new();
                    fs::File::open(path)?.take(MAX_FILE_BYTES + 1).read_to_end(&mut bytes)?;
                    ensure!(bytes.len() as u64 <= MAX_FILE_BYTES, "File grew beyond the snapshot limit");
                    let digest = hash(&bytes);
                    let blob = blobs.join(&digest);
                    if !blob.exists() { atomic_write(&blob, &bytes)?; }
                    digest
                } else {
                    use std::io::Read;
                    let mut file = fs::File::open(path)?;
                    let mut digest = Sha256::new();
                    let mut buffer = [0; 65_536];
                    loop {
                        let count = file.read(&mut buffer)?;
                        if count == 0 { break; }
                        digest.update(&buffer[..count]);
                    }
                    format!("{:x}", digest.finalize())
                };
                Entry::File { digest, permissions:Permissions::capture(&metadata) }
            } else { bail!("Unsupported special file in snapshot: {}", path.display()); };
            entries.insert(path.strip_prefix(root)?.to_owned(), entry);
        }
        Ok(entries)
    }
}

fn hash(bytes: &[u8]) -> String { format!("{:x}", Sha256::digest(bytes)) }

fn atomic_write(path: &Path, bytes: &[u8]) -> Result<()> {
    let mut temp = tempfile::NamedTempFile::new_in(path.parent().context("Path needs a parent")?)?;
    temp.write_all(bytes)?;
    temp.as_file().sync_all()?;
    temp.persist(path)?;
    #[cfg(unix)]
    fs::File::open(path.parent().unwrap())?.sync_all()?;
    Ok(())
}

pub fn preview(path: &Path, before: &str, after: &str) -> String {
    similar::TextDiff::from_lines(before, after).unified_diff().context_radius(3)
        .header(&format!("a/{}", path.display()), &format!("b/{}", path.display())).to_string()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rollback_restores_modified_deleted_created_files_and_permissions() {
        let root = tempfile::tempdir().unwrap();
        let data = tempfile::tempdir().unwrap();
        fs::write(root.path().join("edited"), "original").unwrap();
        fs::create_dir(root.path().join("removed-dir")).unwrap();
        fs::write(root.path().join("removed-dir/deleted"), [0, 255, 10]).unwrap();
        #[cfg(unix)]
        { use std::os::unix::fs::PermissionsExt; fs::set_permissions(root.path().join("edited"), fs::Permissions::from_mode(0o751)).unwrap(); }
        let store = SnapshotStore::new(data.path().into(), vec![]).unwrap();
        let snapshot = store.capture(root.path(), "test").unwrap();
        fs::write(root.path().join("edited"), "changed").unwrap();
        fs::remove_dir_all(root.path().join("removed-dir")).unwrap();
        fs::create_dir(root.path().join("created-dir")).unwrap();
        fs::write(root.path().join("created-dir/new"), "new").unwrap();
        store.finish(&snapshot.id).unwrap();
        let store = SnapshotStore::new(data.path().into(), vec![]).unwrap();
        store.rollback(&snapshot.id).unwrap();
        assert_eq!(fs::read_to_string(root.path().join("edited")).unwrap(), "original");
        assert_eq!(fs::read(root.path().join("removed-dir/deleted")).unwrap(), [0, 255, 10]);
        assert!(!root.path().join("created-dir").exists());
        #[cfg(unix)]
        { use std::os::unix::fs::PermissionsExt; assert_eq!(fs::metadata(root.path().join("edited")).unwrap().permissions().mode() & 0o777, 0o751); }
    }

    #[test]
    fn conflicts_are_detected_before_any_restore() {
        let root = tempfile::tempdir().unwrap();
        let data = tempfile::tempdir().unwrap();
        fs::write(root.path().join("a"), "before").unwrap();
        fs::write(root.path().join("z"), "before").unwrap();
        let store = SnapshotStore::new(data.path().into(), vec![]).unwrap();
        let snapshot = store.capture(root.path(), "test").unwrap();
        fs::write(root.path().join("a"), "after").unwrap();
        fs::write(root.path().join("z"), "after").unwrap();
        store.finish(&snapshot.id).unwrap();
        fs::write(root.path().join("z"), "external edit").unwrap();
        assert!(store.rollback(&snapshot.id).unwrap_err().to_string().contains("conflict"));
        assert_eq!(fs::read_to_string(root.path().join("a")).unwrap(), "after");
        assert!(store.load("../escape").is_err());
    }

    #[cfg(unix)]
    #[test]
    fn symlinks_are_restored_without_touching_their_targets() {
        let root = tempfile::tempdir().unwrap();
        let data = tempfile::tempdir().unwrap();
        let outside = data.path().join("outside");
        fs::write(&outside, "untouched").unwrap();
        std::os::unix::fs::symlink(&outside, root.path().join("link")).unwrap();
        let store = SnapshotStore::new(data.path().join("snapshots"), vec![]).unwrap();
        let snapshot = store.capture(root.path(), "test").unwrap();
        fs::remove_file(root.path().join("link")).unwrap();
        fs::write(root.path().join("link"), "replacement").unwrap();
        store.finish(&snapshot.id).unwrap();
        store.rollback(&snapshot.id).unwrap();
        assert_eq!(fs::read_link(root.path().join("link")).unwrap(), outside);
        assert_eq!(fs::read_to_string(outside).unwrap(), "untouched");
    }
}
