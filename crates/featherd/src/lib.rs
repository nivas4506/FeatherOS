mod state;
mod perception;
pub use state::Daemon;

use anyhow::{Context, Result};
use cortex_core::LlmProvider;
use feather_ipc::{error, read_frame, success, write_frame, Stream};
use fs2::FileExt;
use serde_json::{json, Value};
use std::{fs::OpenOptions, path::PathBuf, sync::Arc};
use tokio::sync::mpsc;
use uuid::Uuid;

pub async fn serve(
    endpoint: &str,
    data_dir: PathBuf,
    workspace: PathBuf,
    provider: Arc<dyn LlmProvider>,
    model: String,
) -> Result<()> {
    std::fs::create_dir_all(&data_dir)?;
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        std::fs::set_permissions(&data_dir, std::fs::Permissions::from_mode(0o700))?;
    }
    let lock = OpenOptions::new().create(true).truncate(false).read(true).write(true)
        .open(data_dir.join("featherd.lock"))?;
    lock.try_lock_exclusive().context("Another daemon owns this data directory")?;
    let daemon = Arc::new(Daemon::new(data_dir, workspace, provider, model)?);
    let background = daemon.start_perception().await?;
    let mut connections = tokio::task::JoinSet::new();

    #[cfg(unix)]
    {
        use std::os::unix::fs::{FileTypeExt, PermissionsExt};
        let path = std::path::Path::new(endpoint);
        if path.exists() {
            anyhow::ensure!(std::fs::symlink_metadata(path)?.file_type().is_socket(), "Endpoint is not a socket");
            anyhow::ensure!(tokio::net::UnixStream::connect(path).await.is_err(), "Endpoint is already serving clients");
            std::fs::remove_file(path)?;
        }
        let listener = tokio::net::UnixListener::bind(path)?;
        std::fs::set_permissions(path, std::fs::Permissions::from_mode(0o600))?;
        eprintln!("featherd listening on {endpoint}");
        let mut terminate = tokio::signal::unix::signal(tokio::signal::unix::SignalKind::terminate())?;
        loop {
            tokio::select! {
                _ = tokio::signal::ctrl_c() => break,
                _ = terminate.recv() => break,
                Some(_) = connections.join_next(), if !connections.is_empty() => {},
                accepted = listener.accept() => {
                    let (stream, _) = accepted?;
                    let daemon = daemon.clone();
                    connections.spawn(async move { connection(Box::new(stream), daemon).await });
                }
            }
        }
        std::fs::remove_file(path)?;
    }
    #[cfg(windows)]
    {
        use tokio::net::windows::named_pipe::ServerOptions;
        let mut listener = ServerOptions::new().first_pipe_instance(true).reject_remote_clients(true).create(endpoint)?;
        eprintln!("featherd listening on {endpoint}");
        loop {
            tokio::select! {
                _ = tokio::signal::ctrl_c() => break,
                Some(_) = connections.join_next(), if !connections.is_empty() => {},
                connected = listener.connect() => {
                    connected?;
                    let next = ServerOptions::new().reject_remote_clients(true).create(endpoint)?;
                    let stream = std::mem::replace(&mut listener, next);
                    let daemon = daemon.clone();
                    connections.spawn(async move { connection(Box::new(stream), daemon).await });
                }
            }
        }
    }
    // Stop accepting work and allow the current serialized action to finish.
    daemon.stopping.store(true, std::sync::atomic::Ordering::Relaxed);
    let _idle = daemon.actions.lock().await;
    connections.abort_all();
    while connections.join_next().await.is_some() {}
    background.abort();
    let _ = background.await;
    daemon.journal.lock().await.persist()?;
    Ok(())
}

pub async fn connection(stream: Stream, daemon: Arc<Daemon>) -> Result<()> {
    let session = Uuid::new_v4().to_string();
    daemon.sessions.lock().await.insert(session.clone(), daemon.workspace.clone());
    let result = connection_inner(stream, daemon.clone(), &session).await;
    daemon.sessions.lock().await.remove(&session);
    daemon.plans.lock().await.retain(|_, p| p.session != session);
    result
}

async fn connection_inner(stream: Stream, daemon: Arc<Daemon>, session: &str) -> Result<()> {
    let (mut reader, mut writer) = tokio::io::split(stream);
    let (tx, mut rx) = mpsc::channel::<Value>(32);
    let mut events = daemon.events.subscribe();
    let writer_task = tokio::spawn(async move {
        loop {
            let message = tokio::select! {
                biased;
                message = rx.recv() => match message { Some(v) => v, None => break },
                event = events.recv() => match event {
                    Ok(v) => v,
                    Err(tokio::sync::broadcast::error::RecvError::Lagged(n)) =>
                        feather_ipc::notification("events.gap", json!({"missed":n})),
                    Err(_) => break,
                }
            };
            tokio::time::timeout(std::time::Duration::from_secs(10), write_frame(&mut writer, &message)).await??;
        }
        Ok::<_, anyhow::Error>(())
    });
    let result = async {
        while let Some(bytes) = read_frame(&mut reader).await? {
            let request: Value = match serde_json::from_slice(&bytes) {
                Ok(v) => v,
                Err(e) => { tx.send(error(Value::Null, -32700, e)).await?; continue; }
            };
            let id = request.get("id").cloned();
            let valid_id = id.as_ref().is_none_or(|v| v.is_null() || v.is_string() || v.is_number());
            if request["jsonrpc"] != "2.0" || !request["method"].is_string() || !valid_id {
                tx.send(error(Value::Null, -32600, "Invalid JSON-RPC request")).await?;
                continue;
            }
            if request.get("params").is_some_and(|v| !v.is_object()) {
                if let Some(id) = id { tx.send(error(id, -32602, "params must be an object")).await?; }
                continue;
            }
            let method = request["method"].as_str().unwrap();
            // Notifications have no response and cannot start mutative work.
            let Some(id) = id else { continue; };
            let params = request.get("params").cloned().unwrap_or(json!({}));
            let response = match daemon.dispatch(session, method, params).await {
                Ok(v) => success(id, v),
                Err(e) => {
                    let code = if e.to_string().starts_with("Unknown method") { -32601 } else { -32000 };
                    error(id, code, format!("{e:#}"))
                }
            };
            tx.send(response).await?;
        }
        Ok(())
    }.await;
    drop(tx);
    writer_task.abort();
    let _ = writer_task.await;
    result
}
