#!/usr/bin/env bash
set -euo pipefail
if [[ ! -x build/latticedb ]]; then
  echo "Binary not found; building..."
  ./scripts/build.sh
fi
./build/latticedb < tests/smoke.sql
