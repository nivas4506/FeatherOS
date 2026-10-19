# FeatherOS Architecture Changes Documentation

This document records all changes made to the FeatherOS architecture, the current layered structure, each subsystem, and the build and runtime model. It contains no emoji characters.

## 1. Summary of Architecture Changes

The FeatherOS architecture was reorganized from a flat crate layout into a formal four-tier layered operating system substrate. The primary change commit ("Architecture changes") introduced the following:

1. Directory reorganization: source code moved from the top-level `crates/` directory into a tiered layout under `os/` (platform-independent OS layer) and `apps/` (application layer).
2. Addition of the Hardware Layer: architecture profiles, board support package device trees, memory map, and hardware requirements.
3. Addition of the Linux Kernel Base Layer: hardened Linux 6.x defconfig, kernel command line, bootloader configs, early userspace init, in-kernel eBPF probe, and build/install/boot-image scripts.
4. Addition of the Hardware and Kernel Abstraction Layer (`feather-hal`) with platform-independent traits.
5. Addition of OS services: supervisor daemon, semantic memory journal, ambient perception engine, and IPC transport.
6. Addition of the safety subsystem: multi-tier risk engine, execution sandbox, and atomic rollback engine.
7. Addition of the intelligence subsystem: cognitive planner, telemetry aggregator, intent classifier, MCP tool registry, and offline vector embedding engine.
8. Addition of the application layer: dual-mode cognitive shell, split-pane terminal TUI, desktop experience, and user-space tools.
9. Adoption of a dual build system: Cargo workspace for Rust application crates and CMake for C++ OS services and the bare-metal kernel.
10. Addition of Visual Studio Code configuration for C/C++ and Rust development.

## 2. Four-Tier Layered Architecture

FeatherOS is structured into four clean layers. Each layer depends only on the layer directly beneath it.

```
+-----------------------------------------------------------------------------+
|                         LAYER 4: APPLICATION LAYER                          |
|      (apps/shell/aish  •  apps/tui/feather-tui  •  apps/desktop  •  apps/) |
|  • Dual-Mode Cognitive Shell (Instant POSIX REPL + Natural Language)        |
|  • Interactive Split-Pane Visual Terminal Dashboard (ratatui)               |
|  • Desktop Experience (Electron + React + TypeScript)                       |
|  • User-Space CLI Tools and MCP Agent Tool Extensions                       |
+-----------------------------------------------------------------------------+
                                    |
                         JSON-RPC 2.0 / IPC (Unix Socket / Pipe)
                                    |
+-----------------------------------------------------------------------------+
|                  LAYER 3: PLATFORM-INDEPENDENT OS LAYER (FeatherOS)         |
|                                                                             |
|  +---------------------------+  +-----------------------------------------+  |
|  |   OS SERVICES & DAEMONS   |  |        SECURITY & SAFETY                |  |
|  | • featherd (Supervisor)   |  | • aegis-exec (Sandbox Coordinator)      |  |
|  | • synapsed (Mem Journal)  |  | • aegis-snapshot (Atomic Rollback)      |  |
|  | • feather-watch (Event)   |  +-----------------------------------------+  |
|  | • feather-ipc (Transport) |  |      INTELLIGENCE & COGNITION           |  |
|  +---------------------------+  | • cortex-core (Planner/Telemetry)       |  |
|                                 | • agent-bus (MCP Tool Registry)         |  |
|                                 | • feather-embed (Vector Embeddings)     |  |
|                                 +-----------------------------------------+  |
|  +-------------------------------------------------------------------------+  |
|  |   feather-hal: Hardware & Kernel Abstraction Layer (HAL)                |  |
|  |   ProcessManager • VirtualFileSystem • SystemTelemetry • Sandbox        |  |
|  +-------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------+
                                    |
                            Syscalls / cgroups v2 / eBPF / /init
                                    |
+-----------------------------------------------------------------------------+
|                    LAYER 2: KERNEL LAYER (Linux 6.x Base)                   |
|      (kernel/config  •  kernel/boot  •  kernel/init  •  kernel/ebpf)        |
|  • Custom Hardened Linux 6.x Base Kernel (feather_defconfig)                |
|  • Early Userspace Init Bootstrap (/init) & inittab Runlevel                |
|  • In-Kernel eBPF Continuous Syscall Monitor (feather_probe.bpf.c)          |
|  • cgroups v2, User Namespaces, Seccomp-BPF & VirtIO Drivers                |
+-----------------------------------------------------------------------------+
                                    |
                        Hardware Control / ACPI / Device Tree
                                    |
+-----------------------------------------------------------------------------+
|                         LAYER 1: HARDWARE LAYER                             |
|      (hardware/arch  •  hardware/bsp  •  hardware/specs)                    |
|  • x86_64 • AArch64 (ARM64) • RISC-V 64 Architecture Profiles               |
|  • Board Support Packages (BSP) & Device Trees (PC, QEMU, RPi4)             |
|  • Physical/Virtual Memory Map, ACPI/UEFI & Hardware Specs                  |
+-----------------------------------------------------------------------------+
```

## 3. Layer Directory Structure

| Layer | Directory | Description |
| :--- | :--- | :--- |
| Layer 1: Hardware | `hardware/` | Architecture specs (`arch/x86_64.json`, `arch/aarch64.json`, `arch/riscv64.json`), device trees (`bsp/generic-pc.dts`, `bsp/qemu-virt.dts`, `bsp/rpi4.dts`), and memory map + hardware requirements (`specs/`) |
| Layer 2: Kernel | `kernel/` | Hardened Linux 6.x kernel config (`config/feather_defconfig`), kernel command line (`config/kernel-cmdline.cfg`), bootloader configs (`boot/grub.cfg`, `boot/systemd-boot.conf`), early userspace bootstrap (`init/init`, `init/inittab`), in-kernel eBPF probe (`ebpf/feather_probe.bpf.c`), and build/install/boot-image scripts (`scripts/`) |
| Layer 3: OS Layer | `os/` | Platform-independent OS files: HAL (`core/feather-hal`), supervisor daemon (`services/featherd`), semantic memory (`services/synapsed`), ambient perception (`services/feather-watch`), IPC transport (`services/feather-ipc`), safety execution & sandbox (`safety/aegis-exec`), rollback engine (`safety/aegis-snapshot`), cognitive engine (`intelligence/cortex-core`), MCP bus (`intelligence/agent-bus`), embeddings (`intelligence/feather-embed`) |
| Layer 4: Application | `apps/` | Dual-mode cognitive shell (`shell/aish`), interactive terminal dashboard (`tui/feather-tui`), desktop experience (`desktop`), and system tools (`tools`) |

## 4. Hardware Layer (Layer 1)

The hardware layer defines architecture profiles, board support packages, and physical specifications.

### Architecture Profiles (`hardware/arch/`)
- `x86_64.json`: x86-64 architecture profile
- `aarch64.json`: ARM64 architecture profile
- `riscv64.json`: RISC-V 64 architecture profile

Each profile describes the supported CPU family, memory layout, boot method, and hardware capabilities.

### Board Support Packages (`hardware/bsp/`)
- `generic-pc.dts`: Generic PC device tree for UEFI/BIOS boot
- `qemu-virt.dts`: QEMU virtual machine device tree
- `rpi4.dts`: Raspberry Pi 4 device tree

### Specifications (`hardware/specs/`)
- `memory-map.md`: Physical and virtual memory map layout
- `hardware-requirements.md`: Minimum and recommended hardware requirements

## 5. Kernel Layer (Layer 2)

The Linux kernel base layer boots directly on hardware or a hypervisor and provides low-level execution primitives, drivers, memory management, and security boundaries.

### Files and Scripts
- `config/feather_defconfig`: Stripped, high-security Linux 6.x kernel configuration tuned for sub-second boot times
- `config/kernel-cmdline.cfg`: Recommended kernel command-line parameters
- `boot/grub.cfg`: GRUB bootloader configuration for UEFI/BIOS boots
- `boot/systemd-boot.conf`: Lightweight systemd-boot loader entry
- `init/init`: Early userspace bootstrap init executable that configures virtual filesystems and launches `featherd`
- `init/inittab`: Inittab runlevel configuration
- `ebpf/feather_probe.bpf.c`: In-kernel eBPF program monitoring process lifecycle and file modifications
- `scripts/build-kernel.sh`: Builds the Linux kernel from upstream source with `feather_defconfig`
- `scripts/install-kernel.sh`: Installs the Linux kernel base layer and creates boot entries
- `scripts/make-boot-image.sh`: Packages kernel, initramfs, and FeatherOS binaries into a bootable QEMU/hardware disk image

### Responsibilities
1. Hardware interfacing: initializes CPU cores, physical memory maps, interrupts, storage controllers (NVMe, SATA, VirtIO), and network interfaces
2. Process and namespace isolation: provides kernel-level primitives used by `aegis-exec` (User Namespaces, PID Namespaces, Mount Namespaces, Network Namespaces)
3. Resource guardrails: enforces cgroups v2 memory limits, CPU quotas, and I/O throttling for cognitive agent tasks
4. Syscall boundary and safety: provides `seccomp-bpf` filters that trap dangerous kernel syscalls
5. Continuous kernel perception: eBPF probes feed real-time telemetry into `feather-watch`
6. Bootstrap to OS layer: the early userspace init system configures virtual filesystems and launches `featherd`

### Bare-Metal Kernel (C++)
A separate bare-metal x86_64 kernel is maintained under `kernel/arch/x86_64/` with CMake build files. It includes:
- Boot entry (`entry.asm`, `boot.asm`), GDT/IDT setup, interrupt handling, context switching
- CPU, task, memory management (paging, frame allocator, heap), VFS, drivers (VGA, keyboard, serial, PIT)
- Syscalls, shell, panic handling, and linker script
- Build configuration under `kernel/build/`

## 6. OS Layer (Layer 3)

The OS layer is the platform-independent FeatherOS substrate. It relies on `feather-hal` rather than direct host-specific assumptions.

### 6.1 Hardware and Kernel Abstraction (`os/core/feather-hal/`)
Defines platform-independent traits for:
- `ProcessManager`: spawning, signaling, and waiting on processes
- `VirtualFileSystem`: atomic file writes, path resolution, directory traversal
- `SystemTelemetryProvider`: cross-platform sampling of CPU, memory, and kernel metrics
- `SandboxDriver`: pluggable sandboxing implementations (Linux Namespaces, Bubblewrap, Seccomp, and Fallback)

Files: `traits.rs`, `process.rs`, `fs.rs`, `sandbox.rs`, `telemetry.rs`, `lib.rs`

### 6.2 OS Services and Daemons (`os/services/`)
- `featherd`: master OS supervisor daemon. Manages multi-client sessions, dispatches tasks, and routes JSON-RPC 2.0 requests
- `synapsed`: semantic memory journal and persistent workspace indexing engine
- `feather-watch`: ambient event perception engine watching filesystem changes and hardware pressure
- `feather-ipc`: inter-process communication framing, protocol codecs, and connection management

### 6.3 Safety and Integrity Subsystem (`os/safety/`)
- `aegis-exec`: multi-tier regex safety engine (Safe, Moderate, High, Critical) and execution sandbox
- `aegis-snapshot`: pre-execution file snapshotting and one-click atomic rollback (`:undo`)

### 6.4 Intelligence and Cognitive Subsystem (`os/intelligence/`)
- `cortex-core`: cognitive planner, telemetry aggregator, and intent classifier (POSIX vs Natural Language)
- `agent-bus`: Model Context Protocol (MCP) tool registry exposing system actions to AI agents
- `feather-embed`: offline 256-dimensional neural vector embedding engine

### Risk Tiers (aegis-exec)
| Tier | Behavior | Example |
| :--- | :--- | :--- |
| Safe | Auto-executed | `ls`, `cat`, `git status` |
| Moderate | Non-destructive writes | `touch`, `mkdir` |
| High Risk | Mutative operations requiring explicit confirmation | file edits, deletes |
| Critical | Automatically blocked | `rm -rf /`, `mkfs`, format commands |

## 7. Application Layer (Layer 4)

The application layer sits on top of the OS layer and provides human and agent interfaces. Applications communicate with `featherd` via JSON-RPC 2.0 over IPC.

### Applications
- `apps/shell/aish`: dual-mode cognitive shell binary `feather` / `aish`. Supports standard POSIX commands and natural language intents, proactive step verification, and rollback preview
- `apps/tui/feather-tui`: binary `feather-tui`. Built with `ratatui` and `crossterm`. Left pane: interactive terminal session. Right pane: cognitive inspector showing plan steps, vector memory search, telemetry gauges, and sandbox indicators
- `apps/desktop`: desktop experience built with Electron + React + TypeScript. Modern dashboard with telemetry widgets, command palette, and mock-first UI connectable to `featherd`
- `apps/tools`: user-space CLI helpers, scripts, and MCP tool plugins for specialized domain workflows

### Shell Commands
- `:status` — system telemetry, kernel version, and sandbox capabilities
- `:tools` — list registered Agent Bus (MCP) tools
- `:memory <query>` — query persistent semantic memory journal
- `:index` — re-index current workspace files
- `:undo` — rollback previous file mutation safely
- `:exit` / `:quit` — exit shell

## 8. Build System

FeatherOS uses a dual build system.

### Cargo Workspace (Rust)
Root `Cargo.toml` defines the workspace members:
- OS layer: `os/core/feather-hal`, `os/services/featherd`, `os/services/feather-watch`, `os/services/feather-ipc`, `os/safety/aegis-exec`, `os/safety/aegis-snapshot`
- Application layer: `apps/shell/aish`, `apps/tui/feather-tui`

Command:
```
cargo build --release
```

Binaries produced:
- `target/release/featherd` — OS Layer Supervisor Daemon
- `target/release/feather` / `aish` — Application Layer Cognitive Shell
- `target/release/feather-tui` — Application Layer Terminal Dashboard

### CMake Builds (C++)
CMake is used for the bare-metal kernel and C++ OS service implementations:
- `kernel/CMakeLists.txt` — bare-metal x86_64 kernel build
- `os/CMakeLists.txt` — OS layer services and HAL
- Per-component `CMakeLists.txt` files under `os/core/feather-hal/`, `os/services/`, `os/safety/`

### Linux Kernel Build
```
./kernel/scripts/build-kernel.sh
sudo ./kernel/scripts/install-kernel.sh
sudo ./kernel/scripts/make-boot-image.sh
qemu-system-x86_64 -m 1G -hda featheros-live.img -nographic
```

## 9. Runtime Operation

### Start the Background OS Daemon
```
featherd
```

### Interactive Cognitive Shell
```
feather
```

### Dual-Pane TUI Monitor
```
feather-tui
```

### IPC Protocol
Clients and the daemon communicate via JSON-RPC 2.0 over length-delimited streaming buffers:
- Linux/macOS: Unix Domain Socket at `~/.feather/featherd.sock`
- Windows: Named Pipe at `\\.\pipe\featherd`

## 10. Version Control Notes

Relevant commits:
- `75b88c9` — Initial commit: FeatherOS cross-platform AI-native operating system substrate
- `ddcb...` / `6aafc7d` / `5cb481e` — Phase 2 work and blueprint
- `03bdb4d` — Architecture changes: directory reorganization, hardware layer, kernel layer, HAL, OS services, safety, intelligence, application layer, dual build system, VS Code config
- `ae46596` — Kernel develop: bare-metal C++ kernel, C++ OS service implementations, desktop application, build artifacts

## 11. License

MIT License. See LICENSE for details.
