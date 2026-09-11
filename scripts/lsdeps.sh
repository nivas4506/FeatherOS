#!/usr/bin/env bash
cd "$(dirname "$0")/../target/x86_64-pc-windows-gnu/release/deps" || exit 1
echo "== c609 files:"
ls -la | grep c609 || echo NONE
echo "== d file content:"
cat featherd-c609fb3cf06acd4c.d 2>/dev/null || echo NO_D
echo "== exes:"
ls -la ./*.exe 2>/dev/null || echo NO_EXES
