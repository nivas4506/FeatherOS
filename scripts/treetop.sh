#!/usr/bin/env bash
cd "$(dirname "$0")/.."
cargo tree -p featherd --target x86_64-pc-windows-gnu --prefix none 2>/dev/null | sort -u | grep -i -E 'winapi|ntapi|fs2|windows'
