#!/bin/sh
# Thin POSIX wrapper around tools/format.py.
# Usage: tools/format.sh [--check] [--list] [--staged]
set -e
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
for candidate in python3 python; do
  if command -v "$candidate" >/dev/null 2>&1; then
    exec "$candidate" "$SCRIPT_DIR/format.py" "$@"
  fi
done
echo "error: neither python3 nor python was found on PATH" >&2
exit 2
