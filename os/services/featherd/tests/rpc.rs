use cortex_core::HeuristicProvider;
use feather_ipc::{read_frame, write_frame};
use serde_json::{json, Value};
use std::sync::Arc;

#[tokio::test]
async fn sessions_share_memory_and_plans_are_single_use() {
    let dir = tempfile::tempdir().unwrap();
    let daemon = featherd::Daemon::new(dir.path().join("data"), dir.path().into(), Arc::new(HeuristicProvider), "test".into()).unwrap();
    daemon.dispatch("a", "session.open", json!({})).await.unwrap();
    daemon.dispatch("b", "session.open", json!({})).await.unwrap();
    let planned = daemon.dispatch("a", "intent.plan_and_execute", json!({"intent":"git status"})).await.unwrap();
    let id = &planned["plan_id"];
    assert!(daemon.dispatch("b", "plan.execute", json!({"plan_id":id})).await.is_err());
    daemon.dispatch("a", "plan.cancel", json!({"plan_id":id})).await.unwrap();
    assert!(daemon.dispatch("a", "plan.execute", json!({"plan_id":id})).await.is_err());
    daemon.journal.lock().await.record("shell_command", "shared context", "socket timeout", vec![]);
    let hits = daemon.dispatch("b", "memory.search", json!({"query":"socket"})).await.unwrap();
    assert_eq!(hits.as_array().unwrap().len(), 1);
    assert!(daemon.dispatch("a", "command.execute", json!({"command":"rm -rf /","confirmed":true})).await.is_err());
}

#[tokio::test]
async fn malformed_requests_do_not_break_connection() {
    let dir = tempfile::tempdir().unwrap();
    let daemon = Arc::new(featherd::Daemon::new(dir.path().join("data"), dir.path().into(), Arc::new(HeuristicProvider), "test".into()).unwrap());
    let (mut client, server) = tokio::io::duplex(4096);
    let task = tokio::spawn(featherd::connection(Box::new(server), daemon));
    for (request, code) in [(json!({"id":1,"method":"ping"}), -32600),
        (json!({"jsonrpc":"2.0","id":2,"method":"missing"}), -32601),
        (json!({"jsonrpc":"2.0","id":3,"method":"ping","params":[]}), -32602)] {
        write_frame(&mut client, &request).await.unwrap();
        let bytes = read_frame(&mut client).await.unwrap().unwrap();
        let response: Value = serde_json::from_slice(&bytes).unwrap();
        assert_eq!(response["error"]["code"], code);
    }
    write_frame(&mut client, &json!({"jsonrpc":"2.0","id":"ok","method":"ping"})).await.unwrap();
    let response: Value = serde_json::from_slice(&read_frame(&mut client).await.unwrap().unwrap()).unwrap();
    assert_eq!(response, json!({"jsonrpc":"2.0","id":"ok","result":{}}));
    drop(client);
    task.await.unwrap().unwrap();
}

#[tokio::test]
async fn watcher_tracks_creation_modification_rename_and_deletion() {
    let dir = tempfile::tempdir().unwrap();
    let data = tempfile::tempdir().unwrap();
    let daemon = Arc::new(featherd::Daemon::new(data.path().into(), dir.path().into(), Arc::new(HeuristicProvider), "test".into()).unwrap());
    let task = daemon.start_perception().await.unwrap();
    let a = dir.path().join("a.rs");
    let b = dir.path().join("b.rs");
    std::fs::write(&a, "watcher_creation").unwrap();
    wait_for(&daemon, "watcher_creation", 1).await;
    std::fs::write(&a, "watcher_update").unwrap();
    wait_for(&daemon, "watcher_update", 1).await;
    std::fs::rename(&a, &b).unwrap();
    tokio::time::timeout(std::time::Duration::from_secs(5), async {
        loop {
            let entries = daemon.journal.lock().await;
            if entries.entries().iter().any(|e| e.title == b.to_string_lossy()) && entries.total_entries() == 1 { break; }
            drop(entries);
            tokio::time::sleep(std::time::Duration::from_millis(100)).await;
        }
    }).await.unwrap();
    std::fs::remove_file(b).unwrap();
    wait_for(&daemon, "watcher_update", 0).await;
    task.abort();
    let _ = task.await;
}

#[tokio::test]
async fn semantic_search_matches_paraphrase_without_shared_tokens() {
    let dir = tempfile::tempdir().unwrap();
    let daemon = featherd::Daemon::new(dir.path().join("data"), dir.path().into(), Arc::new(HeuristicProvider), "test".into()).unwrap();
    daemon.journal.lock().await.record("shell_command", "db outage", "socket timeout connection refused", vec![]);
    daemon.journal.lock().await.record("shell_command", "lunch", "banana pancake recipe delightful", vec![]);
    let hits = daemon.dispatch("a", "memory.search", json!({"query":"network issues"})).await.unwrap();
    let hits = hits.as_array().unwrap();
    assert!(!hits.is_empty());
    assert!(hits[0]["entry"]["content"].as_str().unwrap().contains("socket timeout"));
    assert!(hits[0]["vector_score"].as_f64().unwrap() > 0.25);
}

async fn wait_for(daemon: &featherd::Daemon, query: &str, count: usize) {
    tokio::time::timeout(std::time::Duration::from_secs(5), async {
        loop {
            if daemon.journal.lock().await.search(query, 10).len() == count { break; }
            tokio::time::sleep(std::time::Duration::from_millis(100)).await;
        }
    }).await.unwrap();
}
