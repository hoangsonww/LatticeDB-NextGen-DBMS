#!/usr/bin/env bash
set -euo pipefail
BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build build -j
