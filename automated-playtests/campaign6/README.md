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

### 1504-1520: starvation and first food fix

- Jamestown (39,27) on the Iroquois island: pop 3, food -2. I put the
  Expert Farmer on job 2 (tobacco, not farm) by mistake: the stores ran out
  and colonists starved over ~8 turns (1510-1518). Job 0 is Farmer, 8 is
  Fisherman (docs/terrain_yields.md). Fixed by `field 0 2 0 0`.
- 1518-1520: Congress pick Pocahontas (1496) and Benjamin Franklin (1587).
- Expert Farmers, Elder Statesmen, Master Carpenters bought in London
  (140-220 gold; passage rose over time: 140 in 1502, 200 in 1534, 260
  in 1586).

### 1520-1560: Europe shuttle, Plymouth

- Caravel shuttles: furs (400 gold per 100, sold with `key u` in London)
  are the main early income. Fur stock caps at 100 in Jamestown.
- Jamestown lumber ran out (1532): the Carpenter's Shop turns lumber into
  hammers, so the Stable (64 hammers) stalls. Pioneer clearing on Iroquois
  forest costs 97-195 gold each time (price rises).
- 1547: scouting west found a hill coast with fish. Founded **Plymouth**
  (25,33) with a free colonist (1557). Apache offered peace there (6 camps).
- 1573: Seasoned Scout visited the Iroquois village at (38,26): chief gave
  128 gold of beads. Scouts come with 50 horses (`units` shows it).
- 1576-1589: Jamestown food and lumber still tight; Plymouth food +2. King
  raises tax at every turn now (1 point per few years). Gold 488 at 1589.
- Congress: Franklin joined 1587; FF pick went to Peter Minuit by accident
  (the helper's blind `choose 0` answered the Congress popup). Process
  note only: keep `choose 0` out of loops while a Congress is pending.
