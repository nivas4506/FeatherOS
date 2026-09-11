<div align="center">

# 🪶 FeatherOS

**Cross-Platform, AI-Native Operating System Substrate & Cognitive Shell**

[![Rust](https://img.shields.io/badge/rust-1.80%2B-orange.svg)](https://www.rust-lang.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform: Linux | macOS | Windows](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-green.svg)](#cross-platform-support)

*FeatherOS bridges deterministic POSIX/PowerShell execution with autonomous cognitive planning, semantic memory, Model Context Protocol (MCP) tooling, and proactive sandboxed safety.*

</div>

---

## 🧭 Architecture

Traditional operating systems treat **"Everything as a byte stream or file"**.  
**FeatherOS** evolves this into: **"Everything is Context, Intent, and Observable Capability."**

```
┌────────────────────────────────────────────────────────────────────────┐
│                   USER INTERACTION LAYER (feather / aish)              │
│   • Dual-Mode REPL (Seamless POSIX shell + Natural Language Planner)   │
│   • Interactive Step Confirmation & Unified Diff Previews              │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ IPC / In-Process MCP Bus
┌───────────────────────────────────▼────────────────────────────────────┐
│                    INTENT & TOOL BUS (agent-bus)                       │
│   • Model Context Protocol (MCP) Tool Registry & Semantic Router       │
│   • Fine-grained Policy Guard & System Action Dispatcher               │
└──────────────┬────────────────────┬────────────────────┬───────────────┘
               │                    │                    │
┌──────────────▼──────┐ ┌───────────▼──────────┐ ┌───────▼──────────────┐
│  COGNITIVE ENGINE   │ │   SEMANTIC JOURNAL   │ │  SANDBOXED EXECUTOR  │
│     (cortex-core)   │ │      (synapsed)      │ │     (aegis-exec)     │
│ • Local SLM & Embed │ │ • File Indexer       │ │ • Linux Namespaces   │
│ • Context Compactor │ │ • Shell & Log Index  │ │ • macOS Seatbelt     │
│ • Cloud/Ollama      │ │ • Vector Sim Search  │ │ • Windows Jobs       │
└─────────────────────┘ └──────────────────────┘ └──────────────────────┘
                                    │
┌───────────────────────────────────▼────────────────────────────────────┐
│                       HOST SUBSTRATE LAYER                             │
│       Linux (Kernel 6.x)   •   macOS (Darwin)   •   Windows (Win32)    │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 📦 Workspace Crates

| Crate | Purpose | Key Features |
| :--- | :--- | :--- |
| [`crates/aegis-exec`](./crates/aegis-exec) | **Sandboxed Executor & Safety Guard** | Multi-tier regex safety engine (`Safe`, `Moderate`, `High`, `Critical`), blocks destructive commands (`rm -rf /`, `del /s /q C:\`, fork bombs). Platform sandbox abstraction (Linux Namespaces, macOS Seatbelt, Windows Jobs). |
| [`crates/cortex-core`](./crates/cortex-core) | **Cognitive Engine & Planner** | OS-aware telemetry (CPU, RAM, kernel, git status), hybrid intent classifier (POSIX vs Natural Language), pluggable backends (Offline Heuristics, Local Ollama, OpenAI-compatible). |
| [`crates/agent-bus`](./crates/agent-bus) | **Intent & Tool Bus (MCP)** | Model Context Protocol compliant tool registry exposing system actions (`fs_find`, `fs_read`, `proc_list`) with JSON-schema descriptors. |
| [`crates/synapsed`](./crates/synapsed) | **Semantic Memory Journal** | Disk-persisted memory (`~/.feather/journal.json`), recursive workspace file indexer, recency-decay relevance search. |
| [`crates/aish`](./crates/aish) | **Cognitive Shell Binary** | Dual-mode REPL with dynamic prompt telemetry, command execution, and step verification. |

---

## 🌐 Cross-Platform Support

FeatherOS is written in 100% pure Rust and runs natively across:
* **Linux**: Spawns `$SHELL` or `/bin/sh`, uses Linux User Namespaces and Bubblewrap.
* **macOS (Darwin)**: Spawns `/bin/zsh` or `$SHELL`, integrates with `sw_vers` and `sysctl`.
* **Windows**: Spawns `powershell.exe`, protects against Windows-specific destructive commands (`Format-Volume`, `del /s /q C:\`).

---

## 🚀 Quickstart

### 1. Prerequisites
Ensure you have the Rust toolchain installed:
```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

### 2. Clone & Build
```bash
git clone https://github.com/nivas4506/FeatherOS.git
cd FeatherOS
cargo build --release
```

Install the binary to your local path:
```bash
# Linux / macOS
cp target/release/aish ~/.local/bin/feather

# Windows (PowerShell)
Copy-Item target\release\aish.exe $HOME\.cargo\bin\feather.exe
```

### 3. Running FeatherOS

#### Interactive Mode:
```bash
feather
```

#### Inside the Shell:
* **Direct Execution**: Run standard commands with native performance:
  ```bash
  feather:~/my-project (main)*> ls -la
  feather:~/my-project (main)*> git status
  ```
* **Natural Language Intent**: Ask FeatherOS to perform tasks:
  ```bash
  feather:~/my-project (main)*> check disk space and find largest folders
  feather:~/my-project (main)*> find all rust files modified today
  ```
* **Built-in System Commands**:
  - `:status` - View system telemetry & sandbox capabilities
  - `:tools` - List registered Agent Bus (MCP) tools
  - `:memory <query>` - Query persistent semantic memory
  - `:index` - Re-index current workspace files into semantic memory
  - `:help` - View usage reference
  - `:exit` / `:quit` - Exit shell

#### Run with Local Ollama SLM:
```bash
feather --provider ollama --model llama3.2:3b
```

#### Single-Command Mode:
```bash
feather -c "check disk space and memory usage"
```

---

## 🛡️ Aegis Safety Guard

FeatherOS protects your system from catastrophic agent hallucination. Destructive actions are classified into 4 risk tiers:

```
[SAFE]       -> Auto-executed without prompts (e.g., ls, cat, git status)
[MODERATE]   -> Non-destructive writes (e.g., touch, mkdir)
[HIGH RISK]  -> Mutative operations requiring explicit user confirmation
[CRITICAL]   -> Automatically blocked (e.g., rm -rf /, del /s /q C:\, mkfs)
```

Example:
```bash
$ feather -c "rm -rf /"
Execution blocked by Aegis Safety Guard:
  • CRITICAL: Recursive deletion of root or home directory
```

---

## 📄 License

MIT License. See [LICENSE](LICENSE) for details.
