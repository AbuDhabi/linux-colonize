#!/usr/bin/env bash
# Fast Col1 save-interop check: runs only unit_col1_save (byte-identical
# round-trip over all .SAV fixtures in the repo), not the full ctest suite.
# Use before every user handoff, and after any change touching col1_save.h,
# col1_bridge.c, or colony/unit layout (port_plan.md P10.1).
#
# Usage: tools/check_save_interop.sh [build-dir]
#   build-dir defaults to the "debug" preset tree (build/debug), then
#   build/release, then a plain build/ if one was configured by hand.

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"

if [[ $# -ge 1 ]]; then
  build_dir="$1"
else
  build_dir=""
  for candidate in "$repo_root/build/debug" "$repo_root/build/release" "$repo_root/build"; do
    if [[ -f "$candidate/CMakeCache.txt" ]]; then
      build_dir="$candidate"
      break
    fi
  done
  if [[ -z "$build_dir" ]]; then
    echo "error: no configured build tree found — run 'cmake --preset debug' first" >&2
    exit 1
  fi
fi

if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
  echo "error: '$build_dir' is not a configured CMake build tree — run 'cmake --preset debug' first" >&2
  exit 1
fi

cmake --build "$build_dir" --target unit_col1_save -j"$(nproc 2>/dev/null || echo 4)"

cd "$build_dir"
ctest -R '^unit_col1_save$' --output-on-failure
