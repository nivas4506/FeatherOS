#!/usr/bin/env bash
echo "== winapi import libs:"
ls "$HOME/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/winapi-x86_64-pc-windows-gnu-0.4.0/lib/" | head -n 40
echo "== count:"
ls "$HOME/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/winapi-x86_64-pc-windows-gnu-0.4.0/lib/" | wc -l
echo "== zig libunwind:"
ls "$HOME/.local/zig/lib/" | head -n 30
find "$HOME/.local/zig/lib" -maxdepth 3 -iname '*unwind*' 2>/dev/null | head
echo "== pthread test:"
cd "$(dirname "$0")"
"$HOME/.local/zig/zig" cc -target x86_64-windows-gnu zigt.c -lpthread -o zigt-pthread.exe 2>&1 | head -n 3 && echo PTHREAD_OK
