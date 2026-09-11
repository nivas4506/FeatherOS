#!/usr/bin/env bash
# Force a relink of one small binary under zigbuild while spying on zig.
set -u
cd "$(dirname "$0")/.."
chmod +x scripts/zigspy.sh
rm -f "$HOME/zigspy.log"
touch crates/feather-tui/src/main.rs
CARGO_ZIGBUILD_ZIG_COMMAND="$PWD/scripts/zigspy.sh" \
  cargo zigbuild --release --target x86_64-pc-windows-gnu -p feather-tui 2>&1 | tail -n 2
echo "--- zig invocations:"
grep -c 'ZIG INVOCATION' "$HOME/zigspy.log"
echo "--- link-ish invocations (last):"
grep -A3 'ZIG INVOCATION' "$HOME/zigspy.log" | grep -B1 -A2 '\-o' | tail -n 30
