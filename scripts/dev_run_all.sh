#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}" )/.." && pwd)"
cd "$ROOT"

echo "[devcontainer] Configure with Ninja"
rm -rf build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo

echo "[devcontainer] Build project"
cmake --build build -j

echo "[devcontainer] Format sources (if target exists)"
cmake --build build --target format || true

echo "[devcontainer] Build after formatting"
cmake --build build -j

echo "[devcontainer] Run ctest"
ctest --test-dir build -j 4 --output-on-failure || true

echo "[devcontainer] Run shell test harness"
bash tests/run_all.sh || true

echo "[devcontainer] Smoke SQL"
printf "%s" $'CREATE TABLE t (id TEXT PRIMARY KEY, v INT);\nINSERT INTO t (id, v) VALUES (\'a\', 1);\nSELECT id, v FROM t;\nEXIT;\n' | build/latticedb || true

echo "[devcontainer] Done."

