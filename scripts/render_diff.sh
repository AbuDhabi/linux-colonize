#!/usr/bin/env bash
# Pixel-diff two screenshots (e.g. DOS reference vs current port render).
# Highlights mismatches in red; prints an AE (absolute error) pixel count.
#
# Usage: render_diff.sh <reference> <candidate> [out]

set -euo pipefail

ref="${1:?usage: render_diff.sh <reference> <candidate> [out]}"
cand="${2:?usage: render_diff.sh <reference> <candidate> [out]}"
out="${3:-diff.png}"

# Resize candidate to reference dims if they differ (common early on: scale mismatch).
rw=$(identify -format "%w" "$ref"); rh=$(identify -format "%h" "$ref")
cw=$(identify -format "%w" "$cand"); ch=$(identify -format "%h" "$cand")
if [[ "$rw $rh" != "$cw $ch" ]]; then
  echo "note: size mismatch ref=${rw}x${rh} candidate=${cw}x${ch} -- resizing candidate to match" >&2
  convert "$cand" -resize "${rw}x${rh}!" /tmp/render_diff_resized.png
  cand=/tmp/render_diff_resized.png
fi

compare -metric AE -highlight-color red -fuzz 5% "$ref" "$cand" "$out" 2> /tmp/render_diff_ae.txt || true
ae=$(cat /tmp/render_diff_ae.txt)
echo "wrote $out (AE=${ae} mismatched pixels)"
