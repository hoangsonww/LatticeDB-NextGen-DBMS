#!/usr/bin/env bash
set -euo pipefail
cmake -S . -B build/asan -G Ninja -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" -DCMAKE_BUILD_TYPE=Debug
cmake --build build/asan -j
echo "Run with: ASAN_OPTIONS=detect_leaks=1 ./build/asan/latticedb"
