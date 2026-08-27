#!/usr/bin/env bash
# Fast Col1 save-interop check: runs only unit_col1_save (byte-identical
# round-trip over all .SAV fixtures in the repo), not the full ctest suite.
# Use before every user handoff, and after any change touching col1_save.h,
# col1_bridge.c, or colony/unit layout (port_plan.md P10.1).
#
# Usage: tools/check_save_interop.sh [build-dir]
#   build-dir defaults to "build" under the repo root.

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
build_dir="${1:-$repo_root/build}"

if [[ ! -d "$build_dir" ]]; then
  echo "error: build dir '$build_dir' not found — configure it first (cmake -S . -B $build_dir)" >&2
  exit 1
fi

cmake --build "$build_dir" --target unit_col1_save -j"$(nproc 2>/dev/null || echo 4)"

cd "$build_dir"
ctest -R '^unit_col1_save$' --output-on-failure
