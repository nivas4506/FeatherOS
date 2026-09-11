#!/usr/bin/env bash
D="$HOME/.local/zig/lib/libc/mingw/lib64"
echo "count: $(ls "$D" | wc -l)"
for n in libmsvcrt.a msvcrt.def libmingw32.a libmingwex.a libpthread.a libunwind.a libgcc.a libgcc_eh.a libsynchronization.a synchronization.def libkernel32.a kernel32.def libntdll.a ntdll.def libwinapi_ntdll.a; do
  if [ -e "$D/$n" ]; then echo "FOUND $n"; fi
done
