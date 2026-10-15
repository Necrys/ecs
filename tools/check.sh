#!/bin/sh
# Thin POSIX wrapper around tools/check.py.
# Usage: tools/check.sh [--full] [--sanitizers] [--tidy] [--staged]
set -e
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
for candidate in python3 python; do
  if command -v "$candidate" >/dev/null 2>&1; then
    exec "$candidate" "$SCRIPT_DIR/check.py" "$@"
  fi
done
echo "error: neither python3 nor python was found on PATH" >&2
exit 2
