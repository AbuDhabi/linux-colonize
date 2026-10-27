# Automated playtest, campaign 8

Brief (user): England on **Discoverer**, headless port. Declare independence
before the 1800 cutoff (turn 507), then keep the War of Independence going
without winning or losing until the game auto-ends in 1850. Record bugs.

Harness: `build/debug/play_driver` via `tools/play.py start|send|stop
automated-playtests/campaign8`. New game `--seed 8008`, England, Discoverer
(wizard: Return, click 161 50, click 57 83, click 155 50, click 55 184, Return,
Return, Return, Return). Trace autosaves: `saves/trace_autosave_turn_NNN.sav`.
Log: `bin/opencol.log`. Bugs: [bugs.md](bugs.md). Screenshots: `shots/`.
Turn helper: `helpers/turn.sh N` (endturn + auto + advise, N times).

Rules: [automated-playtests/RULES.md](../RULES.md) (advise GATE lines decide
the declaration; Discoverer REF is 23-30 land units, not the Viceroy 60+ guns).

## Journal

### 1492-1496: landfall

- Start (wizard seed 8008): Caravel (id 1) with Pioneers (100 tools) and a
  Veteran Soldier (50 muskets), 1000 gold. Ship sails west (arrow keys; `goto`
  refused at unexplored or inland targets, RULES "Driver quirks").
- 1495: "Land Ho!" name popup, pre-filled "New England", accepted with Return.
- 1496: Make Landfall (`choose 1`) on forest prairie at (37,30). The Arawak (14
  villages in the region) offered the land as a gift + peace: accepted (`choose 0`).
- Caravel sent to London by `key g` + `key Return` (the port list has one row;
  `choose 0` did not close it, Return did).

### 1497-1505: Jamestown (36,30)

- 1497 pioneer founded Jamestown (coastal, lumber, fish at 36,29; a native
  village 2 tiles south at 37,32 is in view). Starter buildings: Town Hall,
  Carpenter's Shop (no hammers without a worker in it).
- 1497 Congress 1: Benjamin Franklin (political; RULES: no AI European wars).
- 1499 London: recruited Master Carpenter (140) + Expert Farmer (140).
- 1502: Master Carpenter `work 1 2 35` (Carpenter's Shop): hammers 0 -> +6.
  Docks (BID 6) queued; Lumber Mill = BID 36 (campaign 6 row list).
- Food: soil is forest / swamp / savanna, no grassland: farmers only on tiles
  that yield food (Expert Farmer on W (35,30) job 0 gave food +4 -> +10).

### 1505-1514: scouting for gold (the big money)

- Seasoned Scout (London dock) brought over 1507 with 2 recruits.
- Arawak village at (37,32): `Ask to Speak With Chief` (`choose 0`) gave 928
  gold (1510: 520 -> 1448). Second village at (39,37): 928 again (1514: 1956).
  Villages are the cheap money on Discoverer.
- 1507-1541: furs (100 at 1536) sold in London for 490 net (Europe `sell 0 4`).
  Caravel carries 2 passengers, not 3 (1534 trip: Scout stayed on the dock).
- Map-wide: 3 Arawak villages gave 928/928/832 beads (the first visit only),
  Sioux and Tupi chiefs gave "tales" (no gold). A Spanish colony (New Spain)
  at (33,31) asked for a demarcation treaty: accepted (`choose 0`), then
  "go in peace" (`choose 0`).
- 1536 Plymouth founded at (40,30) by Pioneer ejected from Jamestown
  (`eject 1 0 1` = pioneer with 100 tools from colony stock). Its colony gets
  no hammers until a Master Carpenter comes from Europe (Plymouth's starter
  Carpenter's Shop is worked by a Pioneer, 0 hammers).
- 1537-1541: Jamestown hammers +12 (Lumber Mill, Master Carpenter), building
  Warehouse (1528), Custom House (1541, 160 hammers + 50 tools; tools were
  moved to Plymouth by the eject).
- King's tax raise 1536 (+2%): kissed the ring (`choose 0`).
- Gold 2838 at 1541 (scout villages + furs). Unspent: no build gold sink yet.
- 1544 Custom House built (Stuyvesant): furs now sell at the port by themselves
  (`Jamestown sells 52 Furs for 260`), no caravel trip needed.
- 1547 Europe: sold nothing new, trained a Master Carpenter (train 6, 1000
  gold) and bought a Merchantman (purchase 3, 2000). Gold 3925 -> 925. The
  Merchantman carried the Carpenter and a Free Colonist to Jamestown (1552).
  Caravel 187 stays in London.
- Chiefs: only the Arawak villages paid gold (4 visits: 928, 928, 832, 832).
  Sioux, Tupi and a Spanish settlement gave tales (no gold). Scouts cannot
  do more Arawak gold without finding new Arawak villages (none unvisited on
  the seen map; unvisited villages seen: (23,22), (27,22)).
- Unload trick: colony screen, `drag 134 170 275 186` moves hold 0 to the
  warehouse tools (hit positions: hold 134,170; tools 275,186; furs 82,186,
  all 320x200 logical coordinates, NOT the 640x400 screenshot pixels).

### Era plan (1552)
- Gates for 1790 (Discoverer, REF land 23-30): 20-24 artillery, 9-11
  dragoons, a Fortress in Jamestown and a Fort+ in Plymouth, SoL 100% both,
  100 horses, 50 muskets. Stable (17), Armory (3), Stockade (0) and Fort (1)
  next in Jamestown; Carpenter's Shop + Lumber Mill in Plymouth via the new
  Master Carpenter.
- Declare by 1790 at the latest (turn 497 is 1790; cutoff 1800 = turn 507).

### 1552-1610: Iroquois gold, Plymouth, tax rises

- Iroquois (13 villages, north-west) pay 928/832 beads like the Arawak
  (7 visits by 1572 -> gold 7711 at 1593). Sioux/Tupi/Spanish still gave tales.
  A second Seasoned Scout (Europe recruit 254) and Scout 310 explored north.
  Scout 111 disbanded 1593 (gold was no longer the limit).
- Jamestown: Custom House sells furs by itself ("sells 54 Furs for 270").
  Lumber Mill + Master Carpenter = +10 hammers/turn; Plymouth the same from
  1557 (Carpenter bought 1000 gold, Train list). Armory, Stable, Church,
  Fort (1592), Fortress (in progress, 320 hammers, 200 tools).
- Plymouth: Docks, Lumber Mill, Warehouse, Stockade, Fort (1599). Furs piled
  up there (no Custom House yet; queued).
- King: tax 2% -> 4% (1561-ish, kissed ring), 8% (1580, kissed), 10% offer
  (1603): "Hold Plymouth Furs Party" kept it at 8% and threw 100 furs into
  the sea (CN8-O12).
- Europe 1594-1610: 2 Master Carpenters (train 1000 each), 2 artillery (500,
  600 gold: purchase), tools 100 (buy 2 gold each). Gold 4300 at 1610.
- Merchantman (ship id changes each trip) cargo: `buy 0 14 100` (tools) in
  one hold, passengers 3 max. Unload by `drag` onto the warehouse tools
  icon: only the first 1-2 holds drag (CN8-O13).

### 1622: state and the SoL problem

- Gate blockers at 1622 (advise): Jamestown Fortress needs 200 tools (stock
  230 from the merchant, Fortress built the next turn), Plymouth Custom House
  needs 50 tools (none in Plymouth), Roanoke (38,29) founded 1616 (pop 2,
  Docks queued, food 0). Artillery 4 (467, 589, 590, 681). Dragoons 0.
- Horses 0: the scout's 50 horses left with the disbanded scout 111 (1593).
  Disbanding a scout throws away its horses. Keep horses in a colony.
- SoL 14-18% in Jamestown, 6% Plymouth. Gate needs 100%. Mechanics:
  docs/sons_of_liberty.md (SoL% = dividend/divisor from bells; Simon Bolivar
  adds +20% to human colonies; Printing Press +50%, Newspaper +100% bells).
- King taxes: 2 -> 4 -> 6 -> 8 -> 10 -> 12 -> 14% (1600-1622). Kissed the
  ring each time except the 1603 furs party. Tools party refused (we need tools
  bought in Europe).
- Europe trip 1618: 1 Master Carpenter (train 1000), 1 Free Colonist, 100
  tools, 1 artillery (purchase 500).
- Oddity (CN8-O13): drag of a merchant hold into the warehouse refuses when
  the warehouse cap would be exceeded (Jamestown warehouse 200 vs tools 250):
  whole hold refused, no message. Warehouse Expansion (16) raised the cap to 300.
- Oddity (CN8-O14): Ejecting a pioneer (`eject 1 9 1`) takes 100 tools out of
  the colony stock (Jamestown 150 -> 50). Plan the tools first.
- Oddity (CN8-O15): a village-less coastal test: a site "does not have access
  to the ocean" (36,36) even next to ocean (37,37) (lake?). Pioneer re-sited to
  (38,29) Roanoke (coastal).

### 1642: gate stock

- Congress 1625: Simon Bolivar (political, +20% SoL in human colonies).
  Washington was offered 1625 and not taken (choose Washington next time he
  is offered if SoL is already high enough).
- King tax 18% (1642). Kissed the ring every 2%. Furs party (1603) was the only
  refusal; a Tools party would block the Europe tools we need (RULES).
- Sioux asked food/tobacco three times 1622-1640 (give: they then give gifts
  tobacco 6, furs 9). Keep giving: no Sioux raid so far.
- Europe 1642: horses 100 (2 gold each), muskets 50 (6), tools 100 (3). Prices
  now: artillery 900 (was 500 in 1593), horses 2, tools 3, muskets 6.
- Jamestown: Fortress (in progress, tools 210 ok), Blacksmith's Shop (ore 90
  -> tools), horses 100, muskets 50, artillery 4 fortified.
- Gate: artillery 4/19, dragoons 0/9 (each dragoon eats 50 horses + 50
  muskets), SoL 17/10/10 (Jamestown/Plymouth/Roanoke) vs 100.
- SoL math (docs/sons_of_liberty.md): each EOT divisor += 2 x pop, dividend
  += bells, both decay about 1/64. So SoL 100% needs about 2 x pop bells per
  turn (pop 11 -> ~22 bells). Jamestown made 4. Plan: Town Hall 2 and 3, Newspaper, Printing Press, 3-4 colonists in Town Hall.
- 1650: Fortress built in Jamestown. REF 24 land (King added regulars).
  King tax 20%.

### 1666: French invasion of Roanoke

- French (nation 1) attacked: Plymouth artillery beat a dragoon (demoted);
  Roanoke (38,29, pop 5, no Fort) fell: "French march into Roanoke! 347$
  plundered!". Our Roanoke colonists are now French (`COLONY 7 ... nation 1`).
- Decision: defend (Plymouth Fort + artillery, Jamestown artillery 5-6). Retake
  Roanoke after dragoons exist (veteran soldier 611 in London dock).
- RULES.md Franklin claim is wrong (CN8-O16).

### 1668-1688: SoL climbs, parties, Congress

- Town Hall statesmen (3 colonists in Jamestown Town Hall, 1 in Plymouth) made
  Jamestown bells 26/turn: SoL 17% (1642) -> 47% (1671) -> 79% (1679) -> 90%
  (1688). Printing Press built 1679 (after a stale "already built" loop,
  CN8-O19). Plymouth SoL 45% (1679). Bolivar (1625) and Washington (1665),
  Paine (1674), Jefferson (1685) in Congress.
- Rebel sentiment 50% (1669), 60%, 70% (1685): the Founding Fathers suggest
  declaring. Not declared (gate still unmet).
- Tax: Stamp Act (1669) 22% -> 30% offered; Tools party (1669, -59 tools) and
  Tobacco party (1688, -80 tobacco) kept 22%. Tools now boycotted in Europe.
- Roanoke: French took it (1666). Plymouth held (artillery beat French).
- Gold 2327 (1688). Tools and tobacco parties cost stock; Custom House still
  sells furs.

### 1742-1748: Europe purchases, deadlock, taxes

- Europe 1742-1743: merchant bought 100 muskets + 100 tools (+100 muskets).
  Tools boycott lifted by Jakob Fugger (Congress 1715); Europe then listed
  tools again (no BOYCOTT tag, 1742).
- Dragoons from colonists: `eject 1 CI 4` (50 muskets + 50 horses each).
  Dragoons 3 (1665-1685), 3 more needed to reach 9.
- Artillery count 17-20 (gate 22 at REF 27). Plymouth now builds artillery.
- Taxes 46% (1722), 54% (1738), 62% (1748). Furs/sugar/tobacco/lumber/tools
  parties refused some hikes but cost stock. Kissed the ring at 62%.
- Foreign popup "You must attack the infidel" (1744): choices Spanish/Dutch/
  Iroquois/Sioux. Escape cancels (no war).
- Helper: helpers/cycle.sh (admit, fortify guards, endturn). Still needs manual
  taxes, congress, and Europe trips.
