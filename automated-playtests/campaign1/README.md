# Automated playtest, campaign 1

Goal: win the War of Independence on **Discoverer** as the **Netherlands**
(leader Michiel De Ruyter), playing the port headlessly.

## How it is played

- `build/debug/play_driver` (tools/play_driver_main.c) runs the game on the
  input-replay platform of `tests/regression` (no window, no audio) and reads
  commands from stdin: keys, clicks, drags, waits, screenshots, saves and
  state dumps (`status`, `units`, `colonies`, `map`). Every popup / ticker text
  is echoed as `LOG:`, open popup choices as `CHOICES:`.
- `tools/play.py start|send|stop automated-playtests/campaign1` drives it in
  the background (commands appended to `cmd.txt`, output in `out.txt`).
- `settings.json` here turns on `debug.trace_autosaves` (one
  `saves/trace_autosave_turn_XXX.sav` per turn) and `debug.logs`; the
  diagnostics log is `bin/opencol.log` (older runs: `bin/opencol.N.log`).
- New game started with `--seed 1607`. Manual saves (`saves/tN.sav`) were taken
  whenever the driver was restarted after a tooling change; the driver then
  resumed with `--load`.
- Screenshots: `shots/*.png` (2x). `out.txt` is the driver transcript of the
  last run; `bin/opencol*.log` are the diagnostics logs of every run.

Files: `journal` below, bugs in [bugs.md](bugs.md).

## Journal

### 1492-1497: landfall and New Amsterdam

- Start: Merchantman with a Pioneer (100 tools) and a Veteran Soldier at
  (46,56), facing a dark ocean. Three moves west: "Land Ho!", named New
  Netherlands (`shots/t0a.png`).
- The coast is a chain of small forested islands crowded with Arawak villages
  (three villages within 6 tiles). Sight radius 1 makes coasting slow.
- 1496: landfall on a plains-hills tile at (32,57) between a fishery (31,57)
  and a mountain (32,58), next to an Arawak village (31,58). The Arawak (5
  villages) offered peace and the land as a gift (`shots/t4c.png`).
- 1497: founded **New Amsterdam** on the hills. Selected the soldier and
  pressed B, but the game consumed the *pioneer* instead (bug B1). Worked
  around it in the colony screen: the soldier took the 100 tools and became
  the pioneer, his 50 muskets went to the warehouse; the old pioneer works as
  a fur trapper.
- 1498: Continental Congress: picked **Thomas Jefferson** (bells).
- The Merchantman's "Go to port" order took it north to the high seas at
  (30,49), not east as I expected; fine.

### 1499-1500: first Europe run

- A Master Carpenter appeared on the Amsterdam docks by religious unrest.
- Recruit prices 140, 160, 160: recruited an Expert Farmer, a Free Colonist
  and a Veteran Soldier (arrived as a dragoon, muskets + horses). Gold
  1000 -> 520. Dock units board the next ship automatically
  (`European dock options: Don't get on next ship`).
- Prices in Amsterdam 1500: horses 1/2, tools 1/2, muskets 2/3, trade goods
  1/2, furs 5/7, silver 19/20, cloth/coats 12/13.

### 1501-1525: two islands, two colonies

- 1502: the four colonists landed at New Amsterdam: carpenter in the carpenter's
  house, free colonist as lumberjack on the conifer tile (6 lumber), expert
  farmer on the cleared plains, the veteran-turned-fur-trapper as statesman.
  Build queue: Docks (done 1511), then Lumber Mill (done ~1520).
- The pioneer cleared and plowed the SE plains (20 lumber bonus), then joined
  the colony as the fishery fisherman (7 food).
- 1505-1507: the Merchantman explored west. All ocean to x=22, then a second
  island at (16-20,57-60) ringed by six fish resources; further west, more
  land at x=4-9 with several hills.
- 1507: Jefferson joined (+50% statesman bells). 1508: picked Peter Minuit
  (joined 1521). 1522: picked William Brewster.
- The ship's "Go to Amsterdam" from the west island sailed to the western
  high seas (x=0-1), a long way round.
- 1512 Europe run: recruits now 200-220. Elder Statesman + Free Colonist.
- 1517: landfall on the west island's prairie-hills-with-river tile (17,58):
  the Inca (4 cities) offered peace. 1518: founded **Fort Orange** with one
  colonist (statesman; every land tile is Inca land until Minuit joined).
- 1521: the elder statesman joined New Amsterdam's town hall (15 bells with
  two statesmen). I meant to drop a free colonist on the north ocean tile but
  the drag picked the fortified dragoon (they stand together at the gate), so
  the veteran became a fisherman and his muskets and horses went to the
  warehouse (100 muskets, 50 horses). The free colonist became the
  blacksmith: ore had hit the 100 cap.
- 1525: sold 100 ore (500) and 15 furs (75). Recruited a Master Blacksmith
  (240) and a Free Colonist (260); a Master Gunsmith came by immigration.

### 1526-1557: buildings, Fort Orange grows, rebels at 50%

- New Amsterdam built a Printing Press (1527, instant from stored hammers),
  then by an off-by-one in the build list a Stockade (meant Armory), then
  the Armory (1546), a Warehouse (1555); now on Artillery. A Master
  Blacksmith replaced the free-colonist blacksmith (ore was capped at 100),
  the free colonist became the third statesman (28 bells), later a gunsmith.
- 1531: picking "Fort Orange" in the port list from New Amsterdam selected
  Amsterdam instead (the current port is greyed and skipped, so my Down
  count was off). The ship was half way to Europe; dragging it from
  "Expected soon" to "Bound for" in the Europe screen reversed the voyage.
- Founding fathers: Brewster (1528), Adam Smith (1534), Stuyvesant (1542),
  Paul Revere (1550); picked de Soto next.
- 1536: the King raised tax to 1%; kissed the ring.
- 1537-1554: Fort Orange got a lumberjack (gunsmith on prime timber: 10
  lumber), a second carpenter, an expert farmer on the cleared river tile,
  and the pioneer finally joined as a fisherman. Docks done 1552, Lumber Mill
  next.
- 1544: population growth in New Amsterdam (free colonist -> second
  lumberjack). An expert fisherman on the fishery makes 14 food.
- 1546: **Spain ceded all its colonies to England** (Treaty of Utrecht
  event). Rebel sentiment hit 50%; the founding fathers suggest declaring.
  The REF is still small: 15 regulars, 5 cavalry, 2 artillery, 2 men-of-war
  (save head `expeditionary_force`).
- 1546: the veteran soldier left the fishing boat and is a dragoon again
  (50 of the 100 horses), fortified in New Amsterdam.
- Win condition (src/core/ai_king_war.c `ai_king_woi_end_win`): the Crown
  holds no colony, has no Regular/Cavalry/Artillery left on the map (fewer
  than 8 once it has captured a colony) and its unsent pool scores below 4
  (regulars + 1 if any cavalry + 1 if any artillery).

### 1559-1599: Inca gold, artillery, scouting

- A Seasoned Scout arrived by immigration (Brewster choice). Speaking with
  chiefs paid off hugely: two Inca cities next to Fort Orange gave
  **1344 gold each** (1564, 1566), a third Inca city on a tiny western island
  1056 (1580), two Tupi camps on the north-western island 336 each. Two lost
  city rumours south of Fort Orange each gave a colonist ("survivors of a
  former colony"); one near New Amsterdam gave nothing.
- With the money: 4 artillery bought in Amsterdam (500, 600, 700, 800 —
  the price rises 100 per purchase), plus a master carpenter, two expert
  farmers and free colonists (recruit price swings: 140-160 after crosses
  build up, then jumps to 360-400).
- New Amsterdam reached 100% Sons of Liberty (1573), built its first
  artillery in 1569 and keeps building them (5 by 1594). Fort Orange: Lumber
  Mill, Printing Press, Stockade, Warehouse; Custom House waiting for tools.
- Founding fathers: de Soto (1559), Pocahontas (1567), Franklin (1577),
  Thomas Paine (1585), Penn (1594); picked Jan de Witt next. Tax 5%.
- 1581: the English (who absorbed Spain in 1546) founded Roanoke 3 tiles
  south of New Amsterdam; first contact, demarcation treaty, peace.
- 1592: met the Tupi (7 camps) on the north-western island.
- 1596: an Inca brave attacked the pioneer clearing forest at Fort Orange;
  the pioneer won. During that native turn the Combat Analysis dialog runs a
  nested input loop, which deadlocked the play driver (it could not read the
  next command); fixed in the driver by pressing Return after 300 idle polls
  inside one update. The turn was replayed from
  `saves/trace_autosave_turn_104.sav`.
- REF unchanged since 1544: 15 regulars, 5 cavalry, 2 artillery, 2
  men-of-war. Overall rebel sentiment 64%.

### 1600-1613: declaring independence, and losing Fort Orange

- 1600: two turns a year from now on. A veteran soldier was hiding in New
  Amsterdam's armory as the gunsmith; re-armed as a dragoon. Spare
  colonists became dragoons too (horses had piled up to the 200 cap).
- 1603: last Europe trip: 1 more artillery (900; Train list: veteran
  soldiers 2000, elder statesman 1900 - unaffordable).
- 1606-1610: Fort Orange got 100 tools by ship and built its **Fort** (1610),
  Custom House (exports ore), Armory queued. **Simon Bolivar** joined in
  1610 (+20% SoL everywhere): Fort Orange ~72%, New Amsterdam 100%, nation
  84%.
- 1611 (turn 130): **declared independence** (`saves/pre_declare_1611.sav`).
  Forces: New Amsterdam 6 artillery + 5 dragoons (2 veterans), Fort Orange
  4 artillery + 1 dragoon; REF 15 regulars, 5 cavalry, 2 artillery, 2
  men-of-war. The two veteran dragoons became Continental Cavalry at the
  end of the declaration turn. France will intervene after 2000 more bells.
- The REF (playing in the old Spanish slot, nation 2) landed 6 units next to
  Fort Orange at once, then 6 more the next turn. Combat lessons from the
  log:
  - REF attacks on Fort Orange: Regulars 112 vs my artillery 120 (48%),
    Cavalry 135 (52%), Artillery 157 (56%). Attack bonus +50%, "Bombard"
    +50% and Tory unrest +25% for them; Fort +150% for me.
  - My artillery attacking out of the colony gets **"Artillery In Open
    -75%"** (40% odds): artillery is for defence only.
  - Dragoon attacks: 80 vs Cavalry 48 (62%, won: cavalry demoted to
    regulars), 80 vs Regulars 60 in forest (57%, lost: dragoon demoted).
- 1612-1613: two REF assault turns destroyed all four Fort Orange artillery
  for 6 regulars lost; **Fort Orange fell** in 1613. The Merchantman fled,
  but its Go To New Amsterdam route ran straight back past Fort Orange and
  the captured fort's coastal guns **sank it**. New Amsterdam is now an
  island with no ship.
- Win condition now needs Fort Orange back (crown must hold no colony) and
  fewer than 8 crown land units on the map (the bar rises after a capture).

### 1613-1615: attempt 1 collapses; reload

- The REF next landed 6 units beside New Amsterdam (whose Fort was 1 tool
  short). In two turns they damaged or destroyed 5 of the 6 artillery while
  losing 3 regulars; my Continental Cavalry (68%) and Continental Army (62%)
  counterattacks both lost. Because Activate only reaches the first unit of
  the colony stack (bug B8) and the turn ends as soon as no unit is pending,
  I could make only one counterattack per turn.
- State at 1615: REF pool 2 regulars + 2 cavalry, ~9 REF land units on the
  map, Fort Orange in Crown hands, no ship to retake it. Rather than grind
  this out I archived attempt 1 (`saves/attempt1/`, including the trace
  autosaves of turns 130-139) and reloaded `saves/pre_declare_1611.sav`.

Lessons for attempt 2:
1. Keep the ship far from any colony that can fall; never Go To past a fort.
2. A Fort (+150%) makes artillery roughly a coin flip against REF regulars;
   aim for a Fortress and 100% SoL (Tory unrest adds up to +25% for them).
3. Leave counterattack units **without orders** (not fortified) so the unit
   cycle offers each of them; fortified units in a colony are unreachable.
4. Demoted dragoons can re-equip from the colony's horses: stockpile horses
   and muskets.

### Attempt 2, 1611-1648: preparing properly

Reloaded `saves/pre_declare_1611.sav` (turn 130) and spent 37 more years
getting ready instead of declaring at once.

- New Amsterdam: Fort (1616), **Fortress** (1624) after shipping in tools,
  then artillery after artillery. Two extra lumberjacks/carpenters pushed
  hammers to 26 a turn.
- Fort Orange: Armory, artillery, Ore Party (refused a tax raise; the custom
  house kept selling despite the boycott), 100% SoL (1626), **Fortress**
  (1640).
- 1617 picked George Washington (winning units are promoted); 1623 John Paul
  Jones (a free Frigate, which arrived in Amsterdam, bought 100 muskets and
  came over). The King twice added units to the REF while I waited (one
  artillery, one man-of-war) and raised tax to 22%.
- Last Europe trips: 200 tools, 187 muskets. Ships parked in New Amsterdam.
- Added an `activate ID` command to the driver to get around bug B8 (it calls
  the port's own `game_click_activate_unit`, i.e. what picking from a stack
  list does).
- Forces at the declaration (Autumn 1648, `saves/pre_declare2_1648.sav`):
  Fort Orange 10 artillery + 5 dragoons; New Amsterdam 9 artillery + 4
  dragoons (2 veterans, promoted to Continental Cavalry on declaring); both
  colonies fortresses at 100% SoL. REF 15 regulars, 5 cavalry, 2-3
  artillery, 3 men-of-war.

### 1648-1652: the War of Independence, won

- Every REF wave (6 units) landed on the two tiles south of Fort Orange: the
  grassland (17,59) and the conifer forest (16,59).
- Against the Fortress their regulars attacked at 39% (90 vs 230), cavalry
  43%, artillery 47%; each failed attack destroyed the attacker.
- Each of my turns I woke every dragoon (`activate`) and attacked the landed
  stacks: dragoons 80 vs regulars 40 on grass (66%), 60 in forest (57%),
  vs artillery in the open only 15 (84-92%). With Washington the winners
  became veterans and then **Continental Cavalry** (128 attack, 68-76%).
  Losers are only demoted and can re-horse in the colony.
- The Fort Orange fortress sank one man-of-war and sent another home damaged.
- REF losses per wave were 6/6 three times running; New Amsterdam was never
  attacked. When the last four landed units died the pool held only one
  cavalry.
- **Autumn 1652 (turn 213): "Royal Expeditionary Force annihilated!"**
  (`shots/win1.png`), the King's concession (`shots/win2.png`), the
  Independence Hall cinematic (`shots/win3.png`), Colonization Score 1108
  (`shots/win5.png`: citizens +100, congress +90, rebels +100, early
  revolution +264, independence +100%), rating 22%, "De Ruyter Mem. Bridge"
  (`shots/win6.png`), Hall of Fame 6th (`shots/win7.png`). Final save:
  `saves/before_win_1652.sav` (the turn before the victory check).

## Summary

- Won the War of Independence on Discoverer as the Netherlands in Autumn
  1652, 213 turns, on the second try at the war (attempt 1 lost Fort Orange
  and its only ship in 1613; attempt 2 reloaded the 1611 pre-declaration save).
- The economy never got big: two island colonies of 12-14 colonists, money
  mostly from Inca/Tupi chief gifts (~4,400 gold) and ore/fur sales.
- What won it: two fortresses at 100% SoL, ~19 artillery as defenders, and
  dragoons counterattacking every landed REF stack the turn it arrived, with
  Washington turning winners into Continental cavalry.
- Bugs and oddities: [bugs.md](bugs.md). The most important for play: B8 (only
  the first unit of a colony stack can be activated), B3/B6 (the unit cycle
  stalls after loading a save or leaving a colony screen), B1 (Build Colony
  uses the wrong unit).
