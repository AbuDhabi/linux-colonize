#!/usr/bin/env bash
# Overlay a pixel-coordinate grid on a screenshot, for matching UI element
# positions against source code constants.
#
# Usage: grid_overlay.sh <image> [grid_step] [out]
#   grid_step: spacing in px between gridlines (default 20)
#   out:       output path (default <image>.grid.png)
#
# Major lines every 5*grid_step are drawn brighter and labeled with their
# coordinate. Original game screens are often 320x200 or 640x480 -- use a
# small step (10-20) for those.

set -euo pipefail

img="${1:?usage: grid_overlay.sh <image> [grid_step] [out]}"
step="${2:-20}"
out="${3:-${img%.*}.grid.png}"

w=$(identify -format "%w" "$img")
h=$(identify -format "%h" "$img")

major=$(( step * 5 ))

# Build minor gridlines (thin, semi-transparent) and major gridlines
# (brighter, labeled) as MVG draw commands.
minor_draw=""
for ((x=0; x<=w; x+=step)); do
  minor_draw+="line $x,0 $x,$h "
done
for ((y=0; y<=h; y+=step)); do
  minor_draw+="line 0,$y $w,$y "
done

major_draw=""
for ((x=0; x<=w; x+=major)); do
  major_draw+="line $x,0 $x,$h "
done
for ((y=0; y<=h; y+=major)); do
  major_draw+="line 0,$y $w,$y "
done

convert "$img" \
  -fill none -stroke "rgba(0,255,0,0.35)" -strokewidth 1 -draw "$minor_draw" \
  -fill none -stroke "rgba(255,0,0,0.8)" -strokewidth 1 -draw "$major_draw" \
  "$out"

# Label major gridlines with coordinates along top and left edges.
label_draw=""
for ((x=0; x<=w; x+=major)); do
  label_draw+="text $((x+2)),10 '$x' "
done
for ((y=major; y<=h; y+=major)); do
  label_draw+="text 2,$((y-2)) '$y' "
done

convert "$out" \
  -fill "rgba(255,255,0,0.9)" -stroke none -pointsize 10 -font DejaVu-Sans-Mono \
  -draw "$label_draw" \
  "$out"

echo "wrote $out (${w}x${h}, minor=${step}px, major=${major}px)"
