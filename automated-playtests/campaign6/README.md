# Automated playtest, campaign 6

Goal (user brief): play **England** on **Discoverer** (easiest) with the
headless port. Declare independence before the 1800 cutoff, then keep the
War of Independence going (do not win, do not lose) until the game
auto-ends in 1850. Record bugs. Not a win attempt.

Harness: `build/debug/play_driver` via `tools/play.py start|send|stop
automated-playtests/campaign6`. New game `--seed 1812`, England, Discoverer
(wizard: Discoverer card, England flag, Return through the text pages).
Trace autosaves: `saves/trace_autosave_turn_NNN.sav` (one per turn). Log:
`bin/opencol.log`. Bugs: [bugs.md](bugs.md). Screenshots: `shots/`.

Win/loss gates (src/core/ai_king_war.c, `ai_king_woi_end_*`), which decide
how to stay in the war:
- WIN for us (game ends): crown holds no colony AND crown land units below
  the give-up bar (1, or 8 after a crown capture) AND REF pool score < 4.
  A crown-held colony blocks the win.
- LOSS (game ends): no colonies left, or no port colony, or crown holds
  >= 90% of colony population.
- 1850 WoI: `@RETIRING2` auto-end.

## Journal

### 1492-1496: landfall, Jamestown

- Start: Caravel with Pioneer (100 tools) and Veteran Soldier (50 muskets),
  1000 gold. Turn 0 sails west; "Land Ho" on turn 2 (1494).
- Named the land by typing "New Albion"; the field was pre-filled with
  "New England" and the typed text was appended: "New AlbionNew England"
  (CN6-O1).
- Iroquois (4 villages) offered peace and the land on turn 2 (1494).
- Founded **Jamestown** (39,27) on 1495 with the soldier. Warned "few
  productive squares"; built anyway (choice 1). Soldier fishes south.
- 1496: Pioneer clears the forest tile north; clearing Iroquois forest at
  peace asks for compensation: paid 97 gold (kept peace).
- 1496 Congress: picked **Pocahontas** (Political).

### 1496-1504: Europe trip

- 1502: sailed the Caravel to London. Recruited an Expert Farmer (140 gold),
  then an Elder Statesman (160, the second recruit list differed from the
  first). Gold 603.
- Caravel sailed back: Jamestown received the Statesman and the Farmer
  (1504, admitted with `admit`). Jamestown pop 3, food -2.
