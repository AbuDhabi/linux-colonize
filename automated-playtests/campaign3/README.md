# Automated playtest, campaign 3

Goal: win the War of Independence on **Conquistador** (moderate, difficulty 2)
as **France** (leader Jacques Cartier), playing the port headlessly.

**Result: independence won in spring 1816** (turn 540, declared autumn 1786),
score 600 (400 points x 1.5; the Dutch had already won their own independence
around 1700, so the recognition bonus was +50%, not +100%).

Bugs and oddities: [bugs.md](bugs.md). Screenshots: `shots/` (2x PNG).
Saves: `saves/` (`trace_autosave_turn_NNN.sav` every turn, manual `tNNN.sav`
whenever the driver was restarted).

## How it is played

Same harness as campaigns 1 and 2: `build/debug/play_driver` driven by
`tools/play.py start|send|stop automated-playtests/campaign3`, `settings.json`
with `debug.trace_autosaves` and `debug.logs` (diagnostics in `bin/opencol.log`,
older runs `bin/opencol.N.log`). New game started with `--seed 1789`; every
later restart used `--load saves/tNNN.sav` (no seed, so rumour and other 04ca
reseeds come from the wall clock).

Driver additions this campaign (tools/play_driver_main.c):

- `choose N`: answer the open choice popup with row N (arrow keys, then
  Return; Returns through a woodcut in front; presses again while a portrait
  popup is still ignoring keys). `CHOICES:` lines now end with `(sel N)`, the
  highlighted row. Reason: a bare `key Return` takes the highlighted row, and
  arrow / keypad keys sent while a popup is up move that highlight, which is
  how the very first Arawak peace offer got answered "No" (reloaded).
- `res [R [X Y]]`: seen special resources (fish, ore, silver, ...) around a
  tile; the ASCII `map` hides them under its overlay letters.
- `colony N` takes the `colonies` index and looks up the colony id (it used
  to pass the index as the id: C3-B3).

Driving notes:

- `activate ID` clears a Go To order, so `activate ID` + `goto ID X Y` +
  `auto` (scratch helper `go`) moves a unit now; a plain `goto` only marks it
  and the unit moves at end of turn.
- A unit waiting for orders blocks `endturn`; Fortify (`f`) parks wagons and
  ships too. Sentry does not stick while foreign units stand next to the
  colony (they wake it every turn).
- Colony screen: units outside the colony stand at the fence (1x y=116,
  x=155..175; `hit` finds them); dragging a worker onto the fence (x=140)
  opens the "Select a Profession" menu (Pioneer / Soldier / Scout / Colonist);
  its first row keeps the current job, so leaving needs row 1 ("Colonist").
  Build menu rows: `(No Production)` is row 0.
- Completing a build zeroes the hammer pool (DOS FUN_364b_0114): switching a
  half-built Fortress to a Wagon Train threw away 128 hammers.

## Journal

### 1492-1510: landfall, Quebec

- Start: Caravel at (55,14) in the far north-east with a Hardy Pioneer
  (100 tools) and a soldier, **0 gold** (Conquistador). Land Ho at (52,14)
  after 3 moves, named New France (`shots/land.png`).
- The east coast is small islands; it took until turn 13 to reach real land:
  a strip of tropical forest and swamp at y 31-34 with Sioux camps and many
  fish resources off the coast.
- 1497: the first Arawak peace offer was answered "No" by stray keypad
  presses (driver issue above); reloaded the turn-5 autosave.
- 1505-1510: the Sioux (5 camps) and later the Arawak, Apache (8 camps) and
  Inca (9 cities) all offered peace. A Dutch colony (New Amsterdam, then Fort
  Orange) sits 5-6 tiles from where I settled.
- Spring 1510 (turn 18): the pioneer founded **Quebec** at (34,31), a swamp
  forest tile with two fish resources and an ocean inlet, a Sioux camp next
  door, and only three land tiles, two of them Sioux land. Centre tile gave 2
  food, so the pioneer cleared and plowed it (+40 lumber) while the soldier
  worked as carpenter.
- 1511: picked **Benjamin Franklin** (Dutch next door; joined 1549).

### 1510-1564: docks, scouting, Cibola

- Docks (1527) made Quebec viable: two fishermen on the fish tiles, food +10.
  A Master Carpenter came free from Europe by religious unrest; Lumber Mill
  1537, Printing Press, Stockade, Warehouse, Wagon Train.
- Money from nothing: 28 sugar sold for 139, which bought 69 horses; a Free
  Colonist with 50 of them became a Scout. The Sioux paid 343 for 50 horses,
  the Apache 725 for 100, the Arawak 496 for 77; the Apache sold tobacco for
  296 (after haggling) that the Arawak bought for 955. Haggling a second time
  with another village ended its trade ("our patience is exhausted").
- The scout spoke to every chief in reach (all "tales of nearby lands", one
  "guides", which made him a Seasoned Scout) and explored 8 rumours: 7
  "nothing but rumors" and one **Seven Cities of Cibola**, 4800 on a mountain
  at (16,23) far to the west (observation C3-O1).
- 1564: the scout founded **Montreal** (15,23) next to the treasure so the
  King's galleons would take it: Crown share 60% at Conquistador, **+1920
  gold**. Montreal is Inca land on every side, 2 food; it stays at two
  statesmen-ish colonists and reached SoL 80%.
- 1551: picked **Peter Minuit** (joined 1576: native land free), then Thomas
  Jefferson (joined 1597), Paul Revere (1608), Simon Bolivar (picked 1609).

### 1565-1615: recruits, Guadeloupe, the Dutch squat

- Recruits in La Rochelle cost 104-340 depending on crosses; bought a Veteran
  Soldier, Seasoned Scout, Expert Farmer, Free Colonists, a Master Distiller,
  a second Master Carpenter and a Hardy Pioneer (arrives with 100 tools).
  Bought a **Merchantman** (2000) in 1582.
- The Merchantman found a sea route from Quebec to Montreal through an inland
  sea west of Quebec, and carried Montreal's furs (95 for 442 after 7% tax).
- 1597: the Expert Farmer founded **Guadeloupe** (32,28), inland on two
  savannahs (one with a sugar resource): 8 sugar or 7 food from one man.
- Quebec: Fort (1594), Church, then the **Fortress** (320 hammers, 200 tools).
  Master Distiller makes rum from Guadeloupe sugar (rum 12-14 in Europe).
  Tory penalty at 10 colonists and SoL ~25%; two statesmen and Jefferson /
  Bolivar brought it to 40%+.
- 1601: an English dragoon captured my Wagon Train (50 horses) right after
  first contact and a peace treaty (bug C3-B1).
- From about 1595 a Dutch wagon train, dragoons, soldiers and later an
  artillery piece have been standing on all three of Quebec's land tiles:
  nothing of mine can leave Quebec by land for many turns at a time. Asking
  them to withdraw: "Our forces protect valid Dutch interests and shall not be
  moved." (observation C3-O2). The Dutch also asked me to cancel my treaty
  with England (refused).
- The Sioux camp next to Quebec reached friction 103 ("do not trust the men
  in your ships"); a gift of 56 rum reset it to 0, and a Jesuit missionary now
  runs a mission there.

### 1615-1698: the long middle — blockades, boycotts, guns

- Quebec reached SoL 100% (1645) and builds artillery from its two Master
  Carpenters (32 hammers a turn) whenever it has 40 tools; Guadeloupe got a
  Lumber Mill, Armory and a blacksmith and builds a gun every ~15 turns.
  Founding fathers: Thomas Paine, Jan de Witt (picked by a stray Space on the
  Congress popup), Adam Smith, George Washington (1665), Peter Stuyvesant
  (1675), Jakob Fugger (1687), then de Soto.
- **Land logistics broke down.** Dutch and Spanish units (border-park soldiers,
  dragoons, artillery, plus my own captured wagon, observation C3-O2) stand
  on almost every land tile next to Quebec, Guadeloupe and Cayenne for
  decades; the Spanish and Dutch fight each other around Plymouth and destroyed
  the Sioux camps next to Quebec. Wagons and colonists can rarely walk
  between my colonies, so everything goes by ship.
- 1633: founded **Cayenne** (32,25) as a port for Guadeloupe. It became the
  tool shop: an Expert Ore Miner on its mountain (10 ore) and blacksmiths
  (8 tools a turn). The Merchantman carries 100 tools at a time round the
  north of the continent to Quebec (4 turns each way).
- Ships keep getting mauled: a Spanish frigate (no contact, no war
  declaration) damaged the Merchantman, the royal frigate (accepted 1626 with
  a +10% tax) and the Caravel with 147 rum; a Dutch privateer later sank 100
  coats on the frigate.
- Taxes: the King raised them roughly every 10 turns, 25% in 1631 to 62%
  by 1696. Parties: rum (1636), sugar (1643), tools (Cayenne 1681, 100 tools
  lost), rum again (1689), sugar (Montreal 1696). Lifting the first rum
  boycott would have cost 5500. Fugger later lifted the boycotts.
- Natives: rum sold well at first (Arawak 607 / 100, Sioux 530), then every
  tribe was saturated; ships cannot trade with Dutch colonies at all (bug
  C3-B4). From 1680 a **Custom House** in Quebec sells coats / furs / rum.
- The colony BUY button tops up missing tools cheaply (5 tools for 55, a whole
  gun's 40 tools for 330-440), which became the main use of gold.
- 1682: the game re-seated every Quebec worker during the Dutch turn
  (observation C3-O3); fixed by hand.
- Mistake worth noting: a gun waiting for tools keeps stacking hammers, and
  completion zeroes the pool, so Quebec threw away ~700 hammers twice.
- 1698 (turn 305): 15 artillery (14 in Quebec), 3 dragoons, a Fortress, SoL
  100% in Quebec and Guadeloupe. REF: 40 regulars, 15 dragoons, 14 artillery,
  8 men-of-war.

### 1698-1786: the gun factory, and the 300-unit wall

- Routine for ~90 years: Quebec builds a gun every 4-5 turns (42 hammers a
  turn), Guadeloupe one every 7-10; the Merchantman fetches ~100 tools from
  Cayenne every 9 turns; a Frigate fetches Montreal's coats for the Quebec
  Custom House; spare gold buys artillery in Europe (700, 800, 900).
- Taxes went to 75% (Quebec Food Party 1732, otherwise kissed the ring).
  Custom House income at 75% tax is tiny (100 coats net 125).
- The Inca would not trade with ships ("do not trust the men in your ships")
  after demanding 100 horses from Montreal, so horse sales were out.
- New founding fathers: La Salle (1746, free stockades), Magellan (1767).
- Spanish dragoons (about 15) now sit on every land tile around Guadeloupe and
  Quebec at peace; Guadeloupe's guns cannot leave (observation C3-O2).
- 1771: the Dutch AI rewrote Guadeloupe again (TB-14, C3-O3): five workers
  turned fur trappers, build queue Schoolhouse, hammers 0. Fixed by hand;
  Guadeloupe then built a Fort.
- **1765-1771: the 300-unit cap.** From turn ~430 the map held 300 units
  (Dutch 118, Spanish 93, me 66, natives 23). Each completed gun after that
  ate its hammers and tools and produced nothing: three guns lost before I
  noticed the @TOOMANYUNITS popups (observation C3-O5, DOS TB-47). AI
  nations, natives and the Crown may only create a unit while the map holds
  fewer than 292; the human up to 300. So I disbanded the Caravel, both
  wagon trains, a scout and seven idle colonists, one per finished gun, and
  the guns came out again.
- 1786 (turn 481): **56 artillery** (43 in the Quebec Fortress, 13 in
  Guadeloupe behind a Fort), 3 dragoons, 2 frigates, the Merchantman.
  REF: 56 regulars, 19 cavalry, 14 artillery, 9 men-of-war. The map is full
  (300 units). I expected that to stop the Crown from landing (it needs fewer
  than 292), but the declaration removes every Spanish and Dutch unit (see
  below), so it never mattered.

### 1786-1816: the War of Independence

- **Autumn 1786 (turn 481): declared** (`saves/t481.sav` is the turn before;
  `shots/declaration.png`). Rebel sentiment was 100% in Quebec, Guadeloupe and
  Montreal and 27% in Cayenne. The declaration withdrew Spain and the
  Netherlands and **deleted all their units**: the map went from 300 units to
  90 and the Spanish squatters around my colonies vanished. The veteran
  dragoon became Continental cavalry; Spain offered to intervene at 5000 bells.
- 1787-88: the REF landed next to **Cayenne** (no garrison, SoL 27%) and took
  it in one turn, then took **Montreal** (no garrison, harassed by the Inca).
  From then on the win needed fewer than 8 REF land units.
- 1788-1806: every later wave (up to six units, limited by the Man-O-War pool)
  landed on the two forest tiles north of Quebec and attacked the Fortress.
  Odds for the REF: regulars 40%, cavalry 45%, artillery 49% against healthy
  guns (Fortress +200%, SoL 100%); 52-56% against damaged guns. Guadeloupe's
  Fort gave 46-51%. One lone REF cavalry next to Guadeloupe won four 51% rolls
  in a row there.
- Sorties: Quebec's dragoons and Continental cavalry attacked only REF
  artillery standing alone (78-86%: artillery in the open is quartered unless
  fortified) and later lone regulars (44-51%). A lost sortie costs 50 horses,
  not a unit: re-equipping in the colony makes the dragoon again.
- 1800: Quebec ran out of healthy guns (all 44 damaged). The Spanish
  squatters were gone, so Guadeloupe's 15 guns walked to Quebec in one turn
  over the AI-built roads. Two more guns came from BUY (520 gold each) and
  Guadeloupe kept building.
- 1807: the REF pool was empty (`expeditionary_force` [0,0,1,0]). Over the war
  the Crown made 183 attacks on my units and won 83.
- **1809: intervention.** The bell pool reached 5000 after 28 turns (106 bells
  a turn). Spain landed at Quebec over three turns: 5 men-of-war, Continental
  Army and cavalry, and artillery, all under my control. From then on my
  attacks on colonies got Bombard +50%.
- **1811: Cayenne retaken.** Continental cavalry rode from Guadeloupe and
  attacked at 49-62% (Bombard +50%, Rebel Unrest +6%); three artillery
  attacks at 49-57% all failed. A Tory uprising (6 soldiers and dragoons)
  then appeared round Cayenne. Cavalry attacking them from open ground gets the
  **ambush bonus** (+150% on hills/mountains: 80-90%) and turns soldiers into
  colonists that the next attack captures.
- **1812-15: Montreal by sea.** Five men-of-war loaded 24 units at Quebec. Two
  of them shared a tile next to the Tory Man-O-War, which sank both in one
  fight (DOS 0ec0 sweep: every ship on the losing tile takes the loss, and
  with no drydock a beaten rebel ship always sinks during the war). The other
  three landed 17 units on the mountain next to Montreal; two REF regulars
  died attacking it. Next turn healthy artillery attacked at 64-74% (Bombard
  +50%, Rebel Unrest +37%) and the Continental cavalry finished it:
  **Montreal retaken in spring 1815** (`saves/t539_montreal.sav`).
- With no Crown colony, no Crown regulars, cavalry or artillery on the map
  (Tory colonists and scouts do not count) and an empty pool, the Crown gave
  up at the end of the turn: "Royal Expeditionary Force annihilated!"
  (`shots/victory.png`, score `shots/end3.png`).

## What decided it

1. **Guns, and the 300-unit cap.** 56 artillery at the declaration, plus 2
   bought and about 8 from Guadeloupe during the war. Three guns were lost to
   the unit cap (C3-O5) before I noticed, and every later gun needed a
   disbanded unit.
2. **Fortress, SoL 100%, no Tories in Quebec.** The REF's odds stayed at 40-49%
   against healthy guns, so ~100 of its 183 attacks failed and each failure
   killed a regular or damaged an REF gun.
3. **Pull Guadeloupe's guns in when the field clears.** The declaration's
   removal of the other Europeans opened the roads; 15 extra guns arrived
   just as Quebec ran out of healthy ones.
4. **Retake with cavalry and Bombard.** After intervention, cavalry and
   healthy artillery retook both colonies at 49-74%; damaged guns are poor
   attackers (49-55%, and they die when they lose).
5. Mistakes: hammers wasted waiting for tools (twice, ~900 hammers); the lost
   guns to the unit cap; two men-of-war on one tile; a stray horse-script that
   turned a Continental unit into a missionary for a turn.

Driver/scripts used for the war (scratch): `sortie` (attack Crown tiles next
to Quebec holding only artillery or 1-2 units with the colony's cavalry),
`rehorse` (re-equip dismounted Continental Army / soldiers as dragoons; the
role menu opens on the unit's current role, so it clicks the Dragoon row),
`loadmow` (sentry six units, then move the ship so they board), `uc` (unit
count and REF pool from the latest trace autosave via sav_json).
