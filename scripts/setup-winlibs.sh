#!/usr/bin/env bash
# One-time setup: collect Windows import libs (.a) into ~/.local/winlibs
# so the zig link wrapper can satisfy rustc's explicit -l flags.
set -euo pipefail
REG="$HOME/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f"
OUT="$HOME/.local/winlibs"
mkdir -p "$OUT"
n=0
for d in \
  "$REG/winapi-x86_64-pc-windows-gnu-0.4.0/lib" \
  "$REG/windows_x86_64_gnu-0.52.6/lib" \
  "$REG/windows_x86_64_gnu-0.53.1/lib" \
  "$REG/windows_x86_64_gnullvm-0.52.6/lib" \
  "$REG/windows_x86_64_gnullvm-0.53.1/lib"; do
  [ -d "$d" ] || continue
  for f in "$d"/*.a; do
    base="$(basename "$f")"
    [ -e "$OUT/$base" ] || { ln -s "$f" "$OUT/$base"; n=$((n + 1)); }
  done
done
echo "linked $n import libs into $OUT"
ls "$OUT" | wc -l
