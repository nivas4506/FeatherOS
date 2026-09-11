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
}

#[derive(Debug, Serialize, Deserialize)]
pub struct SemanticJournal {
    entries: Vec<MemoryEntry>,
    #[serde(skip)]
    journal_path: Option<PathBuf>,
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
        }
    }

    pub fn default_storage_path() -> PathBuf {
        let home = std::env::var("HOME")
            .or_else(|_| std::env::var("USERPROFILE"))
            .unwrap_or_else(|_| ".".to_string());
        PathBuf::from(home).join(".feather").join("journal.json")
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
        }
    }

    pub fn save_to_disk(&self) {
        if let Some(path) = &self.journal_path {
            if let Some(parent) = path.parent() {
                let _ = fs::create_dir_all(parent);
            }
            if let Ok(serialized) = serde_json::to_string_pretty(self) {
                let _ = fs::write(path, serialized);
            }
        }
    }

    pub fn record(&mut self, category: &str, title: &str, content: &str, tags: Vec<String>) -> String {
        let id = Uuid::new_v4().to_string();
        self.entries.push(MemoryEntry {
            id: id.clone(),
            timestamp: Utc::now(),
            category: category.to_string(),
            title: title.to_string(),
            content: content.to_string(),
            tags,
        });
        self.save_to_disk();
        id
    }

    pub fn search(&self, query: &str, limit: usize) -> Vec<SearchHit> {
        let query_tokens: HashSet<String> = query
            .to_lowercase()
            .split_whitespace()
            .map(|s| s.trim_matches(|c: char| !c.is_alphanumeric()).to_string())
            .filter(|s| !s.is_empty())
            .collect();

        if query_tokens.is_empty() {
            return Vec::new();
        }

        let mut hits = Vec::new();

        for entry in &self.entries {
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

            if score > 0.0 {
                let age_seconds = (Utc::now() - entry.timestamp).num_seconds().max(1) as f64;
                let recency_boost = 1.0 / (1.0 + (age_seconds / 86400.0).log10().max(0.0));
                let final_score = score * (1.0 + 0.1 * recency_boost);

                hits.push(SearchHit {
                    entry: entry.clone(),
                    relevance_score: final_score,
                });
            }
        }

        hits.sort_by(|a, b| b.relevance_score.partial_cmp(&a.relevance_score).unwrap_or(std::cmp::Ordering::Equal));
        hits.truncate(limit);
        hits
    }

    pub fn total_entries(&self) -> usize {
        self.entries.len()
    }
}
