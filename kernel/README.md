# Kernel Layer: Linux Base Layer (Layer 2)

The **Linux Kernel Base Layer** is the hardened, minimal kernel substrate that boots directly on the underlying hardware (or hypervisor) and provides low-level execution primitives, hardware drivers, memory management, and security boundaries.

```
┌────────────────────────────────────────────────────────┐
│                   APPLICATION LAYER                    │
│      (Cognitive Shell, TUI Monitor, MCP Agent Tools)   │
└───────────────────────────┬────────────────────────────┘
                            │
┌───────────────────────────▼────────────────────────────┐
│                       OS LAYER                         │
│   (Platform-Independent Services, HAL, Daemon, Aegis)  │
└───────────────────────────┬────────────────────────────┘
                            │ Syscalls / cgroups / eBPF / /init
┌───────────────────────────▼────────────────────────────┐
│                 KERNEL LAYER (Linux 6.x)               │
│    (Custom Defconfig, Initramfs, eBPF, Drivers, Boot)  │
└───────────────────────────┬────────────────────────────┘
                            │ Hardware Control / ACPI / Device Tree
┌───────────────────────────▼────────────────────────────┐
│                    HARDWARE LAYER                      │
│     (x86_64 • ARM64 • RISC-V • Bare-Metal & QEMU)      │
└────────────────────────────────────────────────────────┘
```

## Responsibilities
1. **Hardware Interfacing**: Initializes CPU cores, physical memory maps, interrupts, storage controllers (NVMe, SATA, VirtIO), and network interfaces.
2. **Process & Namespace Isolation**: Provides the kernel-level primitives used by FeatherOS `aegis-exec` (User Namespaces, PID Namespaces, Mount Namespaces, Network Namespaces).
3. **Resource Guardrails**: Enforces cgroups v2 memory limits, CPU quotas, and I/O throttling for cognitive agent tasks.
4. **Syscall Boundary & Safety**: Provides `seccomp-bpf` filters that immediately trap dangerous kernel syscalls.
5. **Continuous Kernel Perception**: Uses in-kernel eBPF probes ([`kernel/ebpf/feather_probe.bpf.c`](./ebpf/feather_probe.bpf.c)) to feed real-time telemetry into FeatherOS ambient perception (`feather-watch`).
6. **Bootstrap to OS Layer**: The custom early userspace init system ([`kernel/init/init`](./init/init)) configures virtual filesystems and launches the FeatherOS OS daemon (`featherd`).

## Directory Layout
- [`kernel/config/feather_defconfig`](./config/feather_defconfig): Stripped, high-security Linux 6.x kernel configuration tuned for sub-second boot times.
- [`kernel/config/kernel-cmdline.cfg`](./config/kernel-cmdline.cfg): Recommended kernel command-line parameters.
- [`kernel/boot/grub.cfg`](./boot/grub.cfg): GRUB bootloader configuration for UEFI/BIOS boots.
- [`kernel/boot/systemd-boot.conf`](./boot/systemd-boot.conf): Lightweight systemd-boot loader entry.
- [`kernel/init/init`](./init/init): The early userspace bootstrap init executable.
- [`kernel/ebpf/feather_probe.bpf.c`](./ebpf/feather_probe.bpf.c): eBPF program monitoring process lifecycle and file modifications.
- [`kernel/scripts/build-kernel.sh`](./scripts/build-kernel.sh): Builds the Linux kernel from upstream source with `feather_defconfig`.
- [`kernel/scripts/install-kernel.sh`](./scripts/install-kernel.sh): Installs the Linux kernel base layer and creates boot entries.
- [`kernel/scripts/make-boot-image.sh`](./scripts/make-boot-image.sh): Packages kernel, initramfs, and FeatherOS binaries into a bootable QEMU/hardware disk image.
