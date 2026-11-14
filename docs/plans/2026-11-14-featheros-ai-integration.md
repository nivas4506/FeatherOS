# FeatherOS AI-Native Distro Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build FeatherOS, a lightweight, highly efficient Linux distribution natively integrated with Small Language Models (SLMs) and Vision-Language Models (VLMs) for system-level intelligence, low-resource usage, and direct OS task execution.

**Architecture:** A streamlined Linux kernel (v7.3.4) base with memory-efficient drivers (zRAM, virtio, DRM/KMS, Vulkan/GPU acceleration) powering a lean userspace (musl libc + BusyBox/Toybox + minimal init). A system daemon (`featherd`) provides an ultra-low-latency UNIX domain socket IPC API (`/run/feather/ai.sock`) backed by an embedded C++ inference engine (`llama.cpp`), exposing fast, quantised SLM (text/command reasoning) and VLM (visual context & screen awareness) capabilities to CLI utilities and OS shell workflows.

**Tech Stack:**
- **Kernel:** Linux 7.3.4 (x86_64, modular, zRAM, virtio-gpu, cgroups v2, minimal kernel config)
- **Userspace & Init:** Musl libc, BusyBox, custom lightweight PID 1 init sequence
- **AI Core:** `llama.cpp` / `whisper.cpp` (lightweight C/C++ engine supporting Vulkan, CPU AVX2/AVX-512, OpenCL, and CUDA)
- **Target Models:** SLM: SmolLM2-135M / 360M / 1.7B, Phi-3.5-mini, Qwen2.5-0.5B/1.5B (GGUF Q4_K_M); VLM: SmolVLM-500M / Moondream2 (GGUF)
- **OS Tooling & IPC:** C/Rust CLI (`feather-ai`), UNIX domain socket daemon (`featherd`), virtual terminal frame buffer / screen context hook
- **Virtualization & Emulation:** QEMU (`qemu-system-x86_64`) with KVM/WHPX hardware acceleration

---

### Task 1: Flatten and Organize Kernel Source Repository

**Files:**
- Move: `linux-7.3.4/linux-7.3.4/*` -> `src/kernel/`
- Create: `.gitignore`
- Create: `Makefile` (root orchestrator)

**Step 1: Create project directory structure**
Organize root repository into `src/kernel`, `src/userspace`, `src/ai-daemon`, `src/tools`, `build/`, `docs/`.

**Step 2: Relocate nested kernel files**
Move files from `linux-7.3.4/linux-7.3.4/` to `src/kernel/` to eliminate confusing directory redundancy.

**Step 3: Setup comprehensive `.gitignore`**
Ignore build artifacts, kernel object files (`*.o`, `*.ko`, `*.cmd`), image binaries (`bzImage`, `initramfs.cpio.gz`), and downloaded model weights (`*.gguf`).

**Step 4: Commit repository foundation**
```bash
git add .
git commit -m "chore: structure FeatherOS repository layout and kernel source"
```

---

### Task 2: Configure and Build Minimal FeatherOS Kernel

**Files:**
- Create: `configs/kernel-featheros.config`
- Create: `scripts/build_kernel.sh`
- Target: `build/bzImage`

**Step 1: Generate minimal baseline configuration**
Create a streamlined kernel config derived from `x86_64_defconfig` with:
- `CONFIG_DEVTMPFS=y` (automatic `/dev` population)
- `CONFIG_BLK_DEV_INITRD=y` (initramfs support)
- `CONFIG_BINFMT_ELF=y`, `CONFIG_BINFMT_SCRIPT=y` (executable execution)
- `CONFIG_ZRAM=y`, `CONFIG_ZRAM_DEF_COMP_LZ4=y` (efficient RAM compression for AI loads)
- `CONFIG_DRM_VIRTIO_GPU=y`, `CONFIG_VIRTIO_PCI=y`, `CONFIG_VIRTIO_NET=y` (QEMU hardware acceleration)
- `CONFIG_CGROUPS=y` (resource isolation for AI background processes)

**Step 2: Script automated kernel compilation**
Write `scripts/build_kernel.sh` that checks for required host tools (`gcc`, `make`, `bison`, `flex`, `libelf-dev`, `bc`) in WSL2/Linux, applies `configs/kernel-featheros.config`, and compiles `arch/x86/boot/bzImage`.

**Step 3: Verify kernel build output**
Verify `bzImage` is generated in `build/bzImage` and log build size (target: < 15MB compressed).

**Step 4: Commit**
```bash
git add configs/ scripts/build_kernel.sh
git commit -m "feat(kernel): add FeatherOS minimal kernel build configuration"
```

---

### Task 3: Assemble FeatherOS Root Filesystem (Rootfs) & Custom Init

**Files:**
- Create: `scripts/build_rootfs.sh`
- Create: `src/userspace/init`
- Create: `src/userspace/etc/featheros-release`
- Create: `src/userspace/etc/motd`

**Step 1: Write lightweight custom `/init` script**
Create POSIX-compliant PID 1 init in `src/userspace/init`:
- Mount `/proc`, `/sys`, `/dev`, `/dev/pts`, `/tmp`, `/run`
- Initialize hostname (`featheros`) and network loopback (`lo`)
- Enable zRAM swap for memory efficiency under model execution
- Start `featherd` (Feather AI daemon)
- Launch interactive login shell or recovery console

**Step 2: Package static BusyBox userspace**
Script download/build of statically linked BusyBox targeting musl to populate `/bin`, `/sbin`, `/usr/bin`.

**Step 3: Assemble initramfs CPIO archive**
Package rootfs into `build/initramfs.cpio.gz`.

**Step 4: Test in QEMU**
Run:
`qemu-system-x86_64 -kernel build/bzImage -initrd build/initramfs.cpio.gz -nographic -append "console=ttyS0 quiet"`
Expected: FeatherOS banner displays and drops into an interactive shell in < 1 second.

**Step 5: Commit**
```bash
git add src/userspace/ scripts/build_rootfs.sh
git commit -m "feat(rootfs): add FeatherOS init system and busybox rootfs builder"
```

---

### Task 4: Develop Native AI System Daemon (`featherd`)

**Files:**
- Create: `src/ai-daemon/main.cpp`
- Create: `src/ai-daemon/server.hpp`
- Create: `src/ai-daemon/inference_engine.hpp`
- Create: `src/ai-daemon/CMakeLists.txt`
- Test: `tests/test_ai_daemon.py`

**Step 1: Design UNIX Domain Socket IPC protocol**
Protocol listening on `/run/feather/ai.sock`:
- JSON/Binary IPC payloads:
  - `{"type": "prompt", "model": "slm", "content": "..."}`
  - `{"type": "vision", "model": "vlm", "image_path": "...", "content": "..."}`
  - `{"type": "stream_token", ...}`
  - `{"type": "status"}`

**Step 2: Integrate `llama.cpp` lightweight C++ engine**
- Manage memory cache pool (quantized KV cache for fast inference).
- Pre-load tiny default SLM (e.g. SmolLM2 135M/360M) with sub-100MB RAM footprint.
- Lazy-load VLM when vision request triggers.

**Step 3: Implement system resource throttler**
Ensure background AI inference yields priority to foreground OS processes and interactive shell commands.

**Step 4: Write unit and IPC tests**
Verify socket communication, token generation, and response parsing.

**Step 5: Commit**
```bash
git add src/ai-daemon/ tests/
git commit -m "feat(daemon): implement featherd IPC socket server and llama.cpp backend"
```

---

### Task 5: Build User-Facing AI Utilities (`feather-ai`)

**Files:**
- Create: `src/tools/feather-ai.c` (or Rust `feather-ai/main.rs`)
- Create: `src/tools/feather-vlm-screen.sh` (screen awareness agent)
- Modify: `src/userspace/init` (auto-start featherd)

**Step 1: Write client CLI `feather-ai`**
Commands:
- `feather-ai ask "How do I check free disk space?"` -> returns command or explanation.
- `feather-ai run "list all files larger than 10MB in /tmp"` -> returns executable bash command with confirmation prompt.
- `feather-ai status` -> displays loaded model, RAM consumption, and inference latency.

**Step 2: Build Visual System Assistant (`feather-vlm`)**
- Hook into Linux framebuffer (`/dev/fb0` or DRM dumb buffers) or terminal screen dump (`screendump`).
- Pipe snapshot to `featherd` VLM endpoint (`{"type": "vision", "content": "Explain what is on the screen or what error occurred"}`).

**Step 3: Integrate with shell environment**
Add AI autocomplete / shortcut key handler in `/etc/profile` (e.g. `Ctrl+A` invokes AI context assistant in shell).

**Step 4: Commit**
```bash
git add src/tools/
git commit -m "feat(tools): add feather-ai CLI and framebuffer VLM screen inspector"
```

---

### Task 6: End-to-End Distribution Packaging & Testing

**Files:**
- Create: `scripts/run_qemu.sh`
- Create: `scripts/build_iso.sh`
- Create: `docs/FEATHEROS_MANUAL.md`

**Step 1: Create QEMU hardware acceleration test harness**
Provide `scripts/run_qemu.sh` supporting WHPX/KVM, audio, display, virtio-gpu, and serial console output.

**Step 2: Package bootable ISO with Limine/Syslinux**
Create an ISO image that can be burned to USB and booted on physical laptops/desktops.

**Step 3: Full system validation test**
- Boot system in QEMU.
- Test memory consumption at idle (Target: < 40MB RAM without model, < 250MB with active SmolLM2).
- Test CLI interaction: `feather-ai ask "Hello FeatherOS"` returns valid generated tokens.
- Test VLM image description pipeline.

**Step 4: Commit**
```bash
git add scripts/run_qemu.sh scripts/build_iso.sh docs/
git commit -m "feat(distro): add QEMU runner, ISO packager, and FeatherOS manual"
```

