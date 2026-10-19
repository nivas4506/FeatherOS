# Hardware Layer (Layer 1)

The **Hardware Layer** forms the foundational base of the FeatherOS system stack. It abstracts bare-metal silicon, system-on-chips (SoCs), and virtualized hardware interfaces into standardized architectural definitions, memory layouts, and device tree bindings.

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
                            │
┌───────────────────────────▼────────────────────────────┐
│                 KERNEL LAYER (Linux 6.x)               │
│    (Custom Defconfig, Initramfs, eBPF, Drivers, Boot)  │
└───────────────────────────┬────────────────────────────┘
                            │
┌───────────────────────────▼────────────────────────────┐
│                    HARDWARE LAYER                      │
│     (x86_64 • ARM64 • RISC-V • Bare-Metal & QEMU)      │
└────────────────────────────────────────────────────────┘
```

## Supported Architectures

| Architecture | Target ABI | Hardware Profile | Description |
| :--- | :--- | :--- | :--- |
| **x86_64** | `x86_64-unknown-linux-gnu` / `musl` | [`hardware/arch/x86_64.json`](./arch/x86_64.json) | Standard PC / Cloud Hypervisors (KVM, QEMU, Bare Metal Intel/AMD) |
| **AArch64** | `aarch64-unknown-linux-gnu` / `musl` | [`hardware/arch/aarch64.json`](./arch/aarch64.json) | ARM64 Neoverse / Apple Silicon / Raspberry Pi 4 & 5 |
| **RISC-V 64** | `riscv64gc-unknown-linux-gnu` | [`hardware/arch/riscv64.json`](./arch/riscv64.json) | SiFive / QEMU Virt RISC-V 64-bit platform |

## Board Support Packages (BSP) & Device Trees
- [`hardware/bsp/generic-pc.dts`](./bsp/generic-pc.dts): ACPI/PCIe baseline device tree for generic x86_64 bare-metal and VMs.
- [`hardware/bsp/qemu-virt.dts`](./bsp/qemu-virt.dts): VirtIO-accelerated device tree for QEMU microvm and virt machines.
- [`hardware/bsp/rpi4.dts`](./bsp/rpi4.dts): Broadcom BCM2711 peripheral mapping for Raspberry Pi 4/400.

## Hardware Specifications
- [`hardware/specs/hardware-requirements.md`](./specs/hardware-requirements.md): Minimum & recommended CPU, RAM, storage, and I/O specifications.
- [`hardware/specs/memory-map.md`](./specs/memory-map.md): Physical and virtual memory addressing model, DMA buffers, and kernel boundary regions.
