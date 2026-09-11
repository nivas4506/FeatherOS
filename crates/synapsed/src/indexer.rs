use crate::memory::SemanticJournal;
use anyhow::Result;
use std::{collections::HashSet, fs, path::Path};
use walkdir::WalkDir;

pub struct FileSystemIndexer;

impl FileSystemIndexer {
    pub fn eligible(root: &Path, path: &Path) -> bool {
        path.strip_prefix(root).is_ok_and(|relative| relative.components().all(|c| {
            let name = c.as_os_str().to_string_lossy();
            !name.starts_with('.') && !matches!(name.as_ref(), "target" | "node_modules" | "__pycache__" | "vendor")
        }))
    }

    pub fn index_file(journal: &mut SemanticJournal, path: &Path) -> Result<bool> {
        let extension = path.extension().and_then(|s| s.to_str()).unwrap_or("");
        if !matches!(extension, "rs" | "toml" | "json" | "md" | "sh" | "yaml" | "yml" | "py" | "c" | "h" | "txt" | "js" | "ts" | "tsx" | "jsx" | "go" | "html" | "css") {
            return Ok(false);
        }
        let metadata = fs::symlink_metadata(path)?;
        if !metadata.is_file() || metadata.len() > 1024 * 1024 {
            journal.remove_files_under(path);
            return Ok(false);
        }
        let bytes = fs::read(path)?;
        let Ok(content) = String::from_utf8(bytes) else { journal.remove_files_under(path); return Ok(false); };
        let content: String = content.lines().take(100).collect::<Vec<_>>().join("\n").chars().take(16_384).collect();
        journal.upsert_file(path, content);
        Ok(true)
    }

    pub fn index_directory(journal: &mut SemanticJournal, root: &Path, max_files: usize) -> Result<usize> {
        Self::index_directory_excluding(journal, root, max_files, &[])
    }

    pub fn index_directory_excluding(journal: &mut SemanticJournal, root: &Path, max_files: usize, excluded: &[std::path::PathBuf]) -> Result<usize> {
        if max_files == 0 { return Ok(0); }
        let root = root.canonicalize()?;
        let mut present = HashSet::new();
        let mut complete = true;
        for entry in WalkDir::new(&root).follow_links(false).into_iter()
            .filter_entry(|e| (e.depth() == 0 || Self::eligible(&root, e.path())) && !excluded.iter().any(|p| e.path().starts_with(p))) {
            let entry = match entry { Ok(e) => e, Err(_) => { complete = false; continue; } };
            if entry.file_type().is_file() {
                match Self::index_file(journal, entry.path()) {
                    Ok(true) => { present.insert(entry.path().to_owned()); }
                    Ok(false) => {},
                    Err(_) => { complete = false; }
                }
                if present.len() >= max_files { complete = false; break; }
            }
        }
        if complete { journal.prune_files(&root, &present); }
        journal.persist()?;
        Ok(present.len())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn indexing_upserts_prunes_and_skips_symlinks_and_builds() {
        let temp = tempfile::tempdir().unwrap();
        fs::write(temp.path().join("a.rs"), "old").unwrap();
        fs::create_dir(temp.path().join("target")).unwrap();
        fs::write(temp.path().join("target/generated.rs"), "skip").unwrap();
        let mut journal = SemanticJournal::new();
        assert_eq!(FileSystemIndexer::index_directory(&mut journal, temp.path(), 100).unwrap(), 1);
        fs::write(temp.path().join("a.rs"), "new").unwrap();
        FileSystemIndexer::index_directory(&mut journal, temp.path(), 100).unwrap();
        assert_eq!(journal.total_entries(), 1);
        assert_eq!(journal.search("new", 10).len(), 1);
        fs::rename(temp.path().join("a.rs"), temp.path().join("b.rs")).unwrap();
        FileSystemIndexer::index_directory(&mut journal, temp.path(), 100).unwrap();
        assert_eq!(journal.total_entries(), 1);
        fs::remove_file(temp.path().join("b.rs")).unwrap();
        FileSystemIndexer::index_directory(&mut journal, temp.path(), 100).unwrap();
        assert_eq!(journal.total_entries(), 0);
    }
}
