# FeatherOS Phase 2 — Setup & Operator Notes

Phase 2 turns `feather` into a client of a persistent background service (`featherd`).

## Binaries

| Binary | Crate | Purpose |
|---|---|---|
| `featherd` | `crates/featherd` | Background daemon: sessions, planner, executor, watcher, snapshots, vector memory |
| `feather` / `aish` | `crates/aish` | CLI shell client (JSON-RPC over socket/pipe) |
| `feather-tui` | `crates/feather-tui` | Dual-pane dashboard: shell + cognitive inspector |

## Quickstart (Linux/macOS)

```bash
cargo build --release -p featherd -p aish -p feather-tui

# 1. Start the daemon (workspace = directory it watches/indexes)
./target/release/featherd --workspace ~/my-project &
# endpoint defaults to ~/.feather/featherd.sock

# 2. Shell
./target/release/feather --help
./target/release/feather -c "check disk space"
./target/release/feather
feather:~/my-project> check disk space and find largest folders
feather:~/my-project> :undo        # restore workspace files from last action
feather:~/my-project> :diff        # unified diff of last snapshot
feather:~/my-project> :memory network issues   # semantic search (matches "socket timeout")

# 3. TUI
./target/release/feather-tui
```

Windows (PowerShell):

```powershell
cargo build --release -p featherd -p aish -p feather-tui
Start-Process .\target\release\featherd.exe --workspace $HOME\my-project
.\target\release\feather.exe -c "check disk space"
.\target\release\feather-tui.exe
# endpoint defaults to \\.\pipe\featherd
```

Env overrides:

- `FEATHER_ENDPOINT` — explicit socket path / pipe name
- `FEATHER_HOME` — data dir (journal, snapshots, shell history). Defaults to `~/.feather`

## Background service templates

Examples only — adjust binary paths:

- `docs/featherd.service.example` → copy to `~/.config/systemd/user/featherd.service`, then
  `systemctl --user daemon-reload && systemctl --user enable --now featherd`
- `docs/com.feather.daemon.plist.example` → copy to `~/Library/LaunchAgents/com.feather.daemon.plist`, then
  `launchctl load ~/Library/LaunchAgents/com.feather.daemon.plist`
- Windows: Task Scheduler → run `featherd.exe --workspace %USERPROFILE%` at logon,
  or `sc.exe create featherd binPath= "...\featherd.exe --workspace ..." start= auto`

## What `:undo` covers (important)

- Snapshots cover **workspace files only**: modified, deleted, and created files/directories, plus permissions and symlinks.
- Excluded: `.git`, `.feather`, `target`, `node_modules`, `__pycache__`, daemon data dir.
- Limits: 10k entries / 128 MiB total / 16 MiB per file. Larger trees fail fast before executing.
- Rollback refuses when files changed after the action (conflict) and restores nothing in that case.
- External effects (package installs, `git push`, network calls, DB writes) are **not** reversible — the result reports `rollback_scope` explicitly.

## Vector memory (Pillar 3)

- `crates/feather-embed`: 256-dim offline vectors — 32 concept dims (network/disk/memory/…) + hashed lexical dims, L2-normalized cosine search.
- Proves the blueprint example: `network issues` matches `socket timeout` / `connection refused` with no shared tokens (`vector_score > 0.25`).
- 100% offline, no download. `daemon.status` reports `{embedding: {dim, offline, custom_model}}`.
- To plug a transformer later: drop `.onnx`/`.safetensors`/`.gguf` into `~/.feather/models/` and implement `TransformerEmbedder: Embedder` — no `synapsed`/`featherd` API change needed.
- Vectors are cached in memory only (`journal.json` stays small); they are recomputed deterministically on startup.

## Verification

```bash
cargo test --workspace   # 15 tests: IPC framing, sessions/plans, watcher, snapshots, embeddings, semantic RPC
```
