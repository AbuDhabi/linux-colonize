#!/bin/bash
# usage: cycle.sh N -- N turns. Per turn: admit colonists standing outside a
# colony, fortify guards (guards.txt) and WARN units, endturn, auto. Then reads
# the last CHOICES line: a King tax popup ("Kiss pinky ring") answers the
# first "Party" row (refuses the tax; costs that stock) unless no party row;
# other popups answer row 0. Prints turn lines and tax/Congress events.
N=${1:-5}
D=automated-playtests/campaign8
OUT=$D/out.txt
cd "$(dirname "$0")/../../.." || exit 1
for i in $(seq 1 "$N"); do
  out=$(python3 tools/play.py send $D "key Escape" "wait 2" "advise" "sync a$RANDOM" 2>&1)
  admits=$(echo "$out" | grep -E "WARN unit [0-9]+ Colonists" | awk '{print $4}' | sort -u)
  mil=$(echo "$out" | grep -E "WARN unit [0-9]+ (Artillery|Dragoons|Soldiers|Merchantman|Caravel|Cont\.|Man-O-War|Frigate|Galleon)" | awk '{print $4}' | sort -u)
  args=()
  for a in $admits; do args+=("admit $a"); done
  for g in $(cat automated-playtests/campaign8/helpers/guards.txt) $mil; do args+=("activate $g" "key f" "wait 1"); done
  args+=("endturn" "auto" "choose 0" "auto" "choose 0" "auto" "status" "sync c$RANDOM")
  python3 tools/play.py send $D "${args[@]}" >/dev/null 2>&1
  for k in 1 2 3; do
    last=$(grep "CHOICES" "$OUT" | tail -1)
    if echo "$last" | grep -q "Kiss pinky ring"; then
      idx=$(echo "$last" | grep -o "\[[0-9]*\] Hold '{[^}]*Party}" | head -1 | sed 's/\[\([0-9]*\)\].*/\1/')
      if [ -n "$idx" ]; then python3 tools/play.py send $D "choose $idx" "wait 4" "auto" "sync k$RANDOM" >/dev/null 2>&1; echo "TAX party $idx"; else python3 tools/play.py send $D "choose 0" "wait 4" "auto" "sync k$RANDOM" >/dev/null 2>&1; echo "TAX kiss (no party)"; fi
    elif echo "$last" | grep -q "Continue turn"; then
      python3 tools/play.py send $D "choose 0" "wait 3" "auto" "sync k$RANDOM" >/dev/null 2>&1
    else
      break
    fi
    last=$(python3 tools/play.py send $D "status" "sync q$RANDOM" 2>&1 | grep "STATUS turn" | head -1)
  done
  python3 tools/play.py send $D "status" "sync z$RANDOM" 2>&1 | grep "STATUS turn" | head -1
done
