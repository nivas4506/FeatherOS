#!/usr/bin/env bash
# Minimal repro: does rustc accept libwindows.0.52.0.a via -L for windows-gnu?
set -u
cd "$(dirname "$0")"
LIBDIR="$HOME/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/windows_x86_64_gnu-0.52.6/lib"
echo "--- lib dir has import lib:"
ls "$LIBDIR"
echo "--- rustc lib link:"
rustc --target x86_64-pc-windows-gnu --crate-type lib linktest.rs \
  -L "native=$LIBDIR" -o linktest.rlib 2>&1 | head -n 10
echo "rustc-lib exit=$?"
