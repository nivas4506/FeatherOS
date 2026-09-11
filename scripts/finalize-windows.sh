#!/usr/bin/env bash
# Phase 3: promote freshly linked deps/*.exe to release names.
# Run AFTER the ~/feather-winlinks/*.ps1 scripts succeed on Windows.
set -euo pipefail
cd "$(dirname "$0")/../target/x86_64-pc-windows-gnu/release"
promote() {
  pattern="$1"; dest="$2"
  # newest match wins (a fresh link just overwrote it)
  src="$(ls -t deps/$pattern 2>/dev/null | head -n 1)"
  if [ -z "$src" ]; then echo "MISSING: $pattern"; exit 1; fi
  size="$(stat -c %s "$src")"
  if [ "$size" -lt 1000000 ]; then echo "SUSPICIOUSLY SMALL: $src ($size bytes)"; exit 1; fi
  if [ "$src" -ef "$dest" ]; then
    echo "$dest already linked to $src ($size bytes)"
  else
    cp "$src" "$dest"
    echo "$dest <- $src ($size bytes)"
  fi
}
promote 'featherd-*.exe' featherd.exe
promote 'feather-*.exe' feather.exe
promote 'aish-*.exe' aish.exe
promote 'feather_tui-*.exe' feather-tui.exe
ls -la featherd.exe feather.exe aish.exe feather-tui.exe
