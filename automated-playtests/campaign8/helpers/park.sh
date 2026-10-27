#!/bin/bash
# usage: park.sh ID [ID ...] -- fortify each unit (activate + f). Units with
# moves 0 cannot fortify; run it at turn start, before endturn.
D=automated-playtests/campaign8
cd "$(dirname "$0")/../../.." || exit 1
args=()
for id in "$@"; do args+=("activate $id" "key f" "wait 1"); done
[ ${#args[@]} -eq 0 ] && exit 0
python3 tools/play.py send $D "${args[@]}" "sync p$RANDOM" 2>&1 | grep "ERR" | head -3
