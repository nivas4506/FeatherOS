<div align="center">

# 🪶 FeatherOS

**AI-Native Operating System Substrate & Cognitive Shell**

[![Rust](https://img.shields.io/badge/rust-1.80%2B-orange.svg)](https://www.rust-lang.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Architecture: Hardware->Linux->OS->Apps](https://img.shields.io/badge/architecture-4--Tier%20Layered-blueviolet.svg)](#-system-architecture)

*FeatherOS bridges bare-metal hardware and a hardened Linux kernel base layer with an AI-native operating system substrate, autonomous cognitive planning, semantic memory, Model Context Protocol (MCP) tooling, and proactive sandboxed safety.*

</div>

---

## 🧭 System Architecture

FeatherOS is structured into a clean **4-tier layered architecture**:

```
╔════════════════════════════════════════════════════════════════════════╗
║                     LAYER 4: APPLICATION LAYER                         ║
║      (apps/shell/aish • apps/tui/feather-tui • apps/tools)             ║
║  • Dual-Mode Cognitive Shell (Instant POSIX REPL + Natural Language)   ║
║  • Interactive Split-Pane Visual Terminal Dashboard (ratatui)          ║
║  • User-Space CLI Tools & Custom MCP Agent Tool Extensions             ║
╚═══════════════════════════════════╦════════════════════════════════════╝
                                    ║ JSON-RPC 2.0 / IPC (Unix Socket / Pipe)
╔═══════════════════════════════════▼════════════════════════════════════╗
║           LAYER 3: PLATFORM-INDEPENDENT OS LAYER (FeatherOS)           ║
║                                                                        ║
║  ┌─────────────────────────┐  ┌─────────────────────────────────────┐  ║
║  │   OS SERVICES & DAEMONS │  │        SECURITY & SAFETY            │  ║
║  │ • featherd (Supervisor) │  │ • aegis-exec (Sandbox Coordinator)  │  ║
║  │ • synapsed (Mem Journal)│  │ • aegis-snapshot (Atomic Rollback)  │  ║
║  │ • feather-watch (Event) │  └─────────────────────────────────────┘  ║
║  │ • feather-ipc (Transport│  ┌─────────────────────────────────────┐  ║
║  └─────────────────────────┘  │      INTELLIGENCE & COGNITION       │  ║
║                               │ • cortex-core (Planner/Telemetry)   │  ║
║                               │ • agent-bus (MCP Tool Registry)     │  ║
║                               │ • feather-embed (Vector Embeddings) │  ║
║                               └─────────────────────────────────────┘  ║
║  ┌──────────────────────────────────────────────────────────────────┐  ║
║  │   feather-hal: Hardware & Kernel Abstraction Layer (HAL)         │  ║
║  │   ProcessManager • VirtualFileSystem • SystemTelemetry • Sbx    │  ║
║  └──────────────────────────────────────────────────────────────────┘  ║
╚═══════════════════════════════════╦════════════════════════════════════╝
                                    ║ Syscalls / cgroups v2 / eBPF / /init
╔═══════════════════════════════════▼════════════════════════════════════╗
║                   LAYER 2: KERNEL LAYER (Linux 6.x)                    ║
║      (kernel/config • kernel/boot • kernel/init • kernel/ebpf)         ║
║  • Custom Hardened Linux 6.x Base Kernel (feather_defconfig)           ║
║  • Early Userspace Init Bootstrap (/init) & inittab Runlevel           ║
║  • In-Kernel eBPF Continuous Syscall Monitor (feather_probe.bpf.c)     ║
║  • cgroups v2, User Namespaces, Seccomp-BPF & VirtIO Drivers           ║
╚═══════════════════════════════════╦════════════════════════════════════╝
                                    ║ Hardware Control / ACPI / Device Tree
╔═══════════════════════════════════▼════════════════════════════════════╗
║                   LAYER 1: HARDWARE LAYER                              ║
║      (hardware/arch • hardware/bsp • hardware/specs)                   ║
║  • x86_64 • AArch64 (ARM64) • RISC-V 64 Architecture Profiles          ║
║  • Board Support Packages (BSP) & Device Trees (PC, QEMU, RPi4)        ║
║  • Physical/Virtual Memory Map, ACPI/UEFI & Hardware Specs             ║
╚════════════════════════════════════════════════════════════════════════╝
```

---

## 📂 Layer Directory Structure

| Layer | Directory | Description & Components |
| :--- | :--- | :--- |
| **Layer 1: Hardware** | [`hardware/`](./hardware) | Architecture specs ([`arch/x86_64.json`](./hardware/arch/x86_64.json), [`arch/aarch64.json`](./hardware/arch/aarch64.json), [`arch/riscv64.json`](./hardware/arch/riscv64.json)), device trees ([`bsp/`](./hardware/bsp)), and memory maps ([`specs/`](./hardware/specs)). |
| **Layer 2: Kernel** | [`kernel/`](./kernel) | Hardened Linux 6.x kernel config ([`config/feather_defconfig`](./kernel/config/feather_defconfig)), early userspace bootstrap ([`init/init`](./kernel/init/init)), GRUB/systemd-boot loaders ([`boot/`](./kernel/boot)), in-kernel eBPF probes ([`ebpf/`](./kernel/ebpf)), and build/install scripts ([`scripts/`](./kernel/scripts)). |
| **Layer 3: OS Layer** | [`os/`](./os) | **Platform-Independent OS Files**: Hardware Abstraction Layer ([`os/core/feather-hal`](./os/core/feather-hal)), supervisor daemon ([`os/services/featherd`](./os/services/featherd)), semantic memory ([`os/services/synapsed`](./os/services/synapsed)), ambient perception ([`os/services/feather-watch`](./os/services/feather-watch)), IPC transport ([`os/services/feather-ipc`](./os/services/feather-ipc)), safety execution & sandbox ([`os/safety/aegis-exec`](./os/safety/aegis-exec)), rollback engine ([`os/safety/aegis-snapshot`](./os/safety/aegis-snapshot)), cognitive engine ([`os/intelligence/cortex-core`](./os/intelligence/cortex-core)), MCP bus ([`os/intelligence/agent-bus`](./os/intelligence/agent-bus)), and embeddings ([`os/intelligence/feather-embed`](./os/intelligence/feather-embed)). |
| **Layer 4: Application**| [`apps/`](./apps) | Dual-mode cognitive shell ([`apps/shell/aish`](./apps/shell/aish) / `feather`), interactive terminal dashboard ([`apps/tui/feather-tui`](./apps/tui/feather-tui)), and system tools ([`apps/tools/`](./apps/tools)). |

---

## 🚀 Building & Installing

### 1. Build the OS and Application Layer Binaries
```bash
cargo build --release
```

Binaries produced:
- `target/release/featherd` (OS Layer Supervisor Daemon)
- `target/release/feather` / `aish` (Application Layer Cognitive Shell)
- `target/release/feather-tui` (Application Layer Terminal Dashboard)

### 2. Build & Install the Linux Kernel Base Layer
```bash
# Build custom Linux 6.x kernel with FeatherOS defconfig
./kernel/scripts/build-kernel.sh

# Install kernel bzImage and boot configuration
sudo ./kernel/scripts/install-kernel.sh
```

### 3. Build a Bootable Live Disk Image
```bash
sudo ./kernel/scripts/make-boot-image.sh
# Test boot under QEMU:
qemu-system-x86_64 -m 1G -hda featheros-live.img -nographic
```

---

## 🎮 Running FeatherOS

### Start the Background OS Daemon
```bash
featherd
```

### Interactive Cognitive Shell:
```bash
feather
```

#### Inside the Shell:
* **Standard POSIX Execution**:
  ```bash
  feather:~/project (main)*> ls -la
  feather:~/project (main)*> git status
  ```
* **Natural Language Intent**:
  ```bash
  feather:~/project (main)*> check disk space and find largest folders
  feather:~/project (main)*> find all rust files modified today
  ```
* **System Commands**:
  - `:status` - System telemetry, kernel version & sandbox capabilities
  - `:tools` - List registered Agent Bus (MCP) tools
  - `:memory <query>` - Query persistent semantic memory journal
  - `:index` - Re-index current workspace files
  - `:undo` - Rollback previous file mutation safely
  - `:exit` / `:quit` - Exit shell

### Dual-Pane TUI Monitor:
```bash
feather-tui
```

---

## 🛡️ Aegis Safety Guard

Destructive actions are intercepted and verified before execution:
```
[SAFE]       -> Auto-executed (e.g., ls, cat, git status)
[MODERATE]   -> Non-destructive writes (e.g., touch, mkdir)
[HIGH RISK]  -> Mutative operations requiring explicit user confirmation
[CRITICAL]   -> Automatically blocked (e.g., rm -rf /, del /s /q C:\, mkfs)
```

---

## 📄 License

MIT License. See [LICENSE](LICENSE) for details.
