# FeatherOS Hardware Requirements

FeatherOS is engineered as an ultra-lightweight, high-performance cognitive operating system substrate. Because the core services are written in memory-safe, zero-cost abstraction Rust, it runs smoothly on constrained edge devices as well as high-end workstations.

## Minimum Specifications (Headless Edge / MicroVM)
- **CPU**: 1 Core (x86_64 or AArch64 @ 1.0 GHz)
- **RAM**: 512 MB (Daemon + In-memory perception cache)
- **Storage**: 1 GB NVMe, SSD, or eMMC (Kernel + Rootfs + OS Layer)
- **Network**: 10/100 Mbps Ethernet or VirtIO Net
- **Firmware**: UEFI 2.4+ or Legacy BIOS / U-Boot

## Recommended Specifications (Cognitive Workstation with Local SLM)
- **CPU**: 4+ Cores (x86_64 AVX2 / ARM64 NEON)
- **RAM**: 4 GB - 8 GB (Allows running local quantized 3B SLMs like Llama 3.2 3B via Ollama or ONNX Runtime)
- **Storage**: 16 GB NVMe SSD (Semantic Vector Store + Snapshot Journal)
- **GPU (Optional)**: Vulkan / CUDA / ROCm / Apple Metal compatible accelerator for fast vector embeddings
