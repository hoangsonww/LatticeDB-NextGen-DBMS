#!/usr/bin/env bash
set -euo pipefail
BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}"

if [[ -f build/CMakeCache.txt ]]; then
  # Reuse existing generator/config
  cmake --build build -j
else
  # Use Unix Makefiles to avoid generator mismatches in CI
  cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
  cmake --build build -j
fi
