# Automated playtest, campaign 7

Brief (user): play **England** on **Discoverer** (easiest) with the headless
port. Declare independence before the 1800 cutoff, then keep the War of
Independence going (do not win, do not lose) until the game auto-ends in 1850.
Record bugs. Not a win attempt.

Harness: `build/debug/play_driver` via `tools/play.py start|send|stop
automated-playtests/campaign7`. New game `--seed 7007`, England, Discoverer
(wizard: title "Start a Game in NEW WORLD", Discoverer card, England flag,
Return through the text pages). Trace autosaves:
`saves/trace_autosave_turn_NNN.sav`. Log: `bin/opencol.log`. Bugs:
[bugs.md](bugs.md). Screenshots: `shots/`.

Rules followed: [automated-playtests/RULES.md](../RULES.md). Declaration gates
are the Viceroy numbers there (no Discoverer numbers recorded; do not lower
them).

## Journal

### 1492-1498: landfall, Jamestown

- Start: Caravel (id 1) with Pioneers (100 tools) and Veteran Soldier (50
  muskets), 1000 gold, at (56,42). `goto` to (40,40) moved 3 tiles, then
  `goto` on later turns did nothing (no MOVE in log). Arrow keys (`key Left`,
  `key KP_4`) moved the caravel reliably; use keys, not goto, for ship moves.
- Land Ho on turn 4 (1496). Name popup: accepted the pre-filled "New England"
  with `key Return` (no typing; see CN7-O1).
- Caravel "Make Landfall" popup (CN7 note): `choose 0` Stay With Ships first
  to survey, then `key Left` again and `choose 1` Make Landfall. Pioneers
  landed on (48,41) (Rain Forest with Minerals, Ore nearby, fish at 50,40).
- 1498 (turn 6): pioneer `activate 2` + `key b` (Build Colony; the first
  `b` only raised the Ore tutorial hint, the second worked). Named the colony
  with `key Return` (default "Jamestown"). Colony 3 of the world.
- Colony opens with Town Hall built, Docks queued (food +2, hammers +0).

### 1498-1554: slow start, one colony (Jamestown 48,41)

- Caravel shuttles Europe: 1500 bought 2 Expert Farmers + 1 Free Colonist
  (gold 1000 -> 520). Passage 140-200. Recruit list changes per visit
  (1500: Master Carpenters / Expert Farmers / Free Colonists; 1545: Master
  Carpenters, Indentured Servants, Seasoned Scouts).
- Jamestown starts with Carpenter's Shop, Weaver's House, Rum Distiller's
  House already built (founding starters, see CN7-O5). No forest-free
  hammers: hammers come only from a staffed Carpenter's Shop (lumber in).
  Hammers were 0 until a colonist worked the Carpenter's Shop (`work 3 CI 35`).
- Ring tiles at (48,41): N, S, SW, W are forest (uppercase on map). Lumberjack
  (job 5) on forest tiles 0/4/6 gives lumber; hammers +3 -> +12 with Master
  Carpenter in the shop plus Lumber Mill.
- Ore sells for 2 each in London in 1515 (100 ore = 200 gold, `key u` in
  Europe). Not a money source early.
- Build order done: Docks (1523-1530), Stockade (1530), Stable (1536),
  Warehouse (1545), Armory (1555 in progress), Fort next (120 hammers).
- Caravel parking: `activate ID`, `key f` (fortify). A caravel with moves
  and no orders blocks `endturn`. Sailing from Europe: click ship, `choose 1`
  (Set sail), `choose 0` (Yes). Port trips take about 3 turns each way.
- Congress: Paine (1499), Franklin (1523, row 3 of the list).
- King: tax 2% (1544) raise -> `choose 0` (kiss ring).

### 1554-1639: slow colony, bells start

- 1555-1568: Armory, Fort (120 hammers, 10 tools) done. Tools went 100 -> 0
  at the Fort; Blacksmith's Shop (tier 1 of the Blacksmith chain) accepted
  after the House (row 39) was already owned (starter, CN7-O5).
- Food crashes twice (1553 food -3, 1578 food 12 with -4): a colonist left
  the farm for the Blacksmith. Rule: one farm/fish colonist per 2 pop.
  Europe trip 1593: sold 100 ore for 188 (tax 6%, price 2), bought Expert
  Farmer 260 (passage up from 140 in 1500).
- Stray Seasoned Scout (unit 529, from the Europe dock) and caravel with no
  orders blocked `endturn` ("ERR no End of Turn prompt"): `key f` each.
- 1609-1624: King tax 2 -> 8% (kiss ring each time). Congress 1624: Peter
  Stuyvesant (row 0; Custom House). Iroquois peace offered 1631 (accepted).
- 1629: Town Hall bells 1 -> 6 -> 10 with two colonists in Town Hall
  (`work 3 CI 9`); SoL 6% (1622) -> 13% (1629) -> 20% (1639).
- Food stock 132 (+18) at 1639, pop 8 stuck: colony growth takes the food
  surplus toward a 200 bank, then a new colonist (building_production.md:40).
- Montreal (French), Fort Orange (Dutch), Guadeloupe, Santo Domingo, Fort
  Nassau (AI) founded around us by 1640.

### 1639-1712: Fortress, tools, Europe sales

- Tools sell well: 100 tools = 800 gold in London (1671, tax 22%). Ore sells
  for 2. Europe trip 1671: gold 100 -> 1000, then recruits (Master Carpenter
  + 2 Indentured Servants, 260 each); only 2 boarded (the caravel's cargo
  took the passenger room, CN7-O10).
- Las Casas (1643) and Pocahontas (1688) in Congress. Stuyvesant (1624).
- Fortress built 1712 needs 200 tools at completion (tools x10 in NAMES),
  not 20. Tools were 0 at 1660 and 187 at 1700 after two Blacksmith workers.
- Food: colonist moves between farm/Town Hall/Blacksmith caused -5..-7 food
  three times. Keep one farmer per 2 pop before changing jobs.
- Decision (1712): the Viceroy declaration gate (60+ guns, 130 horses,
  200 muskets) is not reachable at this economy (6 hammers/turn, 0-2 colonies
  of size). Plan: keep the economy growing, declare at the latest turn that
  still lands before 1800 (turn 507 at the latest) with the best defence the
  colony can buy/build, record the outcome. Flag to user before declaring.

### 1794: independence declared (turn 497)

- Gate check (RULES "Before declaring", Viceroy numbers):
  - Fortress in Jamestown: MET (built 1712).
  - SoL 100% in the target colony: NOT MET (SoL 95%).
  - 60+ artillery in the target colony: NOT MET (6 artillery, all fortified
    in Jamestown; the artillery build repeated from 1743, ~1 gun per 16 turns
    at +12 hammers/turn).
  - One gun in every other colony: N/A (only one colony).
  - ~130 horses and ~200 muskets: NOT MET (1 scout with 50 horses, 1 soldier
    with 50 muskets).
  - Declared anyway: the user's brief asks for independence before 1800, and
    the economy here could not reach the gate in time (see 1712 decision).
  Deviation recorded here; not a recommendation.
- Declared from GAME menu: click 20 6 (open menu), click 54 85 (DECLARE
  INDEPENDENCE), choose 1 ("Yes! Give me liberty ..."). The confirm was the
  only gate (CHOICES list).
- Result of the declaration turn: Continental Congress signs; Royal Navy
  seized our Frigate on the high seas (the King's gift from 1718).
- Save: saves/declared_1794.sav (state before the first war turn is
  pre_declare_1796.sav, name is wrong: it was saved at 1794).

### 1794-1797: the war, lost

- Turn 497 (1794) declared. Turns 498-502: REF landed next to Jamestown (log
  "Royal Expeditionary Force lands near Jamestown!"). Six artillery (fortified
  in the colony) traded fights with Tories: each Rebel Artillery win was
  followed by a Tory win that destroyed the gun. Regulars and Continental
  Army went down; colonists were captured.
- Turn 502 (spring 1797): "Tories march into Jamestown!" Our only colony is
  gone; the game ends on the COLONIZATION SCORE screen: total 130,
  Rebel Sentiment +95, English Citizens +0.
- Result: LOST in 1797, 3 years after declaring. The brief (keep the war to
  1850) was not met.
- Why (my reading, not a verified bug): one colony, pop 17, six artillery,
  no horses or muskets to re-arm. The gate numbers in RULES.md were not met
  (see the gate check above). Also the Viceroy-measured gate says 60+ guns;
  six guns did not hold one landing.
