#!/bin/bash
# Regenerates src/unimplemented.cc for what the shim does not implement yet:
# links node without it, collects the undefined symbols and writes a stub for
# each, then links again.
#
#   deps/v8shim/tools/update-stubs.sh out/Release
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$1"
LOG="$(mktemp)"
: > "$HERE/src/unimplemented.cc"
ninja -C "$OUT" node > "$LOG" 2>&1 || true
if grep -q "error:" "$LOG" && ! grep -q "undefined symbol" "$LOG"; then
  grep "error:" "$LOG" | head -20
  exit 1
fi
python3 "$HERE/tools/generate-stubs.py" "$LOG" > "$HERE/src/unimplemented.cc"
echo "$(grep -c 'Unimplemented(' "$HERE/src/unimplemented.cc") functions not implemented"
rm -f "$LOG"
ninja -C "$OUT" node
