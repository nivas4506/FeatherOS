#!/usr/bin/env bash
cd "$(dirname "$0")/.."
for d in target/x86_64-pc-windows-gnu/release/build/winapi-x86_64-pc-windows-gnu-*/; do
  echo "== $d"
  ls "$d"
  echo "-- output:"
  cat "$d/output" 2>/dev/null
  echo ""
done
