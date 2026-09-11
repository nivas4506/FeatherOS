#!/usr/bin/env bash
# Re-run the featherd windows link verbosely and save the full link line.
set -u
cd "$(dirname "$0")/.."
cargo zigbuild --verbose --release --target x86_64-pc-windows-gnu -p featherd > win-link.log 2>&1
echo "exit=$?"
grep -o '\-L "[^"]*"' win-link.log | sort -u | head -n 30
