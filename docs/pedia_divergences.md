# Colonizopedia vs actual behaviour

STATUS: audit 2026-10-30. Living record of where `COLONIZE/PEDIA.TXT` (and the numbers the
pedia screen draws from `COLONIZE/NAMES.TXT`) disagree with what VICEROY.EXE actually does.
Not a bug list: almost everything here is DOS behaviour the port reproduces on purpose.
Candidate port bugs are collected in [Port findings](#port-findings) at the end.

PEDIA is not mechanics evidence (see [founding_fathers.md](founding_fathers.md) and
[conventions.md](conventions.md) evidence order). Every "Actual" below comes from the decomp
(`python3 tools/decomp_fn.py FUN_ssss_oooo`; "raw N" = line in `viceroy_unpacked.c`) or the port
code that ports it. "TB-N" = [tolerated_dos_bugs.md](tolerated_dos_bugs.md), "DB-N" =
[dos_bugs.md](dos_bugs.md); the opencol binary runs with `dos_buggy_behavior()` off, so TB fixes
are live there and test binaries keep DOS behaviour.

Tags:
- **TEXT** — the pedia text states something DOS does not do (or does differently).
- **SCREEN** — a number drawn on the pedia screen differs from what the sim uses.
- **HIDDEN** — real DOS behaviour the pedia never mentions.
- **PORT** — OpenCol differs from DOS (deliberate fix or candidate bug).

Sections: Terrain · Units · Goods · Buildings · Founding Fathers · Professions and concepts ·
Port findings · Doc drift found along the way

## Terrain

### What the terrain page shows
`pedia_article_terrain` (`src/core/pedia.c`) ports DOS `FUN_6cb2_0eac`: one row per job whose
NAMES yield is non-zero; Farmer and Fisherman show base+1, Lumberjack base×2, others the bare
base; a "Plow/River", "Road/River" or "Coast/River" `+1` line (Fur Trapper and Lumberjack `+2`);
a prime-resource term (`x2`, `+n`, food/fish `+n/2n`); "Expert: +3" for food and fish, "Expert:
x2" for every other job; Move Cost and "Defense or Ambush Bonus" (`+25%×byte`). Move and defense
match the DS bytes cell for cell. Everything below is about what those numbers leave out.

### SCREEN: shown number is not what a colonist gets
- **Expert Farmer / Fisherman "+3"** is measured from the raw table base, but it sits next to a
  number that already includes +1. Expert farmer on Plains makes 7 (raw 4 + 2 flat + 1 farmer
  step); the screen shows 5 and "+3", implying 8. Expert fisherman on coast makes 6, screen implies
  7. The expert also re-adds the positive SoL bonus a second time. `FUN_15eb_18ec` raw
  11890-11899; `colony_yield.c` expert branch.
- **Fisherman base+1** applies only when fewer than 6 of the 8 neighbours are Ocean/Sea Lane; with
  6-7 it is −1 (so "Fisherman: 4" on open sea is usually 2). Fish is 0 without Docks.
  `colony_yield_fisherman_distance_mod`; 18ec raw 11967.
- **Mountains "Silver Miner 1"**: without a Silver Deposit a free colonist gets 0 (1 with a road),
  an expert 1, and the road/river stack is suppressed entirely. Only a deposit tile matches the
  screen (3 / expert 6). 18ec raw 11925-11938.
- **"Plow" bonus on Hills, Mountains and forest pages** is unreachable: Hills/Mountains cannot be
  plowed (`FUN_2b5a_0b34` raw 42224-42227), and plowing a forest only clears it to the cleared type
  without setting the plow bit (`units_pioneer.c`). Only the river half of the line applies.
- **Rivers**: the screen shows a flat +1/+2. A major river adds its extra step only when the river
  is the sole contributor (TB-42), so a Farmer gets +1 on both kinds; a Fisherman or road-less Ore
  Miner gets +2 on a major river; a road-less Lumberjack +4.
- **Experts double the road/river step and additive resource bonuses** (step size 2 for a matching
  non-food expert): Minerals for an expert Ore Miner is +6 where the screen shows "+3".
- **Silver from Minerals never shown**: Minerals gives Silver Miner +1 (expert +2) even where the
  silver base is 0 (Tundra, Marsh, Swamp, Wetland, Rain). The pedia skips rows with base 0.
- **Tundra prime resource**: the screen draws resource 0 (the Depleted Mine row,
  `k_pedia_terrain_resource[0]`) and no bonus line. In play Tundra rolls Minerals (the MAPEDIT
  value 0 is remapped to 6, `map.c:76, 618-623`): Ore +3, Silver +1.
- **Colony centre tile ignores all of it.** `FUN_15eb_1f72` / `colony_yield_town_commons`:
  centre food is a 4-way class value (Arctic 0; Desert/Scrub 1; forests, Hills, Mountains 2; other
  cleared 3), +1 if plowed, +2 for Oasis/Wheat/Game, no river term. So a Plains centre makes 3,
  same as Tundra. The secondary good is the best of jobs 1-7 excluding Lumberjack, so a forest
  centre makes furs, never lumber; river +1/+2 on the secondary, no plow/road. Difficulty adds +2
  food on Discoverer, +1 on Explorer, +1 secondary on Discoverer.

### TEXT: prose vs NAMES / DOS
- **Mountains** "silver and ore are abundant": silver needs a Silver Deposit (see above).
- **Tundra** "minimal amount of food": food 2, same as Prairie, Grassland, Marsh and Swamp; a
  Tundra colony centre makes 3 food, same as Plains.
- **Omitted yields**: Marsh tobacco 2; Desert cotton 1; Scrub furs 2 and cotton 1; Plains ore 1;
  ore 1 listed for some Boreal/Wetland/Rain variants and omitted for others. Marsh "ore in
  abundance" is ore 2, same as Tundra, Desert and Swamp. Tropical 21 "some fur" and 13 "trapping
  fur" are both furs 2.
- **Sea Lane** "move toward the east (if exiting east) or west": the @SAILHOME prompt only comes
  from an eastward high-seas→high-seas step (`FUN_4720_015c` raw ~76048; `units_capture.c:665`);
  westward it only comes from stepping off the map rim. Sea Lane also fishes like Ocean (3) but
  never has a Fishery.
- "BORREAL" is a typo in TERRAIN8 and 16.

### HIDDEN
- **Movement** (`FUN_465b_0000`; `map.c`, `units_move.c`): 1/3 MP when both tiles have road or
  settlement, or both have river *and* the step is cardinal (diagonal river steps get nothing);
  entering a settlement costs at most 1 MP; a unit with full MP may always enter, even a 3-cost
  Mountain; with partial MP it rolls `rand(1,cost) <= remaining` and pays even when the roll fails;
  landing outside a colony spends all MP. Rivers give no combat bonus.
- **Defense bonus** (`FUN_157e_015e`; `combat_strength.c`, [combat.md](combat.md)): replaced
  entirely on colony and village tiles; a European defender attacked by natives in the open gives
  its terrain bonus to the attacker as ambush (also human vs REF in the open during the WoI); a
  Fortified European attacked by natives: neither side gets terrain. Fortify +50% applies only
  when the terrain byte is under 5, so it adds nothing on Mountains.
- **Pioneer work time** (DS +2 byte, `FUN_479b_01a6` / `0526`): plow/clear takes byte+2 turns,
  road takes byte turns, Hardy Pioneer halves (round down). Swamp, Rain and Mountains 7; Marsh 5;
  Tropical and Wetland 6. Arctic can be plowed (only +1 centre food).
- **Forest clearing lumber**: only to the nearest own colony within distance 4; 20 lumber, or
  (forest lumber byte + 1)×20 with a Lumber Mill; Hardy doubles; capped by warehouse room. Also
  fires the native land-claim alarm tail (base 5). A finished road gives +10 hammers to the
  nearest own colony. `FUN_479b_01a6` raw 76722-76858.
- **Mine depletion**: a per-colony counter (+0x97, scaled by (difficulty+1)/(difficulty+2)) rises
  with Minerals ore (+1), Minerals silver (+2) and Silver Deposit (+1); past 50 every worked
  Minerals/Silver square depletes (Minerals vanishes, Deposit becomes Depleted Mine). The Hills Ore
  Deposit never depletes. 18ec raw 11913-11921; `FUN_364b_033a`; `turn_production.c:433`.
- **Fur Trapper road/river paid twice in DOS** (TB-49), so the screen's "+2" is right for a free
  colonist in DOS; an expert's effective bonus is +4.
- **Henry Hudson** doubles Fur Trapper output between the improvement stack and Convert/Tory steps.
- **Indian converts** get +1 only on Farmer, the three planters, Fur Trapper and Fisherman — not
  lumber, ore or silver.
- **SoL / Tory** modify every field job (+1 per 50%/100% latch, Tory penalty); AI colonies skip the
  field Tory penalty.

## Units

### What the unit page shows
`pedia.c` ports `FUN_6cb2_07e6`: "Combat:" is the NAMES defense column, "Moves:" is the
movement ÷3, "Cargo Holds:" when non-zero; Soldiers/Dragoons add "(Veteran: attack×3/2)",
Artillery adds "(Attack: +(attack−defense) Damaged: −2)". The port matches DOS.

### SCREEN
- **"Combat" is defense only.** Every attacker, at sea too, fights at ×3/2 (shown as "Attack Bonus
  +50%" in Combat Analysis, never in the pedia): Soldier attacks at 3, Dragoon 4.5, Frigate 24,
  Man-O-War 36. `FUN_5fef_1b0e` raw 100457-100458.
- **"Combat: 1"** for Colonist, Pioneer, Missionary, Wagon Train, 2/6/10 for Caravel/Merchantman/
  Galleon, 0 for Treasure: their attack column is 0, they cannot attack.
- **Veteran figure** truncates: a veteran Dragoon defends at 4.5, not 4 (sim is ×8 fixed point).
  The veteran +50% applies to both attack and defense, unit types 1 and 4 only.
  `FUN_157e_004a` raw 8942-8944.
- **Moves** exclude Magellan's +1 for ships; ship evasion and ship-slow depend on max moves.
- **Artillery** "Combat 5 (Attack +2, Damaged −2)" matches, but the open-field ×¼ penalty is not
  shown (below).

### TEXT
- **Converts** (@UNIT0/5, @CARGO8/15): "any colonist" may be a scout, soldier or dragoon, and
  converts are good at outdoor jobs. An Indian Convert is offered only the Colonist row — never
  Pioneer, Soldier, Scout, Dragoon or Missionary; Europe's buy/bless rows are disabled for him.
  Outdoor bonus is +1 on food/sugar/tobacco/cotton/furs/fish only; indoors 1/3 rate like a
  criminal. `FUN_15eb_3454` raw 13557-13560; `colony_workers.c:1030`; `europe_dock.c:785-847`.
- **Artillery** (@UNIT11) "very effective at … attacking fortified units and Indian settlements":
  there is no Artillery attack bonus against anything. The only rule is ×¼ strength (attacking or
  defending) when the defender's tile has no settlement — skipped when the defender is a European
  unit on Fortify/Fortified, so fortified Artillery in the open is *not* "extremely vulnerable".
  Artillery defending a settlement against natives gets ×2; a plain Brave attacking a human's
  Artillery always loses; a loss involving a ship or water destroys it outright.
  `combat_strength.c:762-795`; `FUN_5fef_1b0e` raw 100472-100493, 100573-100577.
- **Regulars/Cavalry** (@UNIT6/8) "weakness … rough wilderness terrain": during the WoI, when the
  human attacks any European defender away from a colony, the defender's terrain bonus moves to
  the attacker, even if Fortified. Not REF-specific, and the REF are not penalised when attacking.
  Crown land attacks get +difficulty×atk/20; against a colony +50% plus a Tory share of
  (100 − SoL)%. `FUN_157e_015e` raw 9015-9037.
- **Wagon Train** (@UNIT12): "any cargo a ship can, except colonists and artillery" — it carries
  goods only; no unit boards a wagon, Treasure included (`units_cargo.c:350-373`). "Never more
  wagon trains than colonies" limits building, not owning: the check is wagons < colonies at
  choice and at completion (failure repeats @NOMOREWAGONS every turn, hammers kept); captured
  wagons or wagons kept after losing colonies can exceed it. Not buyable in Europe. Raw
  13736-13740, 56926-56933.
- **Seasoned Scouts** "better at all these tasks": only Lost City Rumours, Speak with Chief and
  Infiltrate. No extra sight, moves or combat.
- **Missionaries** "counteract Indian anger": establishing a mission always succeeds, alarm change
  = count×8 − {25,15,10,5} by alarm band (count = your existing missions in that tribe), so after
  several missions it *raises* alarm. `ai_contact_actions.c:1553-1630`. "Jesuits particularly
  effective": a Jesuit's own skill only matters for the −1,500 incite discount; ongoing goodwill and
  convert odds come from a Jesuit bit on the mission (also set by de Brebeuf). In DOS the Denounce
  Heresy Jesuit bonus is dead (tests Cotton Planter, DB-7).

### HIDDEN
- **Losing a battle** (pedia mentions only Artillery damage): Dragoon → Soldier, Soldier →
  Colonist, Continental Cavalry → Continental Army → Colonist, Cavalry → Regulars. Scouts,
  Pioneers, Missionaries and Regulars are destroyed. A European combat-unit winner captures
  Colonists, Wagons and Treasure (no gold paid); natives never capture; capture becomes
  destruction if the winner is a ship or either tile is water. A native winner in the open steps
  up: beating a Dragoon or Scout mounts it (+1 tribe horse), beating a Soldier arms it.
  `FUN_5fef_0352` raw 99344-99451; `FUN_5fef_1b0e` raw 100730-100744.
- **Equipping** only in a colony or on the Europe dock: 50 muskets, 50 horses, tools in 20s up to
  100 (Europe always 100). Europe only sells muskets to Colonists/Scouts, horses to
  Colonists/Soldiers, tools to Colonists.
- **Veteran promotion** (@UNIT1 "veteran as a result of winning battles"): a ladder. Criminal →
  Servant → Free Colonist → Veteran; experts and converts never promote. Odds roll(1, winner +
  loser ± difficulty) ≤ loser strength; George Washington makes it certain. Veteran → Continental
  only after the declaration, with the muster flag, human only. `FUN_5fef_172c`.
- **Continental Army/Cavalry** (4/4, 5/5) appear via the one-time first-war-turn muster in colonies
  with SoL > 49 (up to max(1, min(pop/2, pop×(SoL−50)/50)) veterans converted, `FUN_43f7_1eca/2022`),
  combat promotion, the intervention force, and WoI mercenaries.
- **Artillery** also comes from the intervention force and WoI mercenaries; a colony needs an
  Armory (192 hammers, 40 tools).
- **Pioneers**: 20 tools per finished job; below 20 the unit reverts to a colonist and the rest is
  lost. Every colony/village tile counts as a road for movement. DOS quirk: under 20 tools the byte
  wraps and the pioneer keeps working.
- **Scouts**: sight 2 (3 with de Soto); Galleon/Privateer/Frigate 2, Man-O-War 1, others 1.
  - Rumours (`FUN_65dd_0004` raw 103450-103618): skill 0 non-scout, 1 Scout, 2 Seasoned; de Soto
    +1 and reroll only for Scouts. Vanishes/trespass survive 1 time in (skill+1). Cibola
    ((skill+2)×10+d20)×100 gold; small treasure ×(skill+2)/2. Fountain of Youth odds don't improve.
  - Speak with Chief (`ai_contact_actions.c:941-1140`): alarm ≥ 75 certain death; from 25 up death
    possible (roll 0-100 Scout, 0-140 Seasoned); Arawaks add an extra death roll; Coronado turns
    every death into @CHIEFBORED. Reward only on first visit.
  - Infiltrate (`FUN_5f7a_000e` raw 98809-98881): success if 1-36 beats (fort levels + 6)×2 +
    (difficulty − 2), base halved for Seasoned. Failure kills the scout and gives the colony 100
    horses. Meet Mayor is refused after the declaration.
- **Incite** (`FUN_4d56_417e`): price scales with (alarm + 75), minimum 500; refused if the tribe
  hasn't met the target or alarm ≥ 75; effect +100 alarm; the missionary is never consumed. AI
  nations auto-incite against a richer human 4 times in 5.
- **Treasure Train**: "a Galleon is always required" is a space rule — Treasure fills 6 holds, so
  the Galleon must be empty. The King's transport offer fires when the treasure enters an own
  coastal colony and the nation has no Galleon (always, with Cortes); the Crown takes
  max((difficulty+10)×5, 2×tax) capped at 90%, or the tax rate with Cortes (`FUN_5fef_1908`).
  Own Galleon to Europe: Crown takes min(tax, 50%) (`FUN_48d3_06ba`). After the declaration
  treasure cashes in full at any own coastal colony. AI treasure cashes in any own colony at full
  value, no ship, no tax.
- **Privateer**: never triggers a European war, flies no flag, coastal forts fire on it even at
  peace; evasion ×2; Drake +50%.
- **Naval combat**: a defender whose attack is lower than the attacker's may evade (power = max
  moves in thirds + 3, ×2 Privateer, +3 Galleon, −4 per goods hold in use); each loaded hold costs
  strength; damage vs sinking is winner guns vs loser hull, gunless winners only damage; the
  Crown's last Man-O-War cannot be sunk; repair turns = max(0, loser def − winner def), floors 4
  (Frigate) and 8 (Man-O-War); adjacent enemy warships slow a ship (−4/−6/−8 thirds for
  Privateer/Frigate/MoW). Man-O-War cannot be built.
- **Braves**: "formidable in woods and hills" is the general native terrain rule (native defenders
  always get terrain; native attackers take an unfortified European's terrain in the open).
  Mounted Warriors 3/3 equal a plain Dragoon; a veteran Dragoon is better.
- **Combat modifiers on every unit page** (`combat_strength.c:680-900`, [combat.md](combat.md)):
  - human side strength −(difficulty − 4) on the ×8 scale;
  - weak defender: land attacker attack > 1 and defender defense < 2 halves the defender
    (Colonists, Pioneers, Scouts, Missionaries, Wagons, plain Braves);
  - Fortify +50% only once Fortified (orders 6), not while Fortifying;
  - attacker with < 1 move fights at (thirds left)/3;
  - Spanish +50% attacking natives on a village tile;
  - easy levels: Discoverer/Explorer damp AI attacks on human units before the WoI; on Discoverer
    an empty human colony can't be taken before turn 80 and a human attacker's strength doubles.
    None of this shows in Combat Analysis.
- **Europe prices** (none shown): Artillery 500 + 100 per Artillery bought, no cap
  (`FUN_38fd_4b50`); ships fixed (Caravel 1000, Merchantman 2000, Galleon 3000, Privateer 2000,
  Frigate 5000). Training uses NAMES @JOB prices (600-2000); Seasoned Scout, Veteran Dragoon, the
  three Planters, Fur Trapper and Expert Teacher can't be trained. Recruit price
  (recruits + difficulty + 7)×20 minus a crosses discount, minimum 10. Harder levels give more
  Criminals/Servants; a Soldier immigrant arrives mounted 1 time in (difficulty+5).

## Goods

### TEXT
- **Food** "best … plains, grasslands, prairie, and savannah": Grassland and Prairie are food 2,
  same as Tundra, Marsh and Swamp. Only Plains (4) and Savannah (3) are good food land (NAMES
  @UNFORESTED).
- **Furs** beaver "in colder climates": Beaver appears only on Mixed forest.
- **Lumber** "collected whenever your pioneers clear a forested square": only with an own colony
  within 4 tiles (see Terrain HIDDEN).
- **Ore** "hills, mountains, and swamps": also Tundra, Desert, Marsh (2), Plains and some forests (1).
- **Silver**: a bare mountain gives a non-expert 0 without a road; Minerals on other terrain also
  gives silver; only resource squares run dry (see Terrain HIDDEN).
- **Horses**:
  - "can be obtained only in Europe": villages sell and gift horses, capturing a colony takes its
    horses, AI colonies buy 2 for 10 gold, failed infiltration gives 100.
  - "extra food stored in your warehouse": breeding uses **this turn's surplus**, not stock.
    bred = min(ceil(h/50)×2 (ceil(h/25)×2 with Stables), ceil(surplus/2), warehouse room); each
    horse bred eats 1 food; 0 if under 2 horses. AI colonies get the uncapped amount.
    `FUN_15eb_1f72` raw 12650-12681; `colony_prod_horse_breed`.
  - Stables "breed twice as fast": with 25 or fewer horses a Stable adds nothing (2 either way);
    at 51 horses it gives 6 instead of 4.

### HIDDEN
- **Food**: every colonist eats 2, field workers included; 200 food makes a colonist. Starvation
  only from 0 stock plus a shortfall, with easy-level grace (none before 1520, then 2/3 or 1/2 of
  deaths cancelled). AI colonies get 0/1/2 free food by difficulty. Fish needs Docks.
- **Manufactured goods** (Rum, Cigars, Cloth, Coats): 3/6/9 per worker for House/Shop/Factory;
  Servants 2/3, Criminals and Converts 1/3, experts ×2. Factory input = floor(2 × colony gross ÷ 3)
  over the whole colony total (gross 1 is free); short input follows a non-proportional ladder
  (S=1 gives 2, S=4 gives 6). AI factories always run at full capacity. `FUN_15eb_0bd4` raw
  10159-10183; [building_production.md](building_production.md).
- **Trade Goods**: buying from a foreign colony needs Jan de Witt and peace, human only; natives
  never sell them. Native demand falls 2 per 100 tons sold and recovers over time.
- **Tools / Muskets**: 20 tools per pioneer job; muskets Armory 3 / Magazine 6 / Arsenal 9 per
  free colonist with **no** input discount at any tier (see Buildings). AI colonies get missing
  construction tools free.
- **Warehouse**: capacity (level+1)×100, Food exempt from the cap; at end of turn every other good
  is clamped and the overflow destroyed; @SPOIL only reports losses of 2 tons or more that existed
  before this turn's production. `FUN_15eb_0a50`; `FUN_364b_0688` raw 57845-57868.
- **Market**: sell at price − 1, buy at price + markup, tax on sales only; the four nations share
  one pressure value; the Dutch market feels 2/3 of sales and recovers faster; AI-only price
  ceilings on Horses, Tools and Muskets.

## Buildings

### What the building page shows
Both DOS (`FUN_6cb2_1ba8`) and the port (`pedia.c:1533-1601`) show sprite, name, the worker's
expert job and output good, and "Prerequisite: X". No hammers, tools or population appear, so
costs can't mismatch on screen. The sim's build numbers come from NAMES @BUILDING (min colony size
in record +8, read by `FUN_15eb_3650` raw 13663 and `colonies_building_is_buildable`).

### TEXT
- **Minimum population** (PEDIA vs NAMES, NAMES wins): Fort "population reaches 4" → 3;
  Drydock "population of 6" → 4; Weaver's/Tobacconist's Shop, Rum Distillery, Fur Trader's Shop,
  Blacksmith's Shop "at least 4" → 1; Coat Factory "at least 8" → 6.
- **Arsenal** "doubles the output of muskets": Armory 3, Magazine 6, Arsenal 9, so the Arsenal is
  ×1.5 over the Magazine, and muskets never get the factory input saving (9 tools in, 9 out).
  `FUN_15eb_1f72` raw 12689 emits muskets via `0b96`, not `0bd4`; port_bugs #910.
- **Stables** "horse reproduction is doubled": see Goods/Horses.
- **Cathedral** "dramatically increasing … cross production": the passive part is +1 (1 base, +1
  Church bit, +1 Cathedral bit); the real gain is that every church worker's output doubles
  (3→6, Firebrand Preacher 6→12).
- **Custom House** "sell goods … on a regular basis": sells a good only when stock > 99, down to
  50. A human colony sells only ticked goods; the founding default (mask 0x1ede, `FUN_364b_1ba8`
  raw 58059-58068) is Sugar, Tobacco, Cotton, Furs, Ore, Silver, Rum, Cigars, Cloth, Coats — never
  Food, Lumber, Horses, Trade Goods, Tools or Muskets. AI colonies use a deny list (Food, Lumber,
  Horses, Tools, Muskets, plus Ore with an Armory or tools/muskets made that turn). Price = Europe
  price − 1, taxed before the WoI, untaxed during it; boycotts ignored; a human Custom House shuts
  while an enemy warship is adjacent. `europe_market.c:296-470`.
- **Name mismatches inside DOS data**: PEDIA "FUR TRADER'S SHOP"/"COAT FACTORY" vs NAMES "Fur
  Trading Post"/"Fur Factory"; PEDIA row 11 "COLONIAL ASSEMBLY" vs NAMES "Town Hall"; "Master Fur
  Trapper" vs NAMES "Expert Fur Trappers".

### HIDDEN
- **Requirements omitted by the pedia**: Arsenal needs Adam Smith (gate covers rows 5, 0x17, 0x1a,
  0x1d, 0x22, 0x29); Shipyard needs Drydock and pop 8; Newspaper needs Printing Press and pop 4.
  `FUN_15eb_3650` raw 13707-13711.
- **Docks vs Drydock site**: Docks need any water tile in the work ring, lakes included (raw
  13672-13687); Drydock needs the coastal flag, set once at founding from open-sea adjacency (raw
  13688). A lake-only colony gets Docks but never a Drydock.
- **Fortification defense** `(k+4)/4`, k = 2/4/6/8 for none/Stockade/Fort/Fortress: +50% / +100% /
  +150% / +200% matches the pedia, but a colony with no stockade still gives +50%; fortify adds
  only when k < 5 (bare colony fortified +100%, Stockade fortified +150% = Fort, nothing in
  Fort/Fortress). `FUN_157e_015e` raw 9009-9045.
- **"Slow foreign vessels"**: a ship stepping next to a foreign Fort spends 2 MP-thirds; next to a
  Fortress 50 (dead stop); Stockade none; no roll. `FUN_5bfb_3180`.
- **Shore fire**: attack 4 × (Fort bit + Fortress bit) × (1 + artillery on the tile), one salvo per
  adjacent water tile at the first ship there, if not at peace with the owner or any Privateer;
  ship damaged or sunk, a fort loss does nothing. `FUN_364b_03f6` raw 57040-57083.
- **Stockade pop rule**: DOS refuses any "leave colony" while the Stockade bit is set and pop < 4
  (`FUN_2f2b_011e`, code 0x15), so a stockaded colony can never be abandoned voluntarily.
- **Drydock repair**: the damaged ship teleports to the nearest own colony with Drydock/Shipyard
  (own tile skipped, ties to the last colony in the list); none → Europe; during the WoI a human
  ship with no repair port is lost. `FUN_5fef_0352` raw 99584-99604.
- **Town Hall bells** (`FUN_15eb_1f72` raw 12603-12650): workers 3, Elder Statesman 6, plus SoL
  modifier per worker; every colony +1 bell even without a Town Hall; then Jefferson +50%, Paine
  +tax%, AI subsidy +(pop+3)/5; last, Newspaper ×2 *or* Printing Press +50% — they don't stack
  (both = +100%, not +150%).
- **Crosses**: every colony makes 1 cross without a Church; Penn's ×1.5 applies per Preacher only.
- **Lumber Mill / Carpenter**: a colony-wide ×2 applied after base + SoL (base 6 expert, else
  class rate), unlike the craft chains; hammers eat lumber 1:1 including the SoL bonus.
- **SoL / Tory modifier** for building jobs: −floor(tories/(10−difficulty)), +1 at the 50% latch,
  +1 more at 100%, added before tier and skill scaling; AI colonies have Tories zeroed.
  `FUN_15eb_18ec` raw 11866-11888.
- **Schools** (pedia lists only what each teaches): 4/6/8 turns by the *teacher's* level, not the
  building; the student is a random colonist anywhere in the colony (converts can't learn);
  Criminal → Servant → Free Colonist before any specialty; at most 3 graduations per colony per
  turn. `turn_production.c:1094-1111`.
- **Buying a building**: 13 gold per missing hammer + (Europe tools price + 4) per missing tool,
  doubled if no hammers are banked yet. `FUN_2f2b_5e44` raw 52709-52722.
- **Completion**: leftover hammers are lost; a human colony short of tools stops with @NEEDTOOLS,
  AI and Crown colonies get the tools free; upgrading moves the lower tier's workers into the new
  building. `FUN_364b_0688` Phase L raw 57748-57774.
- **Cut content**: rows 10 and 11 (the extra "Town Hall" entries) have empty pedia bodies and can
  never be built (raw 13695-13703). The NAMES upkeep column is never read.

## Founding Fathers

PEDIA quotes and per-Father DOS sites are in [founding_fathers.md](founding_fathers.md); this
section lists only the gaps. Match the pedia: Fugger, Hudson, Drake, Paine, Stuyvesant (the gate
itself — Custom House rules are under Buildings).

### Election (no pedia text covers it; HIDDEN)
- Bells needed (`FUN_4345_0982`): human base (difficulty+3)×2×8, AI (14−difficulty)×8; +50%
  compounding after 1599, 1649, 1699, 1749; threshold (count+1)×base+1, halved for the first
  Father. Overflow above the threshold is thrown away. After the declaration there are no
  elections; the pool feeds the intervention threshold (difficulty×1500+2000).
- Fathers are not exclusive: every power runs its own Congress.
- Era weights (NAMES @FATHERS, bands ≤1599 / 1600-1699 / ≥1700): never before 1600 — Fugger,
  Washington, Jones, Bolivar, Las Casas; never from 1700 — Minuit, Hudson.
- One weighted roll per category; the human cannot cancel; the slate locks at the first bell. The
  AI keeps the roll from the category with the most eligible Fathers (ties to the later category,
  `FUN_4345_015a`), with no preference for effects. Several elections per turn are possible.

### Per Father
- **Adam Smith (0)** TEXT: "1½ manufactured goods per raw unit" does not hold for muskets (1:1 at
  every tier). AI too.
- **Peter Minuit (2)** HIDDEN: besides free land, finished Pioneer clear/plow/road on native land no
  longer raises alarm (+5/+3 base, +difficulty human, ×2 within 3 tiles, ×3 within 2); the AI
  auto-buy is skipped. `FUN_4cc6_07c2`, `FUN_15eb_26e4`, `units_pioneer.c:132-210`.
- **Jan de Witt (4)** HIDDEN: human only; needs peace (@TRADEATWAR); only a wagon or ship with cargo,
  never enters, loses all moves even when refused; sells one hold; price = Europe price − 1 − their
  tax − a 10-(d+1)×12% haggle, halved at war. `FUN_5f7a_020e`, [foreign_colony_trade.md](foreign_colony_trade.md).
- **Ferdinand Magellan (5)** TEXT: no west-edge shortening exists. Every crossing is 1 turn, 2 when
  RNG(1,100) > 89 and the nation has > 2 ships; Magellan only removes that 10% delay. The west-edge
  branch rolls RNG(0,1) and tests Magellan, then discards both (DB-40). +1 ship move is real, AI too.
- **Francisco de Coronado (6)** HIDDEN: reveal includes foreign colonies (±5); every colony founded
  later by anyone is revealed to every Coronado owner (`colonies_reveal_founded_w`); Speak with
  Chief never kills a scout (→ @CHIEFBORED).
- **Hernando de Soto (7)** TEXT: "rumours always positive" only for Scouts (`FF7 && type == 5`,
  raw 103454-103458); colonists and soldiers still get Vanishes/Nothing/trespass; even scouts can
  get an empty or angry burial mound (@SCREWED, +100 alarm). Sight radius 2 for land units only
  (not ships; outer ring same continent only), Scouts 3. No reveal at election.
- **Sieur de La Salle (9)** HIDDEN: election sweeps the elector's pop > 2 colonies; afterwards a
  stockade comes only when a colonist *joins* and pushes a colony to 3 (`FUN_15eb_1068` raw 11311),
  not from births or captured colonies; those colonies then fall under the stockade rule (can't
  drop below 3 or be abandoned). The port's join grant sweeps all owned colonies: port_bugs.md #1329.
- **Hernan Cortes (10)** TEXT/HIDDEN: treasure for tech 0/1 tribes guaranteed and ×1.5; tech 2 +6 to
  a 2-6 roll (+6000 gold); tech 3 multiplier 16→26 (25→35 rich capital) — tech 2/3 always paid
  treasure anyway. "Transport free of charge" still takes the tax-rate share (free = no surcharge).
  The offer appears even with your own galleon; human only. In Demand Tribute European strength
  counts ×1.5 (overlay `a5f4` raw 77391). `FUN_5fef_31ea`, `FUN_5fef_1908`.
- **George Washington (11)** TEXT/HIDDEN: promotion is guaranteed but follows the ladder (Criminal →
  Servant → Free Colonist → Veteran); experts/converts never promote. After the declaration,
  winning Veterans always become Continentals (human, mobilisation flag). `FUN_5fef_172c` raw 100109.
- **Paul Revere (12)** TEXT: no colonist is armed and no muskets are spent. DOS always fields a
  temporary militia defender for an empty colony; Revere only gives it the soldier graphic and
  strength 2 instead of 1. Needs ≥ 50 muskets and **no units at all** on the tile. Works against
  natives and AI. `FUN_5fef_1b0e` raw 100424.
- **John Paul Jones (14)** HIDDEN: a Frigate for any electing nation, created on the westbound lane
  232+n. The port docks it in Europe instead: port_bugs.md #1328. No Frigate at the unit cap.
- **Thomas Jefferson (15)** HIDDEN: +50% of the whole colony bell total (statesmen plus the free +1),
  before Paine and Press, so they compound. AI planners weight bells ×2.
- **Pocahontas (16)** HIDDEN: election drives every tribe's alarm to 0 *and* clears war/attack bits
  (peace with all natives), AI too. "Half as fast" is an integer shift, so +1 bumps become 0;
  stacks with France's own halving. Also halves mission alarm cost and lowers your threat weight in
  Denounce Heresy and AI site scoring. `founding_fathers.c:303-332`, `ai_diplo.c:3122`.
- **Simon Bolivar (18)** TEXT/HIDDEN: +20 percentage points (not +20%), capped at 100, human only
  (`FUN_15eb_0274`). For an AI elector the SoL bonus does nothing; each AI colony instead gets
  +(pop+3)/5 bells (port_bugs.md #938b, fixed). Election also adds a one-time +20 to the cached empire SoL
  `DS:0x53d0` until the next recompute — can unlock the 50% declaration gate early (TB-40).
- **Benjamin Franklin (19)** TEXT/HIDDEN: human only in effect. @KINGNEWWAR never fires, so you
  also lose that event's gold and Veteran Soldiers. "Always offer peace" is inside the negotiation
  dialog only (`FUN_5bfb_13b0`); AI attack paths can still start war. Negotiation: grievance and
  worthy/forced-conflict zeroed or halved, withdraw and alliance cost halved, tribute +100 gold 1 in
  3, treaty cooldown halved.
- **William Brewster (20)** HIDDEN: dock colonists keep their class; AI takes slot 1; the every-4th-turn
  forced-expert refill is skipped (`FUN_38fd_5e52` raw 68577; `europe_harbor.c:1732`).
- **William Penn (21)** TEXT: "+50% crosses in all colonies" — +50% per Preacher only, after the
  Cathedral ×2; base crosses (1 + Church/Cathedral) not boosted.
- **Jean de Brebeuf (22)** HIDDEN: works through the mission Jesuit bit (existing upgraded at
  election, new ones graded at founding): alarm relief ×4, convert roll ×2, subjugation threshold 8;
  a missionary from a razed village returns as a Jesuit. No effect on Denounce Heresy.
- **Juan de Sepulveda (23)** TEXT/HIDDEN: +4/13 subjugation chance (threshold 4→8, Jesuit 8→12, Spain
  +4), only after beating a village holding **your own mission**; the convert appears on the
  attacker's tile, it does not join a colony. Also halves your missions' alarm relief and doubles the
  alarm cost of a new mission.
- **Bartolome de las Casas (24)** HIDDEN: election only; converts in colonies, Colonists-type map units
  and dock immigrants become Free Colonists in place; converts in another unit body untouched; later
  converts stay converts. Also doubles mission alarm relief, halves new-mission alarm cost, and −4
  subjugation threshold (a plain non-Spanish mission then gives 0% converts).

## Professions and concepts

PEDIA.TXT has no concept articles: besides the category sections it has only `@PEDIA` (7
category names), `@MISCELLANEOUS` (12 titles, no bodies) and `@JOB0`..`@JOB27`. The job page shows
no numbers (`pedia_article_job`): icon, expert name, building chain, produced cargo. The only
profession numbers in the pedia are on the terrain pages (see Terrain SCREEN).

### TEXT
- **Expert Fisherman** "twice as productive": flat +2 (plus SoL again), not ×2. Ocean free 4 →
  expert 6. Expert Farmer's "2 more food" is right (flat +2), but the expert also re-adds the
  positive SoL bonus (gap up to +4 at 100% SoL) and gets 0 on a base-0 tile. `FUN_15eb_18ec`;
  `colony_yield.c:487-496`.
- **Fisherman location** "the more land that borders the square … the more productive": DOS has
  only two outcomes, +1 with fewer than 6 ocean neighbours, −1 otherwise (TB-41). The opencol
  binary fixes TB-41 (+2/+3/+4 for sheltered water).
- **"Twice as productive" field experts** (sugar, tobacco, cotton, fur, lumber, ore, silver): ×2
  applies to (base + positive SoL); the Tory penalty is subtracted after doubling (Hills ore with
  Tory −1: free 4, expert 9); road/river adds 2 for every Lumberjack, expert or not (Mixed + road:
  free 8, expert 14); bare-mountain silver free 0, expert 1. `colony_yield.c:481-623`.
- **Expert Carpenter / Firebrand Preacher** "twice as productive" / "twice the crosses": flat 6 vs 3,
  then SoL added undoubled (SoL +1: 7 vs 4; Tory −1: 5 vs 2). Lumber Mill / Cathedral ×2 is
  colony-wide. `FUN_15eb_1d4c`; `colony_production.c:138-171`. Elder Statesman is exactly ×2 of
  (class rate + SoL) and matches; crafts match (×2 output and input).
- **Learning on the job** (JOB1) "works at it long enough may become a master": not time-based.
  Each turn a non-expert, non-Convert working Sugar, Tobacco, Cotton or Fur Trapper (jobs 1-4; not
  Farmer) has a fixed chance — 1/100 free colonist, 1/200 servant, 1/300 criminal — and only while
  the nation owns zero experts of that job anywhere; one success closes it. `FUN_364b_0688` raw
  57591-57611 (TRAINPROFESSION); `turn_production.c:398-417`.
- **"Only from the Indians"** (JOB1-4): also via learning on the job and schools (College for the
  three planters, Schoolhouse for Fur Trapper). Europe never offers them (training cost −1; the
  recruit pool remaps sugar→fisherman, tobacco→lumberjack, cotton→farmer, fur trapper→ore miner,
  `europe_pool.c:154-190`, `FUN_38fd_46d4`).
- **Servants / Criminals**: field work equals a free colonist (matches). Indoors servants 2/3,
  criminals 1/3 (house 1/2/3, shop 2/4/6, factory 3/6/9) — "almost totally ineffective" overstates
  it. The class rate also cuts bells, crosses and hammers (criminal statesman 1 bell).
  `colony_prod_scale_by_class`.
- **Free Colonists** "the most productive" non-specialists: Converts out-produce them on the six
  +1 outdoor jobs (see Units TEXT).
- **Jesuit Missionary** "much more effective at all missionary tasks": in DOS Denounce Heresy has no
  Jesuit bonus (DB-7) and founding a mission has the same alarm effect. Real edges: mission goodwill
  ×4 per turn; convert chance (tech+2)/16 doubled, rolled only when a Brave from the mission village
  visits; incite −1500 gold, and Jesuit missions in the tribe discount 1000 instead of 250.
  `ai_contact_actions.c:1475-1626`; `ai_contact_trade.c:415-420`; `ai_contact_demand.c:296-323`.
- **Seasoned Scouts** "all the above more effectively": sight +1 is for any Scout-type unit, moves
  equal. Real edges: chief threshold rand 0-140 vs 0-100, Arawak kill range doubled, rumour skill 2.
  "Become seasoned after visiting Indian settlements": 1 in 3 of good, not-yet-scouted chief outcomes.
- **Veteran Dragoons** (JOB23) "through training or experience": @JOB 0x17 is never written to a
  unit and has no Europe row; a veteran dragoon is a Veteran Soldier specialist with horses.
- Nits: JOB13/14 headers say "EXPERT" where NAMES says "Master"; @JOB18 "Student" has no body
  (Expert Teacher was cut).

### HIDDEN
- **What natives teach** (`thunk_FUN_1000_a618`; `ai_contact_actions.c:612-930`): a weighted draw
  over Farmer, the three planters, Fur Trapper, Ore/Silver Miner, Weaver, Fur Trader, weights by
  tribe tech (Weaver, Silver and Ore need tech ≥1-2); Farmer → Fisherman on an ocean roll, Fur Trapper
  → Seasoned Scout when (x+y)%3==0. Fixed per village per game. Ordinary villages teach once in
  total across all nations; capitals without limit. Criminals refused (@LEARNCRIMINAL), Converts
  refused, servants learn directly. Alarm 25-49: refusal chance (200·difficulty+100)/1000
  (@LEARNSLOW); alarm ≥ 50: refuse and +3 alarm (@LEARNMAD).
- **School levels** (NAMES @JOB): Schoolhouse — Farmer, Fur Trapper, Lumberjack, Ore/Silver Miner,
  Fisherman, Carpenter, Hardy Pioneer, Seasoned Scout; College — the planters, the crafts, Veteran
  Soldier; University — Preacher, Statesman, Jesuit. (Timing and student pick: Buildings HIDDEN.)
- **Promotion pool**: the promotion roll range shrinks by 10 for criminals and 5 for servants, so
  they step up more easily. Veteran +50% only for Soldier/Dragoon unit types.
- **Indoor cap**: 3 workers per building (DOS misses the Rum Distiller, DB-15).
- **Hardy Pioneer** "faster": exactly half the work turns (integer) and double clear-cut lumber.
- **Taxes**: the King can also cut tax by 2-5 (favour score under 100); cap 75%; nothing before turn
  30; interval 22 down to 5 turns by difficulty and year. `FUN_38fd_5be8`; [difficulty.md](difficulty.md).
- **Crosses / immigration**: +1 base cross per colony, Church and Cathedral +1 each; the docks meter
  gains +2 per turn but −2 per immigrant waiting on the docks. Crosses needed = 2·(colony population
  + all nation units incl. docks) + 8, capped at 4000; English ×2/3; AI ×(8−difficulty)/8.
  `FUN_38fd_584a` / `5e52`; `europe_harbor.c:1365-1520`.
- **Liberty bells / SoL**: full SoL needs about 2 bells per colonist per turn (divisor grows by pop·2,
  both terms decay 1/64); while bells < population a colony loses SoL%/20 bells. The production
  modifier is −floor(tories/(10−difficulty)), +1 at ≥ 50%, +1 more at ≥ 100% (that latch clears
  below 95); AI fields skip the Tory term, AI indoor uses a threshold of 10.
  [sons_of_liberty.md](sons_of_liberty.md).
- **Independence** needs nation SoL ≥ 50%, population-weighted. The **REF** exists from turn 1,
  seeded Regulars 8d+15, Dragoons 5(d+1), Men-o-War 3d+2, Artillery 6d+2, and grows from the royal
  purse each peacetime turn. [difficulty.md](difficulty.md).
- **Arawak chiefs** can kill even a friendly scout (rand(0, 8−difficulty) == 0).

### Port-written Game Concept pages
DOS looks up `@MISC<n>` sections that don't exist, so its 12 concept pages are title-only. The port
shows its own bodies (`k_misc_bodies`, `pedia.c:43-110`; "Roads" blank). Where they are wrong (port_bugs.md #1331):
- **Disband**: "disbanding a colonist in a colony adds that person to the settlement" — `units_disband`
  just deletes the unit; the ship refusal is not limited to "at sea".
- **Plowing** "one extra unit": a matching expert planter gets +2.
- **Veteran** "win battles (or graduate from a college)": overstates promotion (see Units HIDDEN).
- **Fortify**: +50% only once Fortified (after one night); in a colony it adds +2 to the colony
  multiplier instead, nothing in a Fort/Fortress.

## Port findings

Deliberate, already documented: TB-41/42/49 fixes (fisherman cascade, major river, Fur Trapper
double pay — the pedia's "Fisherman 4", "Plow/River +1" and trapper "+2" are therefore off by one
in the opencol binary), DB-7 (Denounce Jesuit; port also grades Brebeuf's Denounce mission as
Jesuit), DB-12 (Artillery price on cancel), DB-13 (de Witt trade debits the colony), DB-15, DB-34
(1-ton spoilage), DB-36, DB-40 (Magellan dead RNG draw), DB-43, DB-46 (pioneer counter), DB-50,
TB-47 (birth at unit cap), DB-9, DB-32, DB-47/48 (rumour counters, scout alarm), known_divergences
#909 and #1018.

Candidates verified 2026-10-30 against the decomp and filed:
1. **Terrain article title 16-23**: port_bugs.md #1327 (OPEN). DOS `FUN_6cb2_0eac` appends
   " Forest" only for `7 < idx < 0x10`; `pedia_terrain_display_name` appends it for 8-23.
2. **John Paul Jones Frigate**: port_bugs.md #1328 (OPEN). DOS raw 73093-73100 spawns it at
   x=y=232+n, the westbound lane out of Europe, so it sails over by itself; the port docks the
   human's Frigate in the Europe harbour (#467 misread the lane as the dock).
3. **La Salle**: the all-colonies admit sweep is port_bugs.md #1329 (OPEN). The `DS:0x35c` gate
   is REFUTED as a bug: 0x35c is set only while `FUN_2f2b_348c` trial-admits every job for its
   production preview (raw 50590, cleared 50842 before the real assignment), and the port has
   no trial admits.
4. **One-bit vs tier tests**: the repair-port test is port_bugs.md #1330 (OPEN), reachable in
   normal play because the raid burn walks down to the lowest owned tier (Docks, then Drydock,
   leaving Shipyard). The Stockade pop rule and the shore-fire probe are REFUTED in practice:
   the raid picker rejects rows 0-2 (raw 99849-99853) and nothing else clears a fortification
   bit, so a Fort/Fortress without the lower bit needs a hand-edited save.
5. **@MERCENARIES**: REFUTED, DOS behaviour. `FUN_43f7_2022` (raw 74976-75070) builds the popup
   from @UNIT rows 6/8/11 (Regulars/Cavalry/Artillery), and on accept `FUN_43f7_10f0(1)` maps
   the slots through `FUN_43f7_0082` (raw 73519-73544) to Continental Army/Cavalry/Artillery
   once the WoI bit is set. The port matches.
6. **Port-written Game Concept pages**: port_bugs.md #1331 (OPEN), blank them to match DOS.
7. **Pedia building list hides Capitol / Capitol Expansion**: not a divergence. The user
   observed that DOS never shows them (a disabled stub of a feature cut before release), so
   the port's list matches. #1332 resolved 2026-10-30, entry removed from known_divergences.md.
8. **AI bells subsidy flag 0x12**: REFUTED, already fixed as port_bugs.md #938(b).
   `FUN_15eb_3960(nation, 0x12)` is the Bolivar FF bit and every caller folds it into
   `nation_is_ai`; the stale "lead #938b" comment in `colony_production.c` is corrected.
9. **Franklin in the Foreign Affairs report**: REFUTED. FF 0x13 is read at exactly eight sites
   (raw 68336, 97524-98422), none in the FA report `3f41`, so the "FA UI PARK" note was stale
   and nothing is missing (founding_fathers.md row 19 corrected).

## Doc drift found along the way

All corrected 2026-10-30 (terrain_yields.md, building_production.md, combat.md,
coastal_fort_fire.md, assets.md + `colony.c` founding comment, europe.md, unit_orders.md,
founding_fathers.md rows 9/14/19 + `founding_fathers.c` comments). Kept for the record:

- [terrain_yields.md](terrain_yields.md) "+0xe type/graphic index … Rain quirk": the terrain record
  starts at DS:0x2f74 with the name word at +0 (`FUN_75c2_1380` raw 120713-120728), so +0xe/+0xf is
  the next record's name handle; record 16 reusing Boreal's handle is normal, not a quirk.
- [building_production.md](building_production.md):220 "Same Stockade ladder" and
  [combat.md](combat.md):162-163 "Stockade/Fort ×2", "Fortify on Stockade/Fort 4+2=6": code gives a
  Fort ×2.5 and no fortify stacking there.
- `original_sources_annotated/turn/coastal_fort_fire.md` says a fort miss slows the ship; removed as
  invented (the #249 note in `units_fort_vs_ship`).
- [assets.md](assets.md):411 says a new colony's first project is the Stockade; code (and DOS raw
  58113-58115) picks Docks if coastal, else Warehouse. `colony.c:1543` misreads +0x8d as the
  project (it is the specialty cargo); the port's Stockade-first branch can't trigger.
- [europe.md](europe.md) `europe_cash_treasure` omits the 50% cap; its Custom House blocked-goods
  list (~660-663) doesn't say the block is AI-only.
- [unit_orders.md](unit_orders.md):205 says Arctic can't be plowed; it can.
- [founding_fathers.md](founding_fathers.md) row 19 still says WORTHY skips provoke for a "Franklin
  pair" (removed as invented, #472). Stale comments in `founding_fathers.c` `apply_effect` (Las Casas
  ownership tick, de Witt ai_euro wagon act, Franklin declare/war-hit) and
  `founding_fathers_la_salle_check`.
