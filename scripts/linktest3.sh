#!/usr/bin/env bash
# Test whether -Wl,/force lets the duplicate ___chkstk_ms through.
set -u
cd "$(dirname "$0")"
LIBDIR="$HOME/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/windows_x86_64_gnu-0.52.6/lib"
WINLIBS="$HOME/.local/winlibs"
"$HOME/.local/zig/zig" cc -target x86_64-windows-gnu \
  scripts/rustc_symbols.o 2>/dev/null || true
rustc --target x86_64-pc-windows-gnu --crate-type bin linkbin.rs \
  -C linker=zig-link-win \
  -L "native=$LIBDIR" -L "native=$WINLIBS" -o linkbin-force.exe 2>&1 | head -n 8
echo "exit=$?"
ls -la linkbin-force.exe 2>/dev/null
