#!/usr/bin/env bash
set -euo pipefail

# Run all tests in tests/run_all.sh
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOT="${DIR%/scripts}"

if [[ ! -x "${ROOT}/build/latticedb" ]]; then
  echo "[test.sh] Binary not found, building..."
  "${ROOT}/scripts/build.sh"
fi

exec "${ROOT}/tests/run_all.sh"
