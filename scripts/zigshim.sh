#!/usr/bin/env bash
# Force a relink of feather.exe under zigbuild and capture the link line.
set -u
cd "$(dirname "$0")/.."
touch apps/shell/aish/src/shell.rs
cargo zigbuild --verbose --release --target x86_64-pc-windows-gnu -p aish > win-zig.log 2>&1
echo "exit=$?"
grep -o -E '\-L [^ ]+' win-zig.log | sort -u
