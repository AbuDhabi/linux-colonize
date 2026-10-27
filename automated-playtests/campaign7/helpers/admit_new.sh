#!/bin/bash
# usage: admit_new.sh -- admit colonists and fortify guns standing on Jamestown (48,41) with no orders.
D=automated-playtests/campaign7
cd "$(dirname "$0")/../../.." || exit 1
lines=$(python3 tools/play.py send $D "units" "sync a$RANDOM" 2>&1 | grep "^UNIT [0-9]* \(Colonists\|Artillery\|Soldiers\|Dragoons\) at (48,41) moves [0-9]* orders 0")
args=()
while read -r _ id kind _; do
  [ -z "$id" ] && continue
  if [ "$kind" = "Colonists" ]; then args+=("admit $id"); else args+=("activate $id" "key f"); fi
done <<< "$lines"
[ ${#args[@]} -eq 0 ] && exit 0
python3 tools/play.py send $D "${args[@]}" "wait 3" "sync b$RANDOM" 2>&1 | grep "ADMIT\|ERR"
