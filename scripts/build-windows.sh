#!/usr/bin/env bash
# Cross-compile FeatherOS release binaries for 64-bit Windows (GNU ABI)
# from Linux using the Rust windows-gnu target plus zig-based wrappers:
#   zig-cc-win   - C compiler for build scripts (translates --target= for zig)
#   zig-link-win - link driver (filters flags cargo-zigbuild-style, maps
#                  -lwinapi_X to -lX, drops compiler_builtins in favor of
#                  zig's compiler_rt, appends ~/.local/winlibs import libs)
#
# One-time setup: scripts/setup-winlibs.sh (import-lib farm).
# Requires: rust target x86_64-pc-windows-gnu, zig on PATH.
# Output: target/x86_64-pc-windows-gnu/release/*.exe
set -euo pipefail
cd "$(dirname "$0")/.."
export CC_x86_64_pc_windows_gnu="${CC_x86_64_pc_windows_gnu:-zig-cc-win}"
export TARGET_CC="${TARGET_CC:-zig-cc-win}"
export CARGO_TARGET_X86_64_PC_WINDOWS_GNU_LINKER="${CARGO_TARGET_X86_64_PC_WINDOWS_GNU_LINKER:-zig-link-win}"
cargo build --release --target x86_64-pc-windows-gnu -p featherd -p aish -p feather-tui
ls -la target/x86_64-pc-windows-gnu/release/featherd.exe \
       target/x86_64-pc-windows-gnu/release/feather.exe \
       target/x86_64-pc-windows-gnu/release/aish.exe \
       target/x86_64-pc-windows-gnu/release/feather-tui.exe
