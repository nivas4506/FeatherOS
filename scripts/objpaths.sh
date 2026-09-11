#!/usr/bin/env bash
cd "$(dirname "$0")/.."
grep -o -E '\\\\[A-Za-z0-9._\\-]*\.o' "$HOME/feather-winlinks/featherd.ps1" | sort -u | head -n 25
