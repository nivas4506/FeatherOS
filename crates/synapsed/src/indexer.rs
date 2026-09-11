use crate::memory::SemanticJournal;
use anyhow::Result;
use std::fs;
use std::path::Path;
use walkdir::WalkDir;

pub struct FileSystemIndexer;

impl FileSystemIndexer {
    pub fn index_directory(journal: &mut SemanticJournal, root: &Path, max_files: usize) -> Result<usize> {
        let mut indexed_count = 0;

        for entry in WalkDir::new(root)
            .max_depth(4)
            .into_iter()
            .filter_entry(|e| {
                let name = e.file_name().to_string_lossy();
                !name.starts_with('.') && name != "target" && name != "node_modules"
            })
            .filter_map(|e| e.ok())
        {
            if entry.file_type().is_file() {
                let path = entry.path();
                // Index text and code files only
                if let Some(ext) = path.extension().and_then(|s| s.to_str()) {
                    if matches!(ext, "rs" | "toml" | "json" | "md" | "sh" | "yaml" | "yml" | "py" | "c" | "h" | "txt") {
                        if let Ok(content) = fs::read_to_string(path) {
                            let truncated: String = content.lines().take(60).collect::<Vec<&str>>().join("\n");
                            let path_str = path.to_string_lossy().to_string();
                            let tags = vec![ext.to_string(), "source_code".to_string()];

                            journal.record("file_chunk", &path_str, &truncated, tags);
                            indexed_count += 1;

                            if indexed_count >= max_files {
                                break;
                            }
                        }
                    }
                }
            }
        }

        Ok(indexed_count)
    }
}
