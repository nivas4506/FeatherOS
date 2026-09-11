#!/usr/bin/env bash
set -u
cd "$(dirname "$0")"
ZIG="$HOME/.local/zig/zig"
echo 'int main(){return 0;}' > zigt.c
run() {
  desc="$1"; shift
  if "$ZIG" cc -target x86_64-windows-gnu zigt.c "$@" -o zigt.exe 2>zigerr.txt; then
    echo "OK   $desc"
  else
    echo "FAIL $desc :: $(head -n 3 zigerr.txt | tr '\n' '|')"
  fi
}
run "plain"
run "-lmsvcrt" -lmsvcrt
run "-Bstatic -lmsvcrt" -Wl,-Bstatic -lmsvcrt
run "-l:libpthread.a" "-l:libpthread.a"
run "-lmingw32 -lmingwex" -lmingw32 -lmingwex
run "-lgcc_eh" -lgcc_eh
run "-lwindows.0.52.0 with -L" -L "$HOME/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/windows_x86_64_gnu-0.52.6/lib" -lwindows.0.52.0
