# Automated playtest, campaign 6

**Result: lost in 1739 (turn 387).** Independence declared 1733 (turn 375);
the King took Plymouth (~1736) and Jamestown (1739), the last colony. The
game ended on the Hall of Fame. The brief (keep the war to 1850) was not met.

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

### 1600-1635: two turns a year, silver and ore, a second ship

- 1600: two turns a year from here (turn 108).
- Plymouth's silver and ore are the money: 100 silver sold for 1900 (1614,
  1622), 100 ore for 450-500. Caravel 282/490 carried them from Plymouth
  to London. Gold 2252 at 1608, 3222 at 1624.
- Market: each sale lowers the price ("silver fallen to 19$"); by 1635
  100 furs sold for 100 (1$ each) after many sales. Sell in parts.
- King's tax: 1% raise every few years (tax 2% in 1558, 4% in 1605,
  7% in 1634). Each raise asks "kiss ring" or "hold party": always kiss
  (party boycotts the goods you sell).
- 1615: King adds artillery to the royal expeditionary force ("Colonial
  leaders express alarm"). First sign of the REF pool growing.
- Purchase list in London (1614): Artillery 500, Caravel 1000, Merchantman
  2000, Privateer 2000, Galleon 5000, Frigate 5000. Bought a second Caravel
  for 1000 (1614).
- Passage price per colonist: 140 (1502), 200 (1534), 260 (1586), 300
  (1614), 340 (1624).
- Jamestown starved again (1627-1634): pop 9 on 5 food tiles, expert
  lumberjack on a farm tile, master carpenters idle with no lumber. Balance
  dropped to 0 and stock ran out twice. Lesson: count food per colonist
  (2 each) before adding colonists or hammers.
- Plymouth: Stockade done (1608 start), then Warehouse; pop 7 (1627).
- Caravel and scout: scout (Seasoned Scout, 50 horses) visited the Iroquois
  village at (38,26) for 128 gold (1573).

### 1640-1654: Docks mistake, reload, the food loop

- 1640: a stray `key Escape force` on the map quit the game (CN6-B1);
  reloaded `trace_autosave_turn_189.sav` and went on.
- Found the root cause of the starvation: no Docks in either colony, so
  fishermen made 0 food (CN6-N1). Plymouth Docks finished 1645, Jamestown
  Docks still stuck at 40/52 hammers with no lumber (lumber comes from
  forest tiles; the carpenters convert lumber to hammers).
- Plymouth bells: a colonist in the Town Hall gave bells 4 (1640). Town
  Hall work is the SoL engine (SoL 14% at Plymouth 1646).
- King's tax: 9% (1642), 11% (1649). Passage: 360 (1642), 420 (1654).
  Europe recruit lists change: statesmen did not show up again after 1585.
- Gold 4127 at 1654 (ore/silver/furs sold in London). Gold is not the
  bottleneck; hammers, lumber and food are.

### 1655-1688: Town Hall bells, Jamestown SoL to 60%

- Jamestown Town Hall (building index 9 in the colony screen `hit`): the
  statesman and idle colonists in it made bells 1 -> 13 a turn (1656).
  SoL 6% (1656) -> 25% (1665) -> 41% (1675) -> 47% (1680) -> 60% (1688).
- Plymouth SoL stuck at 14% (only 1 bell). Its Church (crosses 2), Stockade
  and Schoolhouse built 1670-1688. Note: a repeated build order on a finished
  building logs "already built one" each turn (Stockade, Warehouse): change
  the build after each completion.
- Plymouth docks finished 1655 (fish now feeds). Church done 1670.
- King's tax: 13% (1660), 21% (1687), 29% (1688, "Stamp Act" event, +8%).
- Congress: Franklin (1587), Stuyvesant (1670ish; "14/541 bells" for next),
  Peter Stuyvesant joined 1688; Jefferson picked 1688.
- Gold 3343 (1688) after recruits (Expert Silver Miners, Indentured Servant,
  Expert Fishermen on the dock). Fort (100 tools) and Printing Press (20
  tools) need tools; Europe Purchase list has no tools in the first page.

### 1703-1717: rebel sentiment 50%, the decision

- Jamestown SoL 64-72% (1703-1717) on 17-25 bells; Plymouth SoL 17-23%.
- 1717: LOG "Rebel sentiment is rising ... 50% of the population supports
  independence ... Some of the Founding Fathers think we should request an
  audience with the King and declare". Declaration is now available.
- Plan: declare after the defence is in place (Stockade at Jamestown, 3+
  artillery, soldiers from the Armory in Plymouth). Target around 1740-1750,
  well before the 1800 cutoff; the war then runs to 1850.
- Congress picks by auto `choose 0` again: Jefferson joined (1712), then
  Jakob Fugger (Trade) by the loop's choose 0 (1717). Process note, not a
  game bug.

### 1733: independence declared (turn 375)

- Declared from the GAME menu, "DECLARE INDEPENDENCE" (click 54 85 on the
  open menu), confirm "Yes! Give me liberty..." (MENU.TXT @GAME; the choice
  is the only way to declare, nothing declares by itself).
- Pre-declaration save: saves/pre_declare_1733.sav.
- Defence at declaration: Jamestown (SoL 80%, pop 12): 2 artillery, 1
  veteran soldier, 1 equipped soldier, Stockade. Plymouth (SoL 29%, pop 7):
  2 artillery, colonists; Armory, Stable, Church built.
- Gold 743. Crown tax 29%.
- The war: expeditionary force dispatched; Raleigh calls for volunteers.
  Plan: keep both colonies alive (losing all colonies ends the game), never
  destroy the crown land army (a 0-land-units win), run to 1850.

### 1734-1736: the war, Plymouth lost

- 1734: REF landed next to Plymouth (6 regulars, 2 artillery, cavalry,
  man-o-war). Our 2 artillery in Plymouth fought from the colony; the log
  shows Tories beating the rebel artillery and both were destroyed (1734).
- 1736 (turn ~380): REF marched into Plymouth ("Tories march into Plymouth!"). Our
  caravel 2101 with 2 units aboard was sunk/captured at sea by the Royal Navy
  (man-o-war) on the way from Plymouth to Jamestown ("Caravel captured at sea
  by the Royal Navy!"). Lost ~ 2 colonists, 1 pioneer (100 tools), 1 caravel.
- 1736: Jamestown alone: pop 13, SoL 79%, food +14 (stock 149), hammers +8,
  2 artillery, 2 soldiers, Stockade. Armory and Stable (horses) in
  progress. Gold 743. Game message: "Ships cannot sail to and from Europe for
  the duration of the War of Independence. Colonies which build Custom House
  can trade with European smugglers." No recruit, no sale, no artillery buy
  while at war unless Custom House (50 tools, none).
- Caravel 102 (fortified at Jamestown) is still loaded with 100 furs and
  cannot reach London.

### 1739: the end (game over, King wins)

- Turn 387, autumn 1739: Tory (King's) regulars and cavalry took Jamestown
  after two artillery were destroyed near it (the two soldiers were
  demoted to colonists and captured). Last colony gone, so the game ran
  COLONIZATION SCORE -> exploits -> Hall of Fame -> title menu.
- So the brief was NOT met: independence was declared (1733, turn 375) but
  the war ended in a loss at 1739, not at 1850.
- Result: lost. Score is in the Hall of Fame file (not read here).

## Final verdict

- Declared 1733 with 2 colonies: Jamestown (pop 12, SoL 80%) and Plymouth
  (pop 7, SoL 29%); 4 artillery, 2 soldiers, 743 gold.
- Crown landed 1734 at Plymouth and near Jamestown 1737. Our artillery died
  both times in the open or outside the colony (combat log "Tories defeat
  Rebel Artillery"). Plymouth fell by 1736 (turn ~380), Jamestown at turn 387 (1739).
- Root causes:
  1. Declared with a garrison far too small. Campaign 5 notes say a
     Fortress with 60+ guns and a stock of horses/muskets before declaring.
     I had 4 guns and 50 muskets.
  2. No trade with Europe during the war (game message), so no artillery
     purchases after the declaration; the 743 gold was all I could spend.
  3. The Plymouth caravel with 2 colonists and a pioneer was sunk by the
     Royal Navy at sea (1736) while evacuating.
- Economy before the war was too slow (Docks mistake CN6-N1, starvation,
  lumber shortage, Town Hall bells found late). The Town Hall bell trick
  came at turn 245+; with it earlier, SoL would have reached 50% well before
  1700.
- Not a bug: the King's tax rose to 29% (Stamp Act 1688) and silver/ore/fur
  sales dropped in price. The Crown is not a bug either: "Tories" in the log
  are the King's side.

## Branch 1 (new timeline from the pre-declaration save, 2026-10-26)

- Loaded `saves/pre_declare_1733` (turn 375, autumn 1733, gold 743). The
  lost timeline's saves (turns 375-387, COLONY08/09) moved to
  `saves/branch1_lost_1739/`. Turns before 375 remain shared.
- Crown tax at 58% in London (1734): a furs sale of 100 netted 42. Artillery
  purchase price rose 500 -> 900 (1734), so buying guns in Europe is no longer
  affordable. Preparation has to come from colony production (tools from
  ore at the Blacksmith's House, hammers from Lumber Mill / Carpenter's Shop).
- Building rows (driver `work N CI B`): 9 Town Hall, 10 Armory,
  21-23 Weaver's House, 24-26 Tobacconist's, 27-31 Rum Distiller's,
  32-34 Fur Trader's, 35-36 Carpenter's Shop, 37-38 Church, 39 Blacksmith's House.
- Unloading cargo worked with hand-paced drag: `mdown X Y`, `move X2 Y2`,
  `move X3 Y3`, `mup X3 Y3` onto the warehouse item (ore 125,186 at the colony
  screen). `drag` alone did not unload.
- Plymouth: `Fort` (100 tools) and Jamestown: `Fort` (120 hammers, 100
  tools). Tools come from ore at the Blacksmith (~4 tools per turn with 2
  workers).
- 1733-1772 (branch 1, prep): Forts in both colonies (Plymouth 1755,
  Jamestown 1768). Artillery in the colonies costs 192 hammers + 40 tools
  (docs/building_production.md:286), not 64 hammers. Hammers, not tools, were
  the bottleneck: lumber ran out in both colonies (the Carpenter's Shop eats
  lumber), so hammers fell to +6/turn. Ore for tools came from Plymouth
  (shipped by caravel, unloaded by hand-paced drag). Artillery 3 in Jamestown
  and 2 in Plymouth at 1772, Jamestown SoL 77%, Plymouth 19%.
- Pre-declaration save: saves/pre_declare2_1772.

### 1772-1776: second declaration (turn 453), lost again

- Declared 1772 from `pre_declare2_1772` (GAME menu, confirm Yes).
  Defence: Jamestown SoL 77%, pop 19, Fort, Stockade, 3 artillery, 2
  soldiers. Plymouth SoL 19%, pop 8, Fort, Stockade, 2 artillery.
- REF landed near Plymouth at turn ~455 and marched in the same turn cycle
  (turn ~457). Landed near Jamestown turn ~456; our artillery won several
  fights ("Rebel Artillery defeats Tories") but were then damaged and
  destroyed one by one. Soldiers were routed and demoted to colonists, and
  the colonists were captured. "Tories march into Jamestown!" at turn 460
  (1776). Game ended on the COLONIZATION SCORE screen.
- Final: lost at 1776 (turn 460), 4 years after declaring.

## Why the second preparation also failed

- Hammers were the bottleneck: artillery needs 192 hammers + 40 tools
  (docs/building_production.md:286). Lumber ran out (Carpenter's Shop
  eats lumber), so hammers fell to +6/turn in Jamestown and 0 in Plymouth.
  Ore for tools came from one mine (Plymouth) and ran out in Jamestown.
- Forts (120 hammers + 100 tools) took 30+ turns each.
- Europe purchases were out of reach: artillery 900 gold in 1734 (from 500),
  and Crown tax 58% on sales.
- The REF did not need many turns: 6-10 regulars and 2 artillery beat 3
  artillery + 2 soldiers in a colony with a Fort and a Stockade.
- Campaign 5 (Netherlands) won the same fight after a 40-turn build-up of
  around 60 guns in a fortress. That needs a much bigger economy than this
  England colony reached with lumber and ore shortages.

## Suggested next step (not done)

- Reload an earlier save (about 1700, turn 340, before the Crown tax and the
  Plymouth SoL drop) and build the lumber and ore chain there:
  Lumber Mill in both colonies first, a second Blacksmith worker per ore
  source, Town Hall in Plymouth before 1720, and Europe artillery bought
  while the purchase price is 500.
