#!/usr/bin/env bash
set -euo pipefail
ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format not found. Install it to format C++ sources."
  exit 1
fi

mapfile -t FILES < <(git ls-files | grep -E '(^src/).*\.(cpp|h|hpp)$')
if [[ ${#FILES[@]} -eq 0 ]]; then
  echo "No C++ files found."
  exit 0
fi

if [[ "${1:-}" == "--check" ]]; then
  clang-format --dry-run --Werror "${FILES[@]}"
else
  clang-format -i "${FILES[@]}"
fi
echo "Formatting OK."
