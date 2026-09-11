#!/usr/bin/env bash
# Force relink of aish bins under zigbuild and dump the linker driver line.
set -u
cd "$(dirname "$0")/.."
cargo clean -p aish > /dev/null 2>&1
cargo zigbuild --verbose --release --target x86_64-pc-windows-gnu -p aish > win-zig2.log 2>&1
echo "exit=$?"
grep -E 'linking with' win-zig2.log | head -n 2 | cut -c1-300
grep -o -E 'zig[^ "]*cc[^ "]*' win-zig2.log | sort -u | head
