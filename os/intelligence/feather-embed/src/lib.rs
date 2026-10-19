//! Local neural embedding engine (Phase 2, Pillar 3).
//!
//! 100% offline, pure-Rust, zero-model-download baseline:
//! - 256-dim L2-normalized vectors
//! - dims 0..32 are semantic concepts (so "network issues" matches
//!   "socket timeout" with no shared tokens)
//! - remaining dims are hashed lexical TF for exact/bigram matches
//!
//! The API is transformer-ready: drop a quantized ONNX/Candle model into
//! `~/.feather/models/` and implement `TransformerEmbedder: Embedder` without
//! changing `synapsed` or `featherd`.

pub const DIM: usize = 256;
pub const CONCEPT_DIMS: usize = 32;
pub const MODEL_DIR_NAME: &str = "models";

/// Canonical concept groups. Each group owns one of the first 32 dims.
const CONCEPTS: &[&[&str]] = &[
    &["network", "socket", "connection", "connect", "timeout", "timed", "refused", "dns", "tcp", "udp", "http", "ping", "latency", "unreachable", "reset", "issue", "issues", "error", "failed", "failure"],
    &["disk", "storage", "space", "volume", "filesystem", "large", "quota", "partition"],
    &["memory", "ram", "swap", "oom", "heap", "leak"],
    &["cpu", "load", "processor", "core", "usage", "throttle"],
    &["process", "pid", "port", "listen", "listening", "socket", "kill", "hang"],
    &["git", "commit", "branch", "merge", "conflict", "repo", "push", "pull", "rebase"],
    &["build", "compile", "cache", "target", "artifact", "clean", "cargo", "npm"],
    &["file", "directory", "folder", "path", "find", "search", "locate", "list"],
    &["permission", "denied", "access", "owner", "chmod", "chown", "sudo"],
    &["test", "fail", "pass", "assert", "panic", "exception", "traceback"],
    &["config", "setting", "env", "variable", "yaml", "toml", "json", "ini"],
    &["auth", "login", "token", "password", "key", "certificate", "tls", "ssh"],
    &["database", "sql", "query", "postgres", "sqlite", "migration", "table"],
    &["docker", "container", "image", "volume", "k8s", "pod", "compose"],
    &["log", "warn", "debug", "trace", "event", "audit"],
    &["shell", "command", "terminal", "script", "prompt", "history"],
];

pub trait Embedder: Send + Sync {
    fn dim(&self) -> usize { DIM }
    fn embed(&self, text: &str) -> Vec<f32>;
}

/// Default offline embedder. Deterministic (FNV-1a, no random seed).
#[derive(Default, Clone, Copy)]
pub struct HashEmbedder;

impl Embedder for HashEmbedder {
    fn embed(&self, text: &str) -> Vec<f32> {
        embed_text(text)
    }
}

pub fn embed_text(text: &str) -> Vec<f32> {
    let mut v = vec![0f32; DIM];
    let tokens = tokenize(text);
    if tokens.is_empty() {
        return v;
    }
    // Concept dims: strong signal so paraphrases match.
    for (i, group) in CONCEPTS.iter().enumerate() {
        let mut hits = 0f32;
        for t in &tokens {
            if group.contains(&t.as_str()) {
                hits += 1.0;
            }
        }
        if hits > 0.0 {
            v[i] = 2.0 * hits;
        }
    }
    // Lexical dims: hashed unigrams + bigrams.
    for t in &tokens {
        let h = fnv1a(t) as usize % (DIM - CONCEPT_DIMS);
        v[CONCEPT_DIMS + h] += 1.0;
    }
    for w in tokens.windows(2) {
        let bigram = format!("{} {}", w[0], w[1]);
        let h = fnv1a(&bigram) as usize % (DIM - CONCEPT_DIMS);
        v[CONCEPT_DIMS + h] += 0.5;
    }
    l2_normalize(&mut v);
    v
}

pub fn embed_document(title: &str, content: &str, tags: &[String]) -> Vec<f32> {
    // Title weighted x3, tags x2, content x1 (truncated for speed).
    let content: String = content.chars().take(4000).collect();
    let mut combined = String::new();
    for _ in 0..3 {
        combined.push_str(title);
        combined.push(' ');
    }
    for tag in tags {
        combined.push_str(tag);
        combined.push(' ');
        combined.push_str(tag);
        combined.push(' ');
    }
    combined.push_str(&content);
    embed_text(&combined)
}

pub fn cosine(a: &[f32], b: &[f32]) -> f32 {
    a.iter().zip(b.iter()).map(|(x, y)| x * y).sum()
}

fn tokenize(text: &str) -> Vec<String> {
    text.to_lowercase()
        .split(|c: char| !c.is_alphanumeric())
        .filter(|s| s.len() >= 2)
        .map(|s| s.to_owned())
        .collect()
}

fn fnv1a(s: &str) -> u64 {
    let mut h: u64 = 0xcbf29ce484222325;
    for b in s.bytes() {
        h ^= b as u64;
        h = h.wrapping_mul(0x100000001b3);
    }
    h
}

fn l2_normalize(v: &mut [f32]) {
    let n: f32 = v.iter().map(|x| x * x).sum::<f32>().sqrt();
    if n > 1e-9 {
        for x in v.iter_mut() {
            *x /= n;
        }
    }
}

/// Future transformer slot: returns None until a model file is installed.
pub fn local_model_status(data_dir: &std::path::Path) -> Option<String> {
    let dir = data_dir.join(MODEL_DIR_NAME);
    std::fs::read_dir(&dir).ok()?.filter_map(|e| e.ok()).find(|e| {
        e.path()
            .extension()
            .is_some_and(|ext| ext == "onnx" || ext == "safetensors" || ext == "gguf")
    })?;
    Some(format!("custom model found in {}", dir.display()))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn paraphrase_matches_without_shared_tokens() {
        let q = embed_text("network issues");
        let good = embed_text("socket timeout connection refused");
        let bad = embed_text("banana pancake recipe delightful");
        assert!(cosine(&q, &good) > 0.35, "got {}", cosine(&q, &good));
        assert!(cosine(&q, &good) > cosine(&q, &bad));
    }

    #[test]
    fn embeddings_are_deterministic_and_normalized() {
        let a = embed_text("check disk space and clean build cache");
        let b = embed_text("check disk space and clean build cache");
        assert_eq!(a, b);
        let n: f32 = a.iter().map(|x| x * x).sum::<f32>().sqrt();
        assert!((n - 1.0).abs() < 1e-5);
        assert_eq!(a.len(), DIM);
    }

    #[test]
    fn empty_text_is_zero_vector() {
        assert!(embed_text("!!!").iter().all(|&x| x == 0.0));
    }
}
