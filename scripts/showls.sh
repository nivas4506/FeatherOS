#!/usr/bin/env bash
cd "$(dirname "$0")/.."
grep -o -E '\-L [^ ]+' win-link.log | sort -u
echo '--- link-lib directives seen ---'
grep -o -E 'rustc-link-lib[^"]*' win-link.log | sort -u | head -n 20
