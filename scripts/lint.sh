#!/usr/bin/env bash
set -euo pipefail
ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"

echo "[lint] Checking formatting..."
"${ROOT}/scripts/format.sh" --check || { echo "Formatting failed"; exit 1; }

if command -v cppcheck >/dev/null 2>&1; then
  echo "[lint] Running cppcheck..."
  cppcheck --enable=warning,style,performance,portability --std=c++17 \
           --inline-suppr --quiet --error-exitcode=2 src || {
    echo "cppcheck found issues"; exit 2; }
else
  echo "[lint] cppcheck not installed; skipping."
fi

echo "[lint] OK"
