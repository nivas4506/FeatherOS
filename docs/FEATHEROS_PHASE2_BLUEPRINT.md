# FeatherOS: Phase 2 Architectural Blueprint & Roadmap

## 1. Executive Summary

In **Phase 1**, we built the core substrate of FeatherOS: the cross-platform dual-mode cognitive shell (`feather`), the safety guard (`aegis-exec`), the MCP tool bus (`agent-bus`), system telemetry (`cortex-core`), and persistent file journaling (`synapsed`).

**Phase 2** elevates FeatherOS from a standalone interactive shell into a **continuous, living background operating system service**. 

```
┌────────────────────────────────────────────────────────────────────────┐
│                          CLIENT ECOSYSTEM                              │
│   feather CLI/Shell  •  feather-tui  •  Editor Extensions  •  Scripts  │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ IPC: Unix Socket / Windows Named Pipe
┌───────────────────────────────────▼────────────────────────────────────┐
│                  FEATHER DAEMON (featherd)                             │
│   • Multi-client Session & Connection Manager                          │
│   • Background Agent Orchestrator & Task Scheduler                     │
│   • JSON-RPC 2.0 / Model Context Protocol (MCP) Router                 │
├───────────────────────────────────┬────────────────────────────────────┤
│ CONTINUOUS PERCEPTION ENGINE      │ LOCAL NEURAL EMBEDDING ENGINE      │
│ • File Watcher (inotify/fsevents) │ • Dense Vector Index (Candle/ONNX) │
│ • Terminal / Process Auditor      │ • Cosine Similarity Semantic Store │
│ • Log & Anomaly Listener          │ • Zero-Latency / 100% Offline      │
├───────────────────────────────────┴────────────────────────────────────┤
│ REVERSIBLE ACTION & SAFETY GUARD (aegis-snapshot)                      │
│ • Pre-execution File Snapshot & Backup Journal                         │
│ • One-click Undo / Rollback Engine (:undo)                             │
│ • Capability Sandboxing & Multi-tier Risk Assessment                   │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
┌───────────────────────────────────▼────────────────────────────────────┐
│                       HOST SUBSTRATE LAYER                             │
│           Linux Kernel 6.x  •  macOS Darwin  •  Windows Win32          │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. The Five Pillars of Phase 2

### Pillar 1: The Feather Daemon (`featherd`) & IPC Architecture
Currently, running `feather` launches a process that terminates when exited. In Phase 2, a lightweight background daemon (`featherd`) runs continuously:

* **Background Service Integration**:
  * **Linux**: `systemd --user` unit (`~/.config/systemd/user/featherd.service`)
  * **macOS**: `launchd` agent (`~/Library/LaunchAgents/com.feather.daemon.plist`)
  * **Windows**: Windows Background Service or Startup Task
* **IPC Transport**:
  * **Unix (Linux / macOS)**: Unix Domain Socket at `~/.feather/featherd.sock`
  * **Windows**: Named Pipe at `\\.\pipe\featherd`
* **IPC Protocol**: JSON-RPC 2.0 over length-delimited streaming buffers.
* **Capabilities**:
  * Persistent LLM connection pooling & prompt caching.
  * Multi-terminal awareness: All open terminal windows share the same cognitive context.

#### IPC Protocol Schema Sample:
```json
// Request: Client -> Daemon
{
  "jsonrpc": "2.0",
  "id": "req-101",
  "method": "intent.plan_and_execute",
  "params": {
    "intent": "find large temporary files and show rollback plan",
    "working_dir": "/home/nivas/project",
    "auto_confirm": false
  }
}

// Streaming Event: Daemon -> Client
{
  "jsonrpc": "2.0",
  "method": "plan.step_progress",
  "params": {
    "step": 1,
    "total": 3,
    "status": "executing",
    "command": "du -ah . | sort -rh | head -n 10"
  }
}
```

---

### Pillar 2: Continuous Perception & Event Bus (`feather-watch`)
Traditional shells only know what happens when you press Enter. An AI-Native OS must have **continuous ambient awareness**.

* **Filesystem Event Watcher**:
  * Uses the cross-platform `notify` crate (kernel-backed via `inotify` on Linux, `kqueue/FSEvents` on macOS, and `ReadDirectoryChangesW` on Windows).
  * Automatically updates the semantic index in the background whenever files are created, modified, or deleted.
* **Terminal Error Interceptor**:
  * When any command run in the shell exits with non-zero status, `featherd` captures `stderr` and analyzes the root cause proactively.
  * Suggests automated one-click fixes (e.g., missing compiler dependency, port collision, git merge conflict).
* **System Resource Telemetry**:
  * Periodically samples CPU load, memory pressure, and available disk space.
  * Alerts user when resources drop below critical thresholds.

---

### Pillar 3: Local Neural Vector Engine (`feather-embed`)
In Phase 1, semantic search uses token relevance with recency decay. In Phase 2, we introduce **dense vector embeddings**:

* **Engine**: Pure-Rust inference using Hugging Face `candle` or `ort` (ONNX Runtime).
* **Model**: Bundled quantized embedding model (e.g. `bge-small-en-v1.5` or `all-MiniLM-L6-v2`, ~45MB).
* **Benefits**:
  * True semantic similarity: Querying `"network issues"` matches entries containing `"socket timeout"` or `"connection refused"` without needing exact word matches.
  * 100% offline and private: Embeddings never leave the machine.
  * Sub-millisecond vector cosine search across thousands of indexed code and journal entries.

---

### Pillar 4: Action Snapshot & Rollback Engine (`aegis-snapshot`)
The greatest fear when using AI agents is accidental destruction or untracked file modification. Phase 2 introduces **guaranteed reversibility**:

* **Pre-Execution Snapshot**:
  * Before any agent step executes a file write or deletion, `aegis-snapshot` records the original content and permissions in `~/.feather/snapshots/<snapshot_id>`.
* **The `:undo` Command**:
  * Entering `:undo` or `feather rollback` immediately restores all files touched in the previous agent action.
* **Unified Visual Diff Inspection**:
  * Displays color-coded terminal diffs before applying changes, allowing the user to accept, reject, or edit specific hunks.

---

### Pillar 5: Rich TUI Dashboard & Split-Screen Interface (`feather-tui`)
An interactive terminal dashboard built with `ratatui` and `crossterm`:

```
┌──────────────────────────────────────┬──────────────────────────────────────┐
│ FEATHER SHELL (Active Session)       │ COGNITIVE INSPECTOR & AGENT MONITOR  │
├──────────────────────────────────────┼──────────────────────────────────────┤
│ feather:~/feather-os (main)*>        │ 🧠 Model: Llama 3.2 3B (Local/Fast)  │
│ $ check disk space and clean build   │ 🛡️ Sandbox: UserNamespace (Guarded)   │
│                                      │                                      │
│ [STEP 1] (SAFE) df -h .              │ 📋 Active Plan:                      │
│ Filesystem  Size  Used  Avail  Use%  │   [✔] 1. Inspect disk usage          │
│ /dev/sdd   1007G  5.0G   951G    1%  │   [▶] 2. Find target directories     │
│                                      │   [ ] 3. Clean stale build cache     │
│ [STEP 2] (SAFE) du -sh target/       │                                      │
│ 611M  target/                        │ 📊 System Telemetry:                 │
│                                      │   CPU: [||||||....] 38%              │
│ Clean target folder? [Y/n]: _        │   RAM: 6998 MB Free | Disk: 950 GB   │
└──────────────────────────────────────┴──────────────────────────────────────┘
│ [F1] Help | [F2] Memory Search | [F3] Tools | [F4] Daemon Logs | [F12] Split │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Implementation Roadmap

| Milestone | Deliverables | Target |
| :--- | :--- | :--- |
| **Milestone 2.1: The Daemon & IPC** | `crates/featherd` + IPC Client | Background daemon with Unix socket / Windows pipe + JSON-RPC protocol. |
| **Milestone 2.2: Continuous Perception** | `crates/feather-watch` | Cross-platform filesystem watcher (`notify`) + auto-indexer daemon. |
| **Milestone 2.3: Reversible Snapshots** | `aegis-snapshot` + `:undo` | File backup journal before mutations + instant rollback command. |
| **Milestone 2.4: Neural Vector Memory** | `synapsed` v2 (Candle/ONNX) | Local dense embedding inference + cosine similarity vector index. |
| **Milestone 2.5: Interactive TUI** | `crates/feather-tui` | Ratatui dual-pane visual shell and cognitive monitor. |
