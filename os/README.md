# OS Layer: Platform-Independent FeatherOS Substrate (Layer 3)

The **OS Layer** represents the core operating system substrate of FeatherOS. It bridges the low-level Linux Kernel Base Layer (Hardware + Kernel) with user-space applications (Cognitive Shell, TUI Monitor, MCP Agents).

All services, daemons, and safety subsystems within this layer are designed to be **100% platform-independent**, relying on the Hardware & Kernel Abstraction Layer (`feather-hal`) rather than direct host-specific assumptions.

```
┌────────────────────────────────────────────────────────────────────────┐
│                      APPLICATION LAYER (Layer 4)                       │
│    apps/shell/aish (feather CLI)   •   apps/tui/feather-tui            │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ IPC: JSON-RPC 2.0 / Streaming Frames
┌───────────────────────────────────▼────────────────────────────────────┐
│                         OS LAYER (Layer 3)                             │
│                                                                        │
│  ┌─────────────────────────┐  ┌─────────────────────────────────────┐  │
│  │   SERVICES & DAEMONS    │  │        SECURITY & SAFETY            │  │
│  │ • featherd (Supervisor) │  │ • aegis-exec (Sandbox Coordinator)  │  │
│  │ • synapsed (Mem Journal)│  │ • aegis-snapshot (Atomic Rollback)  │  │
│  │ • feather-watch (Event) │  └─────────────────────────────────────┘  │
│  │ • feather-ipc (Transport│  ┌─────────────────────────────────────┐  │
│  └─────────────────────────┘  │      INTELLIGENCE & COGNITION       │  │
│                               │ • cortex-core (Planner/Telemetry)   │  │
│                               │ • agent-bus (MCP Tool Registry)     │  │
│                               │ • feather-embed (Vector Embeddings) │  │
│                               └─────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │     feather-hal: Hardware & Kernel Abstraction Layer (HAL)       │  │
│  │     ProcessManager • VirtualFileSystem • SystemTelemetry • Sbx  │  │
│  └──────────────────────────────────────────────────────────────────┘  │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Syscalls / cgroups / eBPF / /init
┌───────────────────────────────────▼────────────────────────────────────┐
│                 KERNEL LAYER (Linux 6.x) (Layer 2)                     │
│    (Custom Defconfig, Initramfs, eBPF, Drivers, Boot)                  │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
┌───────────────────────────────────▼────────────────────────────────────┐
│                     HARDWARE LAYER (Layer 1)                           │
│        (x86_64 • ARM64 • RISC-V • Bare-Metal Silicon & QEMU)          │
└────────────────────────────────────────────────────────────────────────┘
```

## Subsystems & Directory Structure

### 1. Hardware & Kernel Abstraction (`os/core/`)
- [`os/core/feather-hal`](./core/feather-hal): Defines platform-independent traits for:
  - `ProcessManager`: Spawning, signaling, and waiting on processes.
  - `VirtualFileSystem`: Atomic file writes, path resolution, directory traversal.
  - `SystemTelemetryProvider`: Cross-platform sampling of CPU, memory, and kernel metrics.
  - `SandboxDriver`: Pluggable sandboxing implementations (Linux Namespaces, Bubblewrap, Seccomp, and Fallback).

### 2. OS Services & Daemons (`os/services/`)
- [`os/services/featherd`](./services/featherd): The master OS supervisor daemon. Manages multi-client sessions, dispatches tasks, and routes JSON-RPC 2.0 requests.
- [`os/services/synapsed`](./services/synapsed): Semantic memory journal and persistent workspace indexing engine.
- [`os/services/feather-watch`](./services/feather-watch): Ambient event perception engine watching filesystem changes and hardware pressure.
- [`os/services/feather-ipc`](./services/feather-ipc): Inter-Process Communication framing, protocol codecs, and connection management.

### 3. OS Safety & Integrity Subsystem (`os/safety/`)
- [`os/safety/aegis-exec`](./safety/aegis-exec): Multi-tier regex safety engine (`Safe`, `Moderate`, `High`, `Critical`) and execution sandbox.
- [`os/safety/aegis-snapshot`](./safety/aegis-snapshot): Pre-execution file snapshotting and one-click atomic rollback (`:undo`).

### 4. OS Intelligence & Cognitive Subsystem (`os/intelligence/`)
- [`os/intelligence/cortex-core`](./intelligence/cortex-core): Cognitive planner, telemetry aggregator, and intent classifier (POSIX vs Natural Language).
- [`os/intelligence/agent-bus`](./intelligence/agent-bus): Model Context Protocol (MCP) tool registry exposing system actions to AI agents.
- [`os/intelligence/feather-embed`](./intelligence/feather-embed): Offline 256-dimensional neural vector embedding engine.
