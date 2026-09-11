#!/usr/bin/env bash
ls ~/.local/zig/lib/libc/mingw/lib64/ | grep -iE 'msvcrt|synchron|unwind|kernel32|mingw|bcrypt|ntdll|userenv|ws2_32|dbghelp'
