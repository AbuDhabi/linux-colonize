# Automated playtest, campaign 2

Goal: win the War of Independence on **Explorer** (second-easiest) as
**England** (leader Walter Raleigh), playing the port headlessly.

## How it is played

Same harness as campaign 1 (see ../campaign1/README.md):
`build/debug/play_driver` driven by `tools/play.py start|send|stop
automated-playtests/campaign2`. `settings.json` here has
`debug.trace_autosaves` (one `saves/trace_autosave_turn_XXX.sav` per turn) and
`debug.logs` on; diagnostics log `bin/opencol.log` (older runs
`bin/opencol.N.log`).

- New game started with `--seed 1776`; manual saves `saves/tNN.sav` whenever
  the driver was restarted (tool changes), resumed with `--load`.
- Unit ids are renumbered on every load: re-read `units` after a restart.
- Driver commands added this campaign (tools/play_driver_main.c):
  - `colonies` now prints, per human colony, the build, SoL, this turn's
    food/bells/crosses/hammers, every non-zero stock with its net change, and
    each colonist's profession and workplace;
  - `goto ID X Y` (Go To order, what a map goto drag sets),
  - `colony N` (open colony N's screen),
  - `rumours` (seen rumour tiles with terrain, hill / mountain),
  - `mdown X Y` / `mup X Y` (hand-paced drags),
  - `hit X Y` (colony-screen hit test: which zone a coordinate lands in).
- UI notes for driving: popups ignore keys for a few frames after they open
  (send `wait 10` first); a unit with Go To orders only advances when it is
  the active unit (or at end of turn); units in a colony board a ship by
  Sentry, the ship takes them when it leaves; Europe buttons answer to keys
  (R/P/T) more reliably than clicks; a purchase has a second Yes/No.

Files: journal below, bugs in [bugs.md](bugs.md), screenshots in `shots/`.

## Journal

### 1492-1502: landfall, Jamestown

- Start: Caravel at (45,14) in the far north with a Pioneer (100 tools) and a
  Veteran Soldier, 300 gold (Explorer). Land Ho at (38,13) after 3 turns
  ("New England", `shots/t1a.png`).
- The land is a small island (x 32-42, y 4-15) shared with the Arawak (14
  villages, peace and land offered at first contact).
- 1494: picked **William Brewster** at the first Congress.
- 1495: the soldier founded **Jamestown** on grassland hills at (37,14): two
  land tiles (conifer + river, mixed forest + beaver), the rest ocean, an Arawak
  village next door. His 50 muskets went to the warehouse.
- 1502: first London trip; recruits cost 160. Took a Seasoned Scout; an
  Indentured Servant was already on the docks.
- Pressing Escape right after sailing from London quit to the title menu
  (bug C2-B1); replayed from the turn-10 trace autosave.

### 1503-1523: Cibola, Plymouth, a second ship

- Food was the first problem: Jamestown's center gives 3 food and fishing
  needs Docks. Two carpenters, a lumberjack and a farmer kept it barely fed
  until the **Docks** (1518); then an expert fisherman on the east tile made 12.
- 1509: sold 40 furs + 40 ore (320); recruited an Expert Farmer; an Expert
  Fisherman arrived by immigration.
- 1513: the scout found one of the **Seven Cities of Cibola** on a mountain
  north of the colony: a 5800 treasure. It walked to Jamestown by Go To and
  no King's Galleon offer came (bug C2-B2); stepping out and back in by hand
  raised it. Accepted the Crown's 55% share: **+2610 gold**.
- 1515: the expert farmer founded **Plymouth** (inland, river) next to plains
  with wheat the pioneer had plowed: 12 food from one farmer.
- 1522: picked **Peter Minuit** (joined ~1543: the Arawak land around both
  colonies became free). Then La Salle (pending).
- 1523: London: bought a **Merchantman** (2000) and recruited 3 free colonists
  and a second scout (200-240 each).

### 1524-1548: slow build-up

- Jamestown: Lumber Mill (1530), Wagon Train (1534), Printing Press (1539),
  Stockade (1545); 2 then 3 statesmen; the Master Fur Trader makes coats from
  local and Plymouth furs; a free colonist learned Expert Fisherman at the
  Arawak village next door (1547).
- Plymouth: a farmer, two fur trappers / lumberjack, grows a colonist every
  ~12 turns from its food surplus; a wagon hauls furs, tobacco, lumber to
  Jamestown.
- 1542: second London run: 53 furs, 72 ore, 80 tobacco sold (~870); bought
  100 tools, recruited a Master Gunsmith and a free colonist.
- Both scouts explored the mainland to the south (Spanish Isabella at (48,24),
  French Montreal / Quebec at (49,36) / (50,41), Cherokee and more Arawak).
  First contacts with Spain (1529) and France (1538): peace treaties. Chiefs
  only ever told tales of nearby lands.
- Of 10 rumours explored, 9 were "nothing but rumors". Not bad luck: every
  rumour in a game is rolled from the same reseed (observation C2-O1).
- Tory penalty seen at Jamestown pop 10 with SoL 15% (hammers 12 -> 8, bells
  10 -> 7); more statesmen fixed it.
- The King raised taxes to 1% (1532) and added Regulars to the REF twice.

### 1549-1602: three colonies, coats, a thin army

- Founding fathers: La Salle (1552, free stockades), Henry Hudson (1574: fur
  output doubled, Plymouth +28 furs a turn), Pocahontas (1590), Thomas
  Jefferson picked next. Peter Minuit had freed the native land around both
  colonies.
- Jamestown: Warehouse, Fur Trading Post (1557, the Master Fur Trader makes 12
  coats), Armory, Newspaper (1570), **Fort** (1578), first artillery (1591).
  Recruits: Master Carpenter, Expert Lumberjack (Brewster pick), free
  colonists, a Master Gunsmith (muskets from tools). An **Elder Statesman**
  trained in London for 1900 (1600). Pop 13, SoL 43%, 26 bells a turn.
- Plymouth: Lumber Mill, Printing Press, Warehouse; two ex-scouts became
  statesmen; the 100 scout horses breed there (128 by 1602). Pop 8, SoL 24%.
- Money: coats, furs, ore and tobacco shipped by the Merchantman, ~1000-2500
  per London trip. Selling 72 coats to the Arawak village gave only 340
  (London paid ~10 a coat then). The coat price fell from 12 to 6 by 1600 as I
  kept selling; furs held at 5.
- Recruit prices swung between 190 and 500 with crosses.
- 1580: Arawak braves ambushed and killed a colonist walking north to found a
  third colony (the Arawak had lost 6 of 14 villages to the Spanish and owned
  150 muskets); in 1589 they attacked the pioneer, who won. Pocahontas (1590)
  calmed them. Soldiers now guard both colonies.
- 1596: founded **Roanoke** on plains hills at (40,6), north coast.
- Food is the hard cap: Jamestown has two land tiles; at 13 colonists it runs
  at 0 to -5 food. Moving colonists out was the only fix.
- 1602 (Continental Congress report, `shots/r4.png`): rebels 31%; the REF is
  **26 regulars, 10 cavalry, 8 artillery, 5 men-of-war**.

### 1603-1624: Fortress, Bolivar, arming

- Founding fathers: Thomas Jefferson (1604), Simon Bolivar (1610: +20% SoL
  everywhere, rebels jumped to 69%), John Paul Jones (1615, a free Frigate in
  London), Adam Smith (1620), Benjamin Franklin picked next.
- Jamestown: 50% SoL in 1605 (+1 production), 100% in 1613 (+2). The **Fortress**
  finished in 1613 (320 hammers + 200 tools; I shipped the tools in two holds
  and unloaded the second one "anyway" past the warehouse limit the turn it
  completed). Since then it builds artillery (192 hammers + 40 tools, one every
  ~6 turns at 34 hammers).
- Plymouth: Newspaper, Fort (1604), Armory (1619), then artillery at 10
  hammers a turn. Roanoke: Docks, Lumber Mill.
- 1605: refused a tax raise with a "Jamestown Coats Party" (coats had dropped
  to 5 and were worthless to me); later Navigation Acts took tax to 11%.
- The coat market collapsed (12 -> 5) and furs fell to 4; switched the Master
  Fur Trader to cigars (17-20), but the tobacco supply was too small.
- Arming: the idle tobacconist / blacksmith / gunsmith became soldiers (muskets
  had hit the 200 cap), then dragoons with Plymouth's horses. Bought artillery
  at 500, 600, 700, 800, 900 (the price rises 100 per purchase), horses at 4.
- The Arawak turned hostile again in the 1620s: they attacked the wagon train
  (it won) and raided Jamestown (wiped out by artillery).
- 1624: rebels 80%. Army: 11 artillery (9 Jamestown, 2 Plymouth), 3 dragoons,
  2 soldiers; 200 horses and a colonist on the way. REF still 26 regulars, 10
  cavalry, 8 artillery, 5 men-of-war.

### Attempt 1 at the war, 1635-1636 (abandoned)

- Spring 1635 (turn 178): declared independence (`saves/pre_declare_1635.sav`;
  the turn-177 save `saves/pre_declare_1634.sav` is the reload point). Rebels
  84%. The Frigate and Caravel waiting in London were seized. The veteran
  dragoon at Jamestown became Continental cavalry.
- The REF only ever lands next to **coastal** colonies, weakest first
  (src/core/ai_king_ref.c FUN_43f7_0982: colonists x (125 - SoL) - 75 x
  strength). Plymouth is inland and was never a target; Roanoke (6 colonists,
  SoL 34%, stockade) was. Three waves of 5-6 landed around it in three turns.
- Combat log (`bin/opencol.log`, COMBAT lines): REF Regulars attacking
  Roanoke 5 -> 144 (Attack +50%, Bombard +50%, **Tory Unrest +61%**) against my
  artillery 5 -> 103 (Stockade +100%, Fortified +50%): 58% for them, 69-73%
  against damaged artillery. My dragoon sortie (3 -> 39 with the attack bonus)
  against Regulars on open plains (5 -> 40) was 49% and lost (demoted).
- After two REF assault turns: 2 of 8 artillery destroyed, most of the rest
  damaged, 4 REF Regulars dead, 15 REF units around Roanoke. It would fall
  within a turn or two, and a win needs the Crown to hold no colony, i.e. I
  would have had to storm a stockade held by fortified Regulars (base 5).
- Reloaded the turn-177 save (attempt saves in `saves/attempt1/`).

Lessons: the Tory-unrest bonus scales with the colony's Tory share, so every
coastal colony needs high SoL before declaring; Roanoke needs SoL, a Fort and
its artillery before the war.

### 1635-1645: second preparation

- Reloaded the turn-177 save and played ten more peaceful turns.
- Roanoke: two colonists moved out and statesmen in; SoL 92% then 100%; a
  **Fort** (1641); 9 of the 20 artillery and 3 dragoons moved in. Jamestown kept
  11 artillery, 4 dragoons and the veteran dragoon (Continental cavalry at the
  declaration); Plymouth 2 dragoons and a soldier.
- More artillery (built and bought); Plymouth's horses mounted the dragoons.
- REF before the declaration (`saves/pre_declare2_1645.sav`, turn 199): 28
  regulars, 10 cavalry, 8 artillery, 5 men-of-war.

### Attempt 2 at the war, 1645-1648 (lost Roanoke, abandoned)

- Declared in autumn 1645. The REF again landed only around Roanoke: 6-8 units
  a turn on three tiles.
- With SoL 100% the Tory bonus was gone. Regulars attacking the Fort were 46%,
  cavalry 51%, artillery 55% (Attack +50%, Bombard +50% against Fort +150%).
  Fortify adds nothing on top of a Fort.
- 1647: three dragoon sorties against Regulars on open plains, 49% each, all
  lost (dragoons demoted to soldiers). Roanoke's guns were all damaged by then,
  and a damaged gun defends at 3 instead of 5. The next assault was 58-66% for
  the REF: six guns destroyed in one turn.
- Spring 1648: Roanoke fell (`shots/war2_roanoke_falls.png`). Score: 9 REF
  regulars killed and 3 cavalry demoted, against 9 artillery and 3 dragoons
  lost. 25 REF land units were ashore and 17 still in the pool, against 11
  artillery and 8 horse. That only goes one way.
- Saves in `saves/attempt2/`.

What I learned from reading the code (src/core/combat_strength.c,
ai_king_war.c, docs/difficulty.md):

- Every exchange is about 50/50, and the REF has about 64 "lives" (Regulars
  die on a lost attack, Cavalry demote first, artillery is damaged first). A
  20-gun army cannot grind that down alone.
- **Foreign intervention** is the lever. After the declaration the bell pool
  starts again from 0. At `2000 + 1500 x difficulty` bells (3500 on Explorer)
  the ally announces, then lands 6 veteran Continental units each turn for
  several turns (player-controlled). From then on, our attacks on colonies
  also get +50% Bombard.
- Attacking a colony during the war adds the colony's SoL to the attack
  ("Rebels"), so retaking 100%-SoL Roanoke is much easier than holding it.
- Once the Crown has captured a colony, the win needs only fewer than 8 REF
  land units on the map (plus an empty pool and no Crown-held colony).
- In the open, during the war, the defender's terrain bonus goes to the
  *attacker* (ambush) unless the defender is fortified. Artillery attacking
  units in the open takes -75% unless the defender is fortified.

Plan for attempt 3 (from `pre_declare2_1645.sav`): before declaring, raise bell
output as far as it goes. Then hold Jamestown, the only port that matters (losing
every port loses the war), in its Fortress with all the guns. Retake Roanoke
once the intervention arrives.
