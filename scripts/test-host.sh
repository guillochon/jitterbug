#!/usr/bin/env bash
# Build and test everything that runs on the dev machine: core, simulator, tools.
set -euo pipefail
source "$(dirname "$0")/common.sh"
cmake -S "$JB_ROOT" -B "$JB_ROOT/build-host" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$JB_ROOT/build-host" -j
ctest --test-dir "$JB_ROOT/build-host" --output-on-failure
