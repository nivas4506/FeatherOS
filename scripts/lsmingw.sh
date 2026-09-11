#!/usr/bin/env bash
for d in /mnt/c/MinGW/ucrt64/lib/gcc/*/*/ /mnt/c/MinGW/ucrt64/x86_64-w64-mingw32/lib/ /mnt/c/MinGW/ucrt64/lib/; do
  echo "== $d"
  ls "$d" 2>/dev/null | head -n 40
done
