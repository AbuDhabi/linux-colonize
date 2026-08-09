# Euro per-unit act (`FUN_521d_5b66`) — thin section-map

Layer D early-settle map only. Full body ~1815 lines at
`viceroy_unpacked.c` ~90446–92260. Line-by-line extract still deferred (R5);
**mid-planner combat / deep case-7 economy / deep land scoring slices are OPEN**
(unpark #4); many thin peels (dock hire matrix, construction prefers, haul,
fortify/wake, naval prey) are **Done**.

Linux: `ai_euro_unit_act` + expand/war thin — deepen vs peels (**OPEN** remainders).

## Entry / wiring

| Item | Detail |
|------|--------|
| Ghidra | `FUN_521d_5b66` |
| Thunk | `2a1f_0488` from `FUN_521d_6d8e` ship/land act loop |
| Args | Decomp shows corrupted far prototype; live arg = **unit index** |
| Annotated | `euro_unit_act` in [`euro_dispatcher.c`](euro_dispatcher.c) |

Not nested inside `20e6`. Goals are `0a60`/`5d04`; scoring is `20e6`; act is `5b66`.

## Phase outline

### 0. Early move-scoring gate (~90552–90580)

```
if moves_spent == 0 OR orders != 0x0B (goto):
    r = 2a1f_04f4 → FUN_521d_20e6 (move_scoring)   @90557
    if r != 0: return
else: path validate (281f_0984); order 'E' Europe-counter tweaks
if orders-7 > 5: clear orders (0934); return
switch (orders) cases 7..0x0b
```

### 1. `switch (314c)` arms (bodies; mid-planner **OPEN**)

| Lines | Case | Label |
|-------|------|-------|
| 90589–91142 | **7** | Europe hire (`0500`/`5c3c`), founding urgency, treasury buy — **partial** (Linux: dock expert matrix Done; deep economy/treasury OPEN; see §2d / §2e) |
| 91143–91158 | **8** | short |
| 91159–91194 | **9** | short |
| 91195–91362 | **10** | UI/chrome / dialog-ish (`281f_04ac` ≠ `06ae`) |
| 91363–92150 | **0x0b** | Ship/land act: ocean probe, naval band, dir8 score |

### 2. Case `0x0b` settle-adjacent notes (**OPEN** deepen)

| Lines | Concern |
|-------|---------|
| 91583–91591 | Unload / labor — `colony+0x8e--`, order `'G'` |
| 91603–91616 | Goal-priority → order `'B'` |
| 92151–92167 | Fortify? colony-check → order `'F'`, dir=8 |
| 92176–92212 | Apply orders 5/6/0xc; idle → `'0'` |
| 92243–92255 | Naval + order `'1'` → `'B'`; clear when goal tile reached |

Post-act primary upsert for exhausted ships lives in **`6d8e`**, not here.

### 2b. Linux thin — naval war hunt (act-level)

When nation is at war with a Euro peer, ships **not in Europe** that are idle /
station-keeping get `AI_SAIL` toward the nearest enemy sea unit or coastal water
beside a foreign colony at war. Adjacent enemy ships call `ai_euro_try_attack` /
`units_resolve_naval_combat`. Deep `20e6` naval combat scoring stays **PARKED** (ocean/T3).

**Coastal Fort/Fortress fire (Marathon8):** EOT `units_coastal_fort_fire_pulse`
(`FUN_364b_03f6`). AI: flee battery adjacency before hunt; war-hunt skips
fort-fire tiles; ocean score −800 into batteries. Cite: fandom Fort/Fortress;
ship-slow formula still **PARKED**.

**Privateer deepen:** display-name Privateer always re-aims hunt (even with a prior
sail goto) — commerce-raid stand-in; reuse `naval_war_hunt_target`. Cite: Europe
Privateer purchase; fandom Drake Privateer combat strength.

**Privateer cargo prey (adjacent):** when choosing naval `try_attack` target,
prefer Merchantman/Caravel cargo ships over warships (then lower defense). Cite:
euro_unit_act §2f; Europe Privateer commerce raid.

**Frigate warship hunt (adjacent):** Frigate prefers warships (Frigate /
Privateer / Galleon / Man-O-War) over cargo when adjacent — complement Privateer
cargo prey; then lower defense. Cite: euro_unit_act §2f; Europe Frigate purchase.

**War transport deepen (Galleon/Frigate/Man-O-War):** at war, idle Galleon /
Frigate / **Man-O-War** with passenger space (`cargo_count < ship_capacity`)
prefers `AI_SAIL` toward coastal water by a **threatened** own coastal colony
(war-peer unit within MD≤3); else falls back to naval war hunt (foe sea / enemy
coast). Cite: Colonization.pdf naval transport; Europe purchase Galleon/Frigate;
Jones Frigate/MoW fallback; king_ref MoW. Full ships without space keep plain hunt.

### 2c. Linux thin — land war hunt (act-level)

When at war with a Euro peer, **or** Indian hostility sticky with a tribe/Brave
on the map, idle land military (Soldier / Dragoon / **Regular** /
**Continental** / Scout — including formerly fortified/sentry) get `AI_MOVE`
toward the nearest enemy land unit, enemy colony, or **at-war native Brave /
tribe tile** (prefer `tribe.state.capital`). Idle `FORTIFY` / `FORTIFIED` /
`SENTRY` are woken via `units_wake` then hunted. Adjacent → `ai_euro_try_attack`,
preferring the foe with lower effective defense (fortified ×2); Indian adjacent
requires `ai_diplo_indian_at_war`. Does not steal founders on FOUND goals.
Act-level hunt / peace-border / scout explore share thin MP-drain goto advance
with FOUND/MILITARY/CONTACT (§2c3). Full multi-step `20e6` combat scoring remains
**PARKED**. Cite: `ai_diplo_indian_*`; Cortes capital treasure path;
Colonization.pdf war / Defending a Colony.

**Alarmed tribe MILITARY (planning F):** friction>50 → MILITARY; capital tribes
prio 5 vs 3.

### 2c2. Linux thin — CONTACT scout rings (0a60 E / act)

Peace + own colonies ≥ 1: idle Scout upserts `AI_GOAL_CONTACT` at a Manhattan
ring tile (MD 2–4) around the nearest beyond-adjacent tribe and `AI_MOVE`s
toward it. When `map.seen` exists, prefer tiles **not** seen by the nation
(`map_tile_seen_by` / Col1 FoW bit) — explore intent, not combat bonuses.
When `ai_diplo_indian_hostility_sticky` ≥ 2 (`unknown26[8]` very-low deepen),
prefer **closer** rings (higher MD weight) when fog is absent. **Sticky + FoW:**
when `map.seen` exists, prefer **deeper unseen** ring tiles (md=4) to push fog
outward; act re-aims even with a prior CONTACT goto. Cite: `euro_diplo.md` /
`ai_diplo.h`; manual fog / Col1 seen bit.

**Fog explore (no CONTACT):** when no beyond-adjacent tribe ring exists, peaceful
Scout `AI_MOVE`s toward an unseen land tile within MD ≤ 8 (`map_tile_seen_by`)
without upserting CONTACT. Prefer `map_tile_has_rumour` over plain unseen when
both exist (Lost City Rumours seek; LCR resolve still on stand only — no invented
gold/FoY table). Plain Scout → nearest within the preferred tier; **Seasoned
Scout** → deeper (max md) within that tier — AI explore preference for the skill
"Better at exploring rumors…" (Colonization.pdf OTHER). Scouts already see 2
squares (de Soto: all units → "as well as scouts"); do **not** invent extra
sight radius or MP. Cite: Colonization.pdf Lost City Rumours / Seasoned Scout;
Pass5 LCR scaffold; manual fog / Col1 seen bit.

### 2c5. Linux thin — Treasure train coast (act)

Idle land unit named Treasure → `AI_MOVE` toward nearest **own coastal colony**
(`map_tile_is_coastal`). If none, nearest coastal land tile (Europe sail path
stand-in). Already on target → hold (park for Galleon / king transport). Cite:
Colonization.pdf Treasure Trains (six holds / coastal colony / king galleon for
a price). No invented ransom/gold. Preserve goto vs FOUND/LABOR yank.

**Treasure → Europe sail deepen:** when Treasure is already on a coastal own
colony and an own ship with passenger space is adjacent/same-tile →
`units_board` / `units_board_stacked` + ship `AI_SAIL` toward eastern high seas
(`units_find_eastern_high_seas_tile`) or eastward water (Europe exit stand-in).
Treasure passengers are skipped by settle unload. Ships with Treasure aboard
skip naval war-hunt yank. **Treasure → Europe gold (unparked):** when Treasure
(or ship carrying Treasure) is at Europe (`x/y≥200`) or on high seas, AI calls
`europe_cash_treasure` with COL1 `cargo_hold[0..1]` LE16 mirrored in
`hold_goods_amount[0..1]`; Treasure despawned. Value 0/unset → PARK (no invented
default). AI may also tick due Expected→Harbor (`cargo_treasure_gold`). Cite:
Colonization.pdf Treasure Trains; GAME.TXT `@LOOTCASH`. **PARK:** KINGGALLEON2
non-Cortes royal-galleon extra share (see `europe_cash_treasure`).

**Cortes KINGGALLEON3 coastal cash (unparked):** with FF Cortes, Treasure on an
own coastal colony cashes via `europe_cash_treasure` (tax = Crown share) without
boarding a ship (`ai_euro_try_cortes_king_galleon_cash`). Cite: fandom Hernan
Cortes; GAME.TXT `@KINGGALLEON3`.

### 2c6. Linux thin — Missionary CONTACT (act)

Peace + Missionary/Jesuit, **not fleeing** (adjacent tribe Alarm/friction ≥55 —
same band as `ai_contact` flee): upsert `AI_GOAL_CONTACT` (prio 3 > Scout ring
prio 2) at nearest tribe with `mission == 0xff` and `AI_MOVE` toward it. Idle
Jesuit prefers convert CONTACT over Scout explore / FOUND yank. Adjacent
convert lives in `ai_contact`. Cite: Colonization.pdf Establishing a Mission;
indian_contact.md.

### 2c3. Linux thin — multi-step land goto (FOUND / MILITARY / CONTACT / hunt)

Toward `AI_GOAL_FOUND`, `AI_GOAL_MILITARY`, or `AI_GOAL_CONTACT`, or when
act-level land war hunt / peace-border wake / scout explore set the goto,
scored advances **drain `moves_left`** in the same act (thin `20e6` MP
full-drain; was hard-cap 2). Full combat multi-step scoring stays **PARKED**.

### 2c4. Linux thin — multi-step naval sail (AI_SAIL)

Ships on `AI_SAIL` use scored ocean steps (same `ai_euro_score_move` /
`ai_euro_ocean_score_step` as land) and **drain `moves_left`** — mirror land
MP-drain. Replaces full `units_advance_goto` so HS west-explore bias applies
per step. Full ocean combat `20e6` stays **PARKED**.

### 2d. Linux thin — Pioneer tools delivery (case 7 economy stand-in)

Idle / arriving Pioneer or Hardy on an **own** colony tile when
`tools_short > 0` or colony `stock[TOOLS] < 20`: add **+10** tools
(cap 100) once per act; trim inventory `tools_short` and may decrement
`urgency`. Wired in `ai_euro_unit_act` just before LABOR/COLONY join.

**Wagon deepen (hire-once):** when a Wagon Train already exists and sits on a
tools-short colony with hold `TOOLS`, unload via `colonies_transfer_from_unit`
(structural cargo only). Pioneer delivery prefers this path when a wagon is on
the same tile before the +10 stand-in.

**Wagon haul (idle):** Wagon with free hold capacity or TOOLS / LUMBER / ORE /
MUSKETS / HORSES / FOOD cargo → `AI_MOVE` toward nearest matching short own
colony (`TOOLS`/`LUMBER`/`ORE`<20, `MUSKETS`/`HORSES`<10, food `<pop*2`). On a
surplus colony (tools/lumber/ore≥40 / muskets≥20 / horses≥20 / food≥pop*4) with
empty capacity, load that cargo via `colonies_transfer_to_unit` before hauling
(load order tools>lumber>ore>muskets>horses>food). Cite: manual Wagon Train
cargo; `COLONIZE_CARGO_*`; §2d unload delivery; 5cf6 lumber/ore_short.

### 2d2. Linux thin — Caravel/Merchantman/Galleon coastal haul (act)

Peace + idle Caravel/Merchantman/**Galleon** with goods-hold capacity or TOOLS /
LUMBER / ORE / MUSKETS / HORSES / FOOD cargo → `AI_SAIL` toward coastal water by
nearest own coastal colony short on that cargo (`TOOLS`/`LUMBER`/`ORE`<20,
`MUSKETS`/`HORSES`<10, food `stock < pop*2`). Adjacent short + matching hold →
`colonies_transfer_from_unit`; surplus (≥40 / ≥40 / ≥40 / ≥20 / ≥20 / ≥pop*4)
near ship → load same ladder as wagon. Cite: Colonization.pdf naval transport /
colony supply; euro_unit_act §2d haul pattern; docs/assets.md Europe purchase
ladder (Galleon). War hunt owns idle ships at war; Treasure Europe sail skips
haul.

**Europe export sail (unparked):** when supply haul does not bind the ship, load
FUN_364b_0636-eligible surplus (`stock>99` → leave 50; prefer Silver) at coastal
own colony, then `AI_SAIL` Europe for existing dump-sell. Cite: FUN_364b_0688 /
`europe_cargo_export_eligible`; Colonization.pdf Europe buy/sell; Custom House
denylist (not Food/Lumber/Horses/Tools/Muskets). No invented sell rates.

**Privateer loot sail:** peace Privateer already holding export-eligible goods
→ `AI_SAIL` Europe dump-sell (no colony load). Cite: Privateer commerce raid /
Europe sell; complements cargo-ship export.

**Wagon inland→coast export feeder:** when supply haul does not bind the wagon,
same FUN_364b load (prefer Silver) then `AI_MOVE` nearest own coastal colony;
on coastal tile unload export holds into colony stock for ship pickup. Cite:
§2d Wagon Train; §2d2 Europe export.

### 2d4. Linux thin — Jan de Witt foreign-colony TRADE_GOODS (act)

With FF Jan de Witt + peace: Wagon on foreign Euro colony tile loads
`TRADE_GOODS` surplus (stock≥20 → 10; same muskets haul chunk) via
`colonies_de_witt_transfer_from_colony`, then `AI_MOVE` toward nearest own
colony and `colonies_transfer_from_unit` unload into warehouse (delivery loop).
Empty wagon may `AI_MOVE` toward nearest foreign surplus. Cargo ships: same load
on foreign dock (ships may enter foreign Euro docks when de Witt + peace via
`units_can_enter` + `g_units_ff_col1`); with TRADE_GOODS aboard → `AI_SAIL`
Europe (existing dump-sell). Stock transfer only — no gold/price.
Cite: docs/fandom_col1994.md Jan de Witt; `colonies_de_witt_transfer_*`.

### 2d3. Linux thin — peace colony garrison fortify (act)

Peace + idle Soldier / Dragoon / **Regular** / **Continental** (Army/Cavalry) on
own colony tile → `units_order_fortify` if not already fortified (overrides
explore/FOUND yank while on-tile; keeps off-colony MILITARY/CONTACT). Cite: case
0x0b fortify arm (`'F'`); Colonization.pdf Defending a Colony ("fortify
soldiers, dragoons, army, cavalry…"). At war: wake+hunt (§2c).

**Peace Artillery fortify:** idle Artillery/Cannon on own colony (peace or war)
→ `units_order_fortify` (same case 0x0b `'F'` arm; PDF "…or artillery"). At war
off-colony: siege hunt toward fortified foreign Euro colonies (Stockade+).

**Peace colony-defense wake (MD≤2):** fortified/idle garrison above **or
Artillery/Cannon** on own colony wakes via `units_wake` when a foreign Euro land
unit is within Manhattan ≤2, then `AI_MOVE` toward that threat (adjacent
`try_attack` may declare war). Extends peace fortify border; war already has
global fortify-wake (§2c). Cite: Colonization.pdf Defending a Colony ("fortify
soldiers, dragoons… or artillery"); `units_wake`.

**Artillery fortify after siege:** covered by peace/war Artillery fortify above.

 **5d04 peace hire (thin, not full case-7 body):** `tools_short>30` + Wagon
 Train/Supply Train/Wagon type → hire wagon **once** (TOOLS loaded on wagon
 before board); else `tools_short>20` prefer Pioneer/Hardy + ship/colony tools
 cargo. Case-7 deepen: prefer Hardy/Expert Pioneer or Master Carpenter already
 on Europe dock (consume dock slot; no free expert spawn). **`tools_short>20`**
 dock miss → prefer Master Blacksmith on Europe dock (Ore→Tools). **`food_short>20`:**
 prefer Expert Farmer on Europe dock (same consume pattern); if Farmer miss and
 nation has a coastal own colony, prefer Expert Fisherman on dock (coastal food
 fallback). **Construction LABOR:**
 when any colony has Stockade/Warehouse/Lumber Mill/Drydock/Shipyard incomplete
 (`ai_euro_colony_wants_construction_labor`), prefer Master Carpenter on Europe
 dock (same consume / `hire_cost`; not tools/food short). **`lumber_short>20`:**
 when any colony wants lumberjack LABOR or has construction in progress with low
 lumber stock, prefer Expert Lumberjack on Europe dock (same consume pattern).
 **`ore_short>20`:** Ore stock&lt;20 tallies → prefer Expert Ore/Silver Miner on
 Europe dock (same consume). **`muskets_short>20`:** Muskets stock&lt;10 tallies →
 prefer Master Gunsmith on Europe dock (same consume). **Unmissioned tribe:**
 prefer Jesuit/Missionary on Europe dock (convert CONTACT; before Seasoned Scout).
 **Peace + colonies≥1:**
 prefer Seasoned Scout on Europe dock (CONTACT / fog explore) when no higher
 shortage hire wins; else Elder Statesman (Town Hall liberty bells).
 **Church/Cathedral present:** prefer Firebrand Preacher on Europe dock (crosses).
 **Craft building + raw≥20:** prefer Master Distiller/Weaver/Tobacconist/Fur Trader
 on Europe dock (Sugar/Cotton/Tobacco/Furs → Rum/Cloth/Cigars/Coats).
 Cite: europe.c expert pools; building_production /
 terrain_yields; euro_unit_act §2e field-assign / §2c2 / §2c6. Treasury: skip hire /
tools-cargo when gold &lt; colonist `hire_cost`; Artillery uses Europe purchase
**500$** (fall back to Soldier when underfunded). **At war + tools_short:** still
prefer Soldier/Dragoon hire over Pioneer (profession_demand Pioneer is peace-only).
**At war + own colonies ≥ 3:** prefer Dragoon hire when type exists (same
`hire_cost`; fall back to Soldier if Dragoon missing). **At war + own colonies
≥ 2:** prefer Veteran Soldier when type exists and gold covers cost (`@UNIT`
cost, else NAMES `@JOB` Soldier→Veteran Soldiers **2000$**). Missing type/cost
→ plain Soldier (**PARK** comment). Cite: `COLONIZE/NAMES.TXT` `@JOB`.
**Ship board military:** at war, idle Soldier / Dragoon / **Regular** /
**Continental** (Army/Cavalry) / Artillery/Cannon on coastal own colony boards
an empty transport (`cargo_count==0`) with passenger space via `units_board` /
`units_board_stacked` before hunt yank / Artillery on-colony fortify — **except**
when the colony is threatened (stay to defend). Cite: Colonization.pdf naval
transport / Defending a Colony ("fortify soldiers, dragoons, army, cavalry, or
artillery").
**Ship unload military:** at war, ship with military cargo adjacent to own
threatened coastal colony (war-peer MD≤3) unloads one passenger onto the colony
tile — prefer Soldier, else Regular/Continental Army, else Dragoon/Continental
Cavalry, else Artillery (mirror king MoW unload ladder + board list) via
`units_unload_passenger` (before move-scoring gate + after sail). Cite:
Colonization.pdf naval transport / Defending a Colony; king_ref MoW unload;
complements board + war-transport sail-to-threatened-port.
**Done:** transport at Europe dump-sells all commodity holds with Europe bid via
`europe_sell_unit_hold` / `europe_sell_proceeds` (tax); nat↔europe gold sync
(Merchantman/Caravel/Galleon **and Privateer** — `units_is_transport` holds).
Skips holds whose cargo type bit is set in `nation.boycott_bitmap` (wiki Boycott /
king refuse — goods blocked in Europe; no invented prices). Cite: Colonization.pdf
Europe buy/sell + tax; fandom Boycott (Col).
**Pioneer plow/road (unparked):** idle Hardy/Expert Pioneer with tools picks a
nearby own-colony surround → `AI_MOVE` then on-tile `units_pioneer_plow`
(clear forest then plow in one API) / `units_pioneer_road`. Prefer plow over
road; among roads prefer tiles **already plowed** (Clear/Plow/Road sequence).
Hardy real power: "Clears forest, plows fields, and builds roads faster"
(Colonization.pdf) — prefer Hardy when both idle; no invented yields. Skip when
`tools_short` or on-colony construction LABOR stay. Cite: Colonization.pdf
Clear/Plow/Road. Remaining mid `5d04` wagon matrix / deep combat tails stay
**OPEN** (unpark #4).

### 2e. Linux thin — LABOR bind (food/tools short + construction)

Idle colonist-capable land unit (Pioneer/Hardy/Free Colonist/Colonist) within
MD≤1 of an own colony when inventory `tools_short` or `food_short` and the
colony is locally short → upsert `AI_GOAL_LABOR` and goto (overrides distant
FOUND). On-tile Pioneer/Hardy skip LABOR-join so tools-delivery stand-in is not
stacked with founder-loot dump — **except** when `building_in_production` is
**Stockade**, **Warehouse**, or **Lumber Mill** (carpenter hammers bind;
stay/LABOR rather than leave). Cite: `docs/building_production.md`. Colony
planning also upserts LABOR for those projects. No invented production numbers.

**Threatened Stockade LABOR:** when at war and a war-peer unit is within MD≤3
of an own colony with incomplete **Stockade**, idle Free Colonist within MD≤3
prefers that Stockade LABOR (prio bump) over distant FOUND. Cite:
`building_production.md` Stockade defense; Colonization.pdf fortify;
`ai_euro_colony_threatened_by_war`.

**Food emergency:** when inventory `food_short` ≥ 4, nearest food-capable
colonist/Pioneer within MD≤8 is bound to a hungry colony LABOR (planning + act).
Cite: manual 2 food/colonist; `5cf6` shortage tallies.

**Expert Farmer food LABOR:** idle Expert Farmer (display-name Farmer, or Free
Colonist/Colonist with `@JOB` Farmer profession 0) → food-short LABOR (MD≤8
when food_short). Cite: `docs/building_production.md` Farmer→Food; Skills Chart.
No invented food rates — LABOR join only.

**Free Colonist food LABOR (non-Expert Farmer):** idle Free Colonist / Colonist
(without Farmer profession) with `food_short` > 0 → MD≤8 toward a hungry own
colony LABOR join (same structural join as Expert Farmer path). Adjacent still
covers tools/construction; MD>1 is food-short only. Cite: manual 2 food/colonist;
5cf6 food_short; euro_unit_act §2e.

**Master Carpenter construction LABOR:** idle Master Carpenter → LABOR when
own colony has Stockade/Warehouse/Lumber Mill incomplete (`building_in_production` —
same Stockade pattern as Pioneer stay). Cite: `docs/building_production.md`
Carpenter→Hammers; Skills Chart Master Carpenter. Construction-only bind
(not tools/food). No invented hammer rates.

**Expert Lumberjack LABOR:** idle Expert Lumberjack → LABOR when own colony has
incomplete **Warehouse** or **Lumber Mill** and that building type exists in
the pool (lumber feeds carpenter hammers). Cite: `docs/building_production.md`
Lumberjack→Lumber; Colonization.pdf Skills Chart. Structural LABOR join only.

**Tools-short Pioneer deepen (peace):** when inventory `tools_short` > 0, idle
peace Pioneer/Hardy within MD≤8 is LABOR-bound toward a tools-short colony
(feeds on-tile §2d tools delivery). Cite: euro_unit_act §2d; 5cf6 tools tallies.

**PARK:** Custom House per-cargo UI chrome (`FUN_15eb_0326`). Drydock /
Shipyard prefer already wired via `colonies_list_buildable` +
`colonies_set_construction`.

**Stuyvesant Custom House construction prefer:** when nation owns Peter
Stuyvesant (`founding_fathers_nation_has` / `has_peter_stuyvesant`), idle
colony without Custom House queues it after Drydock→Shipyard prefer.
Cite: docs/fandom_col1994.md Stuyvesant; colony.c Custom House gate;
founding_fathers elect comment.

**Peace Church construction prefer:** idle colony with Stockade already owned,
no Church/Cathedral → queue Church when buildable (after defense/storage/naval/
Custom House prefers). Cite: building_production.md Church→Crosses;
Colonization.pdf Church / immigration.

**Wartime Armory construction prefer:** at war with a Euro peer, idle colony
with Stockade, no Armory/Magazine/Arsenal → queue Armory when buildable (after
Church prefer so wartime muskets beat crosses). Cite: building_production.md
Armory Tools→Muskets; Colonization.pdf Defending a Colony.

**Wartime Magazine construction prefer:** at war, Armory owned, no Magazine/
Arsenal → queue Magazine when buildable. Cite: building_production.md Magazine
doubles musket output.

**Peace Printing Press construction prefer:** idle colony with Stockade+Church
owned, no Printing Press/Newspaper → queue Printing Press when buildable.
Cite: building_production.md Printing Press +50% liberty bells.

**Peace Schoolhouse construction prefer:** idle colony with Stockade, pop≥4, no
Schoolhouse/College/University → queue Schoolhouse when buildable (after Press).
Cite: building_production.md Schoolhouse teach faculty 1.

**Peace Newspaper construction prefer:** Printing Press owned → Newspaper when
buildable. Cite: building_production.md Newspaper +100% liberty bells.

**Peace College construction prefer:** Schoolhouse owned, pop≥8 → College when
buildable. Cite: building_production.md College faculty 2.

**Peace University construction prefer:** College owned, pop≥10 → University
when buildable. Cite: building_production.md University faculty 3.

**Peace Cathedral construction prefer:** Church owned, pop≥8 → Cathedral when
buildable. Cite: building_production.md Cathedral crosses.

**Wartime Arsenal construction prefer:** at war, Adam Smith elected, Magazine
owned, no Arsenal → queue Arsenal when buildable. Cite: building_production.md
Arsenal factory muskets (Adam Smith); Colonization.pdf.

**Stable construction prefer:** fortified (Stockade/Fort/Fortress), no Stable →
queue Stable when buildable (peace or war). Cite: building_production.md Stable
horse breeding.

**Carpenter's Shop / Lumber Mill construction prefer:** idle → Shop when unmet;
Shop owned → Lumber Mill. Cite: building_production.md lumber chain.

**Blacksmith's House / Shop / Iron Works construction prefer:** ore≥20 →
House; House owned → Shop; Adam Smith + Shop → Iron Works. Cite:
building_production.md Ore→Tools / factory tools (Adam Smith).

**Craft shop/factory construction prefer:** Distiller/Weaver/Tobacconist/Fur
House→Shop→Factory when raw stock≥20; factories need Adam Smith. Cite:
building_production.md craft chains; dock craft hire stock≥20 gate.

**Capitol / Capitol Expansion construction prefer:** fortified → Capitol;
Capitol owned → Expansion. Cite: building_production.md Capitol liberty bells.

**Custom House auto-sell:** `europe_custom_house_autosell` from
`turn_produce` / `turn_run_colony_production` — `FUN_364b_0688` stock>99
leave 50; `FUN_364b_0636` denylist (not Food/Lumber/Horses/Tools/Muskets);
boycott bypass; WoI (`unknown46[0]`) untaxed.

**Expert Lumberjack forest field-assign (unparked):** idle Expert Lumberjack →
admit + `colonies_assign_field` on a free forest surround (pedia 8–23) with
`COLONIZE_JOB_LUMBERJACK`. Off-tile MD≤8 → LABOR goto. Warehouse/Lumber Mill
LABOR join remains the no-forest fallback. Cite: terrain_yields /
building_production Lumberjack→Lumber; Colonization.pdf Skills Chart. No
invented lumber rates.

**Expert Ore Miner / Silver Miner field-assign (unparked):** idle Expert Ore
Miner / Silver Miner → admit + `colonies_assign_field` on a free surround with
positive Ore/Silver yield (`COLONIZE_JOB_ORE_MINER` / `_SILVER_MINER`). Off-tile
MD≤8 → LABOR goto. Cite: terrain_yields Ore/Silver; Colonization.pdf Skills
Chart. Parallel to Lumberjack forest field-assign. No invented rates.

**Expert Farmer food field-assign (unparked):** idle Expert Farmer (display-name
Farmer, or Free Colonist/Colonist with `@JOB` Farmer profession 0) → admit +
`colonies_assign_field` on a free surround with positive Farmer food yield
(prefer higher `colony_yield_for_tile`). Off-tile MD≤8 → LABOR goto. Food-short
LABOR join remains the no-field fallback. Cite: terrain_yields / building_production
Farmer→Food; Colonization.pdf Skills Chart. Parallel to Lumberjack/Ore Miner.
No invented food rates.

**Expert Fisherman coastal field-assign (unparked):** idle Expert Fisherman
(display-name Fisherman, or Free Colonist/Colonist with `@JOB` Fisherman
profession 8) → admit + `colonies_assign_field` on a free ocean/sea-lane surround
(pedia 25–26) with positive Fisherman yield. Off-tile MD≤8 → LABOR goto. Cite:
terrain_yields Fisherman (Ocean/Sea Lane fish); building_production; Skills Chart.
Parallel to Farmer field-assign. No invented fish rates.

**Expert Sugar / Tobacco / Cotton Planter + Fur Trapper field-assign
(unparked):** idle Expert Sugar/Tobacco/Cotton Planter or Fur Trapper → admit +
`colonies_assign_field` on a free surround with positive matching yield
(`COLONIZE_JOB_SUGAR_PLANTER` / `_TOBACCO_PLANTER` / `_COTTON_PLANTER` /
`_FUR_TRAPPER`; prefer higher `colony_yield_for_tile`). Off-tile MD≤8 → LABOR
goto. Cite: terrain_yields Sugar/Tobacco/Cotton/Fur; Colonization.pdf Skills
Chart. Parallel to Farmer field-assign. No invented crop rates.

**Elder Statesman / Firebrand Preacher / Expert Teacher / Master Carpenter
workplace assign (unparked):** idle Elder Statesman → Town Hall; Firebrand
Preacher → Church else Cathedral; Expert Teacher → Schoolhouse else College else
University; Master Carpenter → Carpenter's Shop else Lumber Mill (highest owned;
construction LABOR join remains fallback without Shop/Mill). Off-tile MD≤8 →
LABOR goto. Cite: building_production.md Skills Chart jobs 13, 16–18;
Colonization.pdf. Parallel to craft workplace assign. No invented rates.

**FOUND on Indian homeland:** `colonies_found_with_indian_land` (FUN_4cc6_07c2
gold charge; Minuit FF 2 → free). Short gold → PARK (no despawn); thin human
`ctx->status` when cost>0 and gold short. Cite: Colonization.pdf Minuit /
indian land purchase; `colonies_indian_land_purchase_gold`.

**Pioneer plow/road** — see §2d (unparked).

### 2f. Linux thin — naval adjacent-foe pick

Like land adjacent-foe: when choosing naval `try_attack` target —
**Privateer** prefers Merchantman/Caravel cargo over warships; **Frigate**
prefers warships over cargo (complement); else lower type defense
(`ai_euro_naval_best_adjacent_foe`). **Done (thin FUN_157e_004a):** vet Soldier/
Dragoon profession `0x15` +50% land toughness; Drake Privateer +50% naval
toughness. **PARKED:** ship damage byte `0x3150` subtract (no named live field).

**PARK:** Wagon load FOOD — euro AI uses FOOD only for colony stock shortage /
LABOR tallies; haul loads TOOLS / MUSKETS / HORSES (§2d / §2d2). No wagon FOOD
cargo path.

**Seasoned + sticky fog deepen:** Seasoned Scout fog-explore with
`ai_diplo_indian_hostility_sticky` ≥ 2 and `map.seen` deepens a shallow prior
goto once at fresh MP (`pick_md > goto_md`) — mirror CONTACT sticky deepen
without max-md walk drift on dispatcher sticky waves. Cite: Colonization.pdf
Seasoned Scout; euro_unit_act §2c2.

**Done (thin Artillery siege / Dragoon open):** off-colony Artillery at war hunts
fortified foreign Euro colonies (Stockade+; MD slack ≤3 vs open); adjacent-foe
prefers higher fort %. Dragoon hunt prefers open colonies (leave forts to
Artillery). On own colony Artillery still FORTIFY. Cite: king_ref Artillery
siege / Dragoon open bias; Colonization.pdf Artillery.

**PARK:** deep `FUN_521d_20e6` combat scoring (terrain/artillery tables,
multi-hex threat weights) — thin adjacent-toughness pick includes fortified ×2,
colony Stockade/Fort/Fortress %, and FUN_157e_004a vet/Drake +50% peels +
2-step goto only.

**Done:** Treasure → Europe gold via `europe_cash_treasure` (LE16 hold value;
despawn; Expected→Harbor tick). **PARK:** value unset / KINGGALLEON2 extra share.

### 2g. Linux thin — ocean west-explore / east-Europe HS bias

When ship is on high seas and goto is westward, ocean `20e6` score prefers
westward HS steps (structural score only; no invented MP). When goto is
eastward (Treasure/Europe exit / eastern HS), prefer eastward HS steps —
complement west-explore. Cite: Colonization.pdf Treasure Trains → Europe;
`units_find_eastern_high_seas_tile`. Full ocean branch still R5 / PARKED.

### 3. Combat / diplomacy tails (**OPEN** mid-planner; Indian raid deep PARKED)

Land combat act tails deepen with unpark #4; Indian raid deep bodies stay PARKED.

## Naval type band note

Decomp often tests `type ∈ (0x0c, 0x13)` (open upper). Annotated
`SHIP_A..C = 0x0a..0x0c` is the dispatcher ship-wave set — **do not conflate**
with the wider naval cargo band inside `20e6` / `0a60`.

## Related symbols

| Symbol | Role |
|--------|------|
| `FUN_521d_20e6` | Direction / move scoring (`04f4` @90557) |
| `FUN_521d_06ae` | Best adjacent founding tile (from `20e6` @89587 only) |
| `FUN_521d_016a` | Upsert primary goal |
| `FUN_1427_*` / `281f_09xx` | MP chrome after steps |

## Exit criteria for a future deep extract

- Sectioned `.c` with provenance headers
- Ship unload + founding-order arms readable end-to-end
- Explicit **OPEN** remainder for land combat / case 7 hire (thin tools-delivery today)
- Ocean naval `20e6` + full line-by-line still R5 / PARKED
- `SYMBOL_MAP` + catalog `links` updated
