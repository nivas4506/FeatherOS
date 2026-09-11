#!/usr/bin/env bash
echo "== bin:"
ls /mnt/c/MinGW/ucrt64/bin/ | head -n 50
echo "== key files:"
for f in gcc.exe x86_64-w64-mingw32-gcc.exe ld.exe ar.exe dlltool.exe windres.exe libwinpthread-1.dll; do
  ls -la "/mnt/c/MinGW/ucrt64/bin/$f" 2>/dev/null || echo "MISSING $f"
done
echo "== pthread libs:"
ls /mnt/c/MinGW/ucrt64/lib/libpthread.a /mnt/c/MinGW/ucrt64/lib/libwinpthread.a 2>/dev/null
ls /mnt/c/MinGW/ucrt64/x86_64-w64-mingw32/lib/libpthread.a 2>/dev/null
