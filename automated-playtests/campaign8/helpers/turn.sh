#!/bin/bash
# usage: turn.sh [N] -- end N turns (endturn, auto, advise each), then status.
# Reads: CHOICES: needs `choose N` (read the rows first). ERR = a unit still
# needs orders or a popup is open. ADVISE WARN = fix before the next endturn.
N=${1:-1}
D=automated-playtests/campaign8
cd "$(dirname "$0")/../../.." || exit 1
args=()
for i in $(seq 1 "$N"); do
  args+=("endturn" "auto" "advise")
done
args+=("status" "sync t$RANDOM")
python3 tools/play.py send $D "${args[@]}" 2>&1 | grep -v "^MAP\|^UNIT\|^NEAR\|^RES\|^  \|^(inactive)\|^EUROPE\|^@@\|^SHOT"
