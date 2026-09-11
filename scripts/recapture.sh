#!/usr/bin/env bash
# Force relink (only final crates recompile) to regenerate single-line captures.
set -euo pipefail
cd "$(dirname "$0")/.."
export CC_x86_64_pc_windows_gnu="${CC_x86_64_pc_windows_gnu:-zig-cc-win}"
export TARGET_CC="${TARGET_CC:-zig-cc-win}"
export CARGO_TARGET_X86_64_PC_WINDOWS_GNU_LINKER="${CARGO_TARGET_X86_64_PC_WINDOWS_GNU_LINKER:-winlink-capture}"
rm -rf "$HOME/feather-winlinks"
cargo clean -p featherd -p aish -p feather-tui > /dev/null 2>&1
cargo build --release --target x86_64-pc-windows-gnu -p featherd -p aish -p feather-tui 2>&1 | tail -n 3
ls -la "$HOME/feather-winlinks/"
head -c 300 "$HOME"/feather-winlinks/featherd.ps1
echo ""
