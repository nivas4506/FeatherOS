#!/usr/bin/env bash
cd "$(dirname "$0")/.." || exit 1
echo "== zig argv lines 24-35 (driver flags):"
sed -n '24,35p' "$HOME/zigspy.log"
echo "== zig argv tail (link opts):"
sed -n '148,154p' "$HOME/zigspy.log"
