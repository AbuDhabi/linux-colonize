#!/bin/bash
# usage: turns.sh N  -- end N turns (endturn + auto each), then status + colonies.
# Read the output: CHOICES: needs `choose N`; ERR no End of Turn = a unit
# still has moves/orders (fortify it or give orders, then rerun).
N=${1:-1}
D=automated-playtests/campaign7
args=()
for i in $(seq 1 "$N"); do
  args+=("endturn" "auto")
done
args+=("status" "colonies" "sync t$RANDOM")
cd "$(dirname "$0")/../../.." && python3 tools/play.py send $D "${args[@]}" 2>&1 | grep -v "^@@\|^UNIT\|inactive\|EUROPE\|Scouts\|COLONY [012]\|colonist [0-9] prof\|^  colonist"
