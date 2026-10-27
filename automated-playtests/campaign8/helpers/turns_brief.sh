#!/bin/bash
# usage: turns_brief.sh N -- N x (endturn, auto, choose 0, auto). `auto` plays
# the turns on by itself, so "no End of Turn prompt" lines are harmless when the
# turn number still moves. Prints CHOICES, STATUS and the important LOG lines.
# Tax/party choices: the batch answers row 0 (kiss ring). Read CHOICES lines
# and fix the rest by hand.
N=${1:-5}
D=automated-playtests/campaign8
cd "$(dirname "$0")/../../.." || exit 1
args=()
for i in $(seq 1 "$N"); do args+=("endturn" "auto" "choose 0" "auto"); done
args+=("status" "sync t$RANDOM")
python3 tools/play.py send $D "${args[@]}" 2>&1 | grep -E "CHOICES|STATUS turn|Rebel|Sons of|French|Spanish|Dutch|King|Congress|Founding|Tory|march into|plunder|captur|Fugger|Stamp|tax rate|Stuyvesant|wiped|war" | grep -v "Spies\|treaty\|New Spain\|New France\|Spanish treasure\|price of\|sells\|Continue turn" | uniq | head -30
