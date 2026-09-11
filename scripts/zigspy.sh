#!/usr/bin/env bash
# Logging shim for zig: records argv, then execs the real zig.
echo "=== ZIG INVOCATION ===" >> "$HOME/zigspy.log"
printf '<%s>\n' "$@" >> "$HOME/zigspy.log"
exec /home/nivas/.local/zig/zig "$@"
