# FeatherOS Phase 2 — Windows Release Status

## ✅ Implemented (All 5 Pillars)

| Pillar | Crate | Status |
|---|---|---|
| **Daemon + IPC** | `feather-ipc`, `featherd` | JSON-RPC over Unix socket (Linux/macOS) / Named pipe (Windows); shared sessions, single-use plan approvals, serialized execution |
| **Continuous Perception** | `feather-watch` | Recursive filesystem watcher (notify), 5s telemetry sampling, auto-indexing, stderr diagnostics |
| **Reversible Snapshots** | `aegis-snapshot` | Pre-execution capture, `:undo`/`:diff`/`:snapshots`, conflict preflight, 10k entries / 128 MiB / 16 MiB per file |
| **Vector Memory** | `feather-embed` + `synapsed` | 256-dim offline vectors (32 concept dims + hashed lexical); paraphrase match without shared tokens (`network issues` ↔ `socket timeout`) |
| **TUI** | `feather-tui` | Dual-pane shell + cognitive inspector with status, active plan, events, snapshot IDs |

## 🛠️ Cross-Compilation Architecture (Windows from Linux)

### Toolchain
- **Target**: `x86_64-pc-windows-gnu` (MinGW-w64 via zig)
- **C compiler**: `zig-cc-win` (translates `--target=` for zig)
- **Link driver**: `zig-link-win` (cargo-zigbuild style, maps `-lwinapi_X` → `-lX`, drops `compiler_builtins` in favor of zig's `compiler_rt`)
- **Import lib farm**: `scripts/setup-winlibs.sh` → `~/.local/winlibs/` (winapi, windows, windows_x86_64_gnu `.a` files)

### Build Command
```bash
# From repo root (Linux WSL)
./scripts/build-windows.sh
# Output: target/x86_64-pc-windows-gnu/release/featherd.exe feather.exe aish.exe feather-tui.exe
```

## 📦 Windows Binaries (Latest)

| Binary | Size | Purpose |
|---|---|---|
| `featherd.exe` | 10.2 MB | Background daemon (named pipe `\\.\pipe\featherd`) |
| `feather.exe` | 8.7 MB | CLI shell client |
| `aish.exe` | 8.7 MB | Alias of feather |
| `feather-tui.exe` | 3.2 MB | Dual-pane dashboard |

## 🧪 Smoke Test Results

### ✅ Working
- **Daemon startup**: `featherd.exe --workspace C:\path\work` — listens on `\\.\pipe\featherd`
- **Daemon status**: `feather.exe status` → returns JSON with model, telemetry, sessions
- **Embeddings**: 256-dim offline vectors, paraphrase match verified in tests

### ⚠️ Known Issues
1. **Client hang on natural-language commands**: `feather -c "check disk space"` waits for confirmation; use `-y` flag (`feather -y -c "..."`)
2. **Named pipe connect timeout**: After ~5s idle, `featherd` may reject new connections (likely event-loop / spawn_blocking exhaustion under single-threaded Tokio)
3. **TUI untested on Windows**: `feather-tui.exe` builds but not yet run in Windows console

### Workarounds
```powershell
# Use auto-confirm for non-interactive commands
$env:FEATHER_HOME = "C:\Temp\feather-home"
$env:FEATHER_ENDPOINT = "\\.\pipe\featherd"

# Start daemon
Start-Process featherd.exe -ArgumentList "--workspace", "C:\my-project"

# Run commands with -y
feather.exe -y -c "check disk space and find largest folders"

# Built-in commands (no confirmation)
feather.exe status
feather.exe :memory "network issues"
feather.exe :undo
```

## 📁 Repository Changes (Uncommitted)

### New Crates
```
crates/feather-ipc/       # JSON-RPC transport (length-delimited frames)
crates/featherd/          # Background daemon (sessions, planner, executor, watcher, snapshots)
crates/feather-watch/     # Filesystem watcher + telemetry sampler + diagnostics
crates/aegis-snapshot/    # Workspace snapshots with rollback
crates/feather-embed/     # Offline 256-dim embedding engine
crates/feather-tui/       # Ratatui dual-pane dashboard
```

### Modified Crates
```
crates/aish/              # Now a JSON-RPC client (feather binary)
crates/synapsed/          # Hybrid keyword + vector search; vector cache
crates/aegis-exec/        # Bounded output, process group kill on drop
crates/cortex-core/       # Telemetry capture-in(dir) for multi-client daemon
crates/agent-bus/         # Unchanged (MCP tool bus)
```

### Build Scripts (Linux → Windows)
```
scripts/build-windows.sh       # Main cross-compile entry
scripts/setup-winlibs.sh       # Import-lib farm
scripts/finalize-windows.sh    # Promote deps/*.exe to release names
scripts/zig-cc-win             # C compiler wrapper for zig
scripts/zig-link-win           # Link driver (cargo-zigbuild style)
```

### Documentation
```
docs/PHASE2_SETUP.md              # Quickstart, service templates, limits
docs/featherd.service.example     # systemd user unit
docs/com.feather.daemon.plist.example  # macOS launchd
docs/PHASE2_WINDOWS_RELEASE.md    # This file
```

## 🔬 Test Suite
```bash
cargo test --workspace
# 15 tests: IPC framing, sessions/plans, watcher, snapshots, embeddings, semantic RPC
```

## 🎯 Next Steps for Windows

1. **Fix daemon event-loop** — diagnose single-threaded accept/connection handling under load
2. **Add `-y` default for Windows** — auto-confirm non-interactive commands
3. **Test TUI in Windows Terminal** — verify crossterm backend
4. **Package release** — zip binaries + `docs/PHASE2_SETUP.md` for distribution
5. **Native MSVC build** — optional, for better Windows integration (requires MSVC toolchain)

## 📋 Quick Start (Windows User)

```powershell
# 1. Unzip release
Expand-Archive featheros-windows.zip -DestinationPath C:\featheros
cd C:\featheros

# 2. Configure
$env:FEATHER_HOME = "C:\Users\$env:USERNAME\.feather"
$env:FEATHER_ENDPOINT = "\\.\pipe\featherd"

# 3. Start daemon (one-time or scheduled task)
Start-Process featherd.exe -ArgumentList "--workspace", (Get-Location)

# 4. Use shell
feather.exe -y -c "check disk space"
feather.exe status
feather.exe :memory "network issues"

# 5. Try TUI
feather-tui.exe
```

---

**Built**: 2026-09-11  
**Rust**: 1.98.1  
**zig**: 0.13.0  
**MinGW-w64**: ucrt64 16.1.0