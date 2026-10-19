# FeatherOS Memory Architecture & Layout

FeatherOS enforces strict memory boundaries between bare-metal hardware, the base Linux kernel, the OS Layer, and user application sandboxes.

## 64-Bit Address Space Organization

```
+-----------------------------------+ 0xFFFFFFFFFFFFFFFF (Top of Virtual Memory)
|   Kernel Space (Base Layer)       |
|   - Direct Physical Mapping       |
|   - vmalloc / eBPF Probes / Ring  |
|   - Device Driver MMIO            |
+-----------------------------------+ 0xFFFF800000000000 (Kernel Base)
|          Canonical Hole           |
+-----------------------------------+ 0x00007FFFFFFFFFFF (User Space Limit)
|   FeatherOS Substrate (OS Layer)  |
|   - featherd Supervisor           |
|   - synapsed Vector Memory Map    |
|   - aegis-exec Sandboxes (cgroups)|
+-----------------------------------+
|   Application Layer Space         |
|   - aish Shell / feather-tui      |
|   - Dynamic User Binaries / MCP   |
+-----------------------------------+ 0x0000000000000000
```

## Security & Protection Constraints
- **Hardware Enforced**: NX (No-Execute) page table attributes active on all user data and stack segments.
- **Kernel Guarding**: Kernel space is strictly isolated using Linux KPTI (Kernel Page Table Isolation) and SMAP/SMEP hardware bits.
- **OS Layer Guarding**: Sandboxed child processes run in isolated memory namespaces with strict cgroups memory limits enforced by `aegis-exec`.
