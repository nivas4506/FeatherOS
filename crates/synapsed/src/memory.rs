use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};
use std::collections::HashSet;
use std::fs;
use std::path::PathBuf;
use uuid::Uuid;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MemoryEntry {
    pub id: String,
    pub timestamp: DateTime<Utc>,
    pub category: String, // "shell_command", "file_chunk", "system_event", "agent_plan"
    pub title: String,
    pub content: String,
    pub tags: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SearchHit {
    pub entry: MemoryEntry,
    pub relevance_score: f64,
    #[serde(default)]
    pub vector_score: f32,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct SemanticJournal {
    entries: Vec<MemoryEntry>,
    #[serde(skip)]
    journal_path: Option<PathBuf>,
    #[serde(skip, default)]
    vectors: std::collections::HashMap<String, Vec<f32>>,
}

impl Default for SemanticJournal {
    fn default() -> Self {
        Self::load_or_new()
    }
}

impl SemanticJournal {
    pub fn new() -> Self {
        Self {
            entries: Vec::new(),
            journal_path: None,
            vectors: std::collections::HashMap::new(),
        }
    }

    pub fn default_storage_path() -> PathBuf {
        feather_ipc::data_dir().join("journal.json")
    }

    pub fn load(path: PathBuf) -> anyhow::Result<Self> {
        let mut journal = if path.exists() {
            serde_json::from_slice::<Self>(&fs::read(&path)?)?
        } else {
            Self::new()
        };
        journal.journal_path = Some(path);
        journal.vectors = std::collections::HashMap::new();
        Ok(journal)
    }

    pub fn persist(&self) -> anyhow::Result<()> {
        use std::io::Write;
        if let Some(path) = &self.journal_path {
            let parent = path.parent().ok_or_else(|| anyhow::anyhow!("journal path has no parent"))?;
            fs::create_dir_all(parent)?;
            let mut temp = tempfile::NamedTempFile::new_in(parent)?;
            serde_json::to_writer(&mut temp, self)?;
            temp.flush()?;
            temp.as_file().sync_all()?;
            temp.persist(path)?;
        }
        Ok(())
    }

    pub fn load_or_new() -> Self {
        let path = Self::default_storage_path();
        if path.exists() {
            if let Ok(data) = fs::read_to_string(&path) {
                if let Ok(mut journal) = serde_json::from_str::<SemanticJournal>(&data) {
                    journal.journal_path = Some(path);
                    return journal;
                }
            }
        }

        Self {
            entries: Vec::new(),
            journal_path: Some(path),
            vectors: std::collections::HashMap::new(),
        }
    }

    pub fn save_to_disk(&self) {
        if let Err(error) = self.persist() {
            eprintln!("Cannot persist semantic journal: {error}");
        }
    }

    pub fn record(&mut self, category: &str, title: &str, content: &str, tags: Vec<String>) -> String {
        let id = Uuid::new_v4().to_string();
        self.vectors.insert(
            id.clone(),
            feather_embed::embed_document(title, content, &tags),
        );
        self.entries.push(MemoryEntry {
            id: id.clone(),
            timestamp: Utc::now(),
            category: category.to_string(),
            title: title.to_string(),
            content: content.to_string(),
            tags,
        });
        if self.entries.len() > 20_000 {
            let removed = self.entries.remove(0);
            self.vectors.remove(&removed.id);
        }
        self.save_to_disk();
        id
    }

    fn vector_for(&mut self, entry: &MemoryEntry) -> Vec<f32> {
        if let Some(v) = self.vectors.get(&entry.id) {
            return v.clone();
        }
        let v = feather_embed::embed_document(&entry.title, &entry.content, &entry.tags);
        self.vectors.insert(entry.id.clone(), v.clone());
        v
    }

    pub fn search(&mut self, query: &str, limit: usize) -> Vec<SearchHit> {
        let query_tokens: HashSet<String> = query
            .to_lowercase()
            .split_whitespace()
            .map(|s| s.trim_matches(|c: char| !c.is_alphanumeric()).to_string())
            .filter(|s| !s.is_empty())
            .collect();

        if query_tokens.is_empty() {
            return Vec::new();
        }

        // Offline dense vector: paraphrases match with no shared tokens.
        let query_vec = feather_embed::embed_text(query);
        let query_has_signal = query_vec.iter().any(|&x| x != 0.0);

        let mut hits = Vec::new();
        let entries = std::mem::take(&mut self.entries);

        for entry in &entries {
            let mut score = 0.0;
            let title_lower = entry.title.to_lowercase();
            let content_lower = entry.content.to_lowercase();

            for token in &query_tokens {
                if title_lower.contains(token) {
                    score += 5.0;
                }
                if content_lower.contains(token) {
                    score += 1.0;
                }
                for tag in &entry.tags {
                    if tag.to_lowercase().contains(token) {
                        score += 3.0;
                    }
                }
            }

            let doc_vec = self.vector_for(entry);
            let vector_score = if query_has_signal {
                feather_embed::cosine(&query_vec, &doc_vec)
            } else {
                0.0
            };

            // Keyword hit, or semantic hit above threshold.
            if score > 0.0 || vector_score > 0.25 {
                let age_seconds = (Utc::now() - entry.timestamp).num_seconds().max(1) as f64;
                let recency_boost = 1.0 / (1.0 + (age_seconds / 86400.0).log10().max(0.0));
                let final_score =
                    score * (1.0 + 0.1 * recency_boost) + f64::from(vector_score) * 10.0;

                hits.push(SearchHit {
                    entry: entry.clone(),
                    relevance_score: final_score,
                    vector_score,
                });
            }
        }

        self.entries = entries;
        // Bound the in-memory vector cache to live entries.
        if self.vectors.len() > self.entries.len() + 512 {
            let live: HashSet<&str> = self.entries.iter().map(|e| e.id.as_str()).collect();
            self.vectors.retain(|k, _| live.contains(k.as_str()));
        }

        hits.sort_by(|a, b| b.relevance_score.partial_cmp(&a.relevance_score).unwrap_or(std::cmp::Ordering::Equal));
        hits.truncate(limit);
        hits
    }

    pub fn total_entries(&self) -> usize {
        self.entries.len()
    }

    pub fn entries(&self) -> &[MemoryEntry] { &self.entries }

    /// Update files by canonical path so watcher events never create duplicate chunks.
    pub fn upsert_file(&mut self, path: &std::path::Path, content: String) {
        let title = path.to_string_lossy().into_owned();
        if let Some(entry) = self.entries.iter_mut().find(|e| e.category == "file_chunk" && e.title == title) {
            if entry.content != content {
                entry.content = content.clone();
                entry.timestamp = Utc::now();
                let vec = feather_embed::embed_document(&entry.title, &content, &entry.tags);
                self.vectors.insert(entry.id.clone(), vec);
            }
        } else {
            let id = Uuid::new_v4().to_string();
            let tags = vec!["source_code".into()];
            self.vectors.insert(id.clone(), feather_embed::embed_document(&title, &content, &tags));
            self.entries.push(MemoryEntry { id, timestamp:Utc::now(),
                category:"file_chunk".into(), title, content, tags });
        }
    }

    pub fn remove_files_under(&mut self, path: &std::path::Path) {
        let doomed: Vec<String> = self.entries.iter()
            .filter(|e| e.category == "file_chunk" && std::path::Path::new(&e.title).starts_with(path))
            .map(|e| e.id.clone()).collect();
        self.entries.retain(|e| e.category != "file_chunk" || !std::path::Path::new(&e.title).starts_with(path));
        for id in doomed { self.vectors.remove(&id); }
    }

    pub fn prune_files(&mut self, root: &std::path::Path, present: &HashSet<PathBuf>) {
        let doomed: Vec<String> = self.entries.iter()
            .filter(|e| e.category == "file_chunk" && std::path::Path::new(&e.title).starts_with(root)
                && !present.contains(std::path::Path::new(&e.title)))
            .map(|e| e.id.clone()).collect();
        self.entries.retain(|e| e.category != "file_chunk" || !std::path::Path::new(&e.title).starts_with(root)
            || present.contains(std::path::Path::new(&e.title)));
        for id in doomed { self.vectors.remove(&id); }
    }
}
