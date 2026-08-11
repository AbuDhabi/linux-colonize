# Settlement building production

Reference for what colonists produce **inside** colony buildings (settlement / town-commons view), which `@JOB` skills apply, and how that relates to the Production tab and worker badges. Field / area yields are documented separately in [terrain_yields.md](terrain_yields.md).

## Sources

| Source | Role |
|--------|------|
| [`COLONIZE/Colonization.pdf`](../COLONIZE/Colonization.pdf) | Manual ch. 6 (colonies, pp. 37–58): workplace rules, Production view, shortfalls, Sons of Liberty production bonus/penalty; player-aid **Skills Chart** and **Building Chart** (back cover) |
| [`COLONIZE/NAMES.TXT`](../COLONIZE/NAMES.TXT) `@BUILDING`, `@JOB` | Building costs / min population; colonist profession names and school tier |
| [`COLONIZE/VICEROY.EXE`](../COLONIZE/VICEROY.EXE) | Embedded tier table **`3, 6, 8`** (file offset **0x16103**) — house / shop / factory manufacturing throughput per worker (before colonist-class and skill multipliers) |
| [`COLONIZE/README.TXT`](../COLONIZE/README.TXT) | Colony **Space** = one free production cycle; Cathedral min pop **8** (v3 fix) |
| Community reverse-engineering (NamuWiki, StrategyWiki) | Cross-check for per-building **6→9** factory efficiency and church / town-hall self-production numbers where the manual gives prose but not a table |

When the manual player-aid chart disagrees with `NAMES.TXT` on **construction** costs (e.g. Carpenter’s Shop hammers), prefer **`NAMES.TXT`** for hammer/tool **costs** and the manual chart for **gameplay effects**.

---

## Scope: settlement vs area

| Location | View | Produces |
|----------|------|----------|
| **Area** (8 surround tiles + town commons) | Area view | Raw commodities: food, sugar, tobacco, cotton, furs, lumber, ore, silver — see [terrain_yields.md](terrain_yields.md) |
| **Buildings** (town commons) | Settlement view | Processed goods, hammers, muskets, crosses, liberty bells, teaching, etc. |

Up to **three** colonists may work the same building at once (school/college/university are special: teachers + students).

The printed manual says converts are “unwilling to work inside manufacturing concerns” (p. 33); that is **misleading**. Converts **can** be assigned to buildings — they are simply very bad at manufacturing (same floor rate as petty criminals). On **tiles** they are better than free colonists (see field note below).

---

## Turn order (settlement production)

Classic order (manual Production view; `README.TXT` “Space = free production”):

1. **Harvest** — town-commons auto-yield + assigned field workers add to warehouse (same turn).
2. **Food** — each colonist eats **2 food**; surplus accumulates toward growth (**200**
   food → new free colonist — **Done** in `turn_produce_one_colony`). Starvation
   latch + second-turn colonist loss (**Done**; last colonist preserved).
3. **Manufacturing** — workers in processing buildings convert **input cargo → output cargo** from warehouse stock (including goods harvested this turn). Unmet input demand creates **shortfalls** (grey / “X” icons in Production view).
4. **Hammers** — carpenters at Carpenter’s Shop / Lumber Mill consume **lumber** toward `building_in_production`.
5. **Crosses / liberty bells** — accumulated into immigration / independence counters (not normal warehouse cargo).

Manufacturing runs **before** carpenters spend lumber so ore→tools and cotton→cloth happen in the same pass as field ore/cotton arriving.

---

## Worker output: colonist class

For **manufacturing** jobs (settlement buildings). Rates below are the **free-colonist equivalent** before building-tier tables; criminals / converts / servants use a reduced floor instead of the free-colonist “3” baseline.

| Colonist type | `@JOB` index | Manufactured units / worker / turn (house / base tier) |
|---------------|-------------:|-------------------------------------------------------:|
| Petty criminal | 26 | **1** |
| Indian convert | 27 | **1** (same floor as criminal; **can** work buildings) |
| Indentured servant | 25 | **2** |
| Free colonist (unskilled) | 19 | **3** |

Higher building tiers scale the same way for free colonists (3 → 6 → 9); criminals / converts stay at the low manufacturing floor (**1** at house tier — confirm shop/factory scaling against DOS when implementing).

### Matching skill vs wrong job

A colonist’s specialty applies **only** when working the matching job:

| Situation | Production treated as |
|-----------|------------------------|
| Skill matches workplace / field job (e.g. Master Blacksmith in a smithy; Expert Fisherman on ocean) | Expert / Master rates (**×2** manufacturing; field experts double tile yield) |
| Skill does **not** match (e.g. Master Sugar Planter in a Lumber Mill; Elder Statesman as Fisherman; Master Blacksmith in a Tobacconist’s House) | **Free colonist** for that assignment — no specialty bonus |

So a mismatched expert is not worse than a free colonist; they simply lose the expert multiplier until reassigned to their trade.

| Skill tier (when matched) | Multiplier on manufacturing output (and input) |
|---------------------------|-----------------------------------------------|
| No matching skill / wrong building | ×1 (free-colonist baseline) |
| Master / Expert / Elder / Firebrand in matching role | **×2** |

Examples:

- Miner **3** ore + free blacksmith **3** tools → balanced.
- Same miner + **Master Blacksmith** wanting **6** tools → **shortfall 3** unless ore is in stock.
- Elder Statesman fishing → free-colonist fisherman yield (no statesman bonus, no fisherman expert bonus).

### Field (area) note — converts

On **tiles**, Indian converts produce **1 more** than a free colonist of the same job on that terrain (before expert multipliers / SoL). Details belong with area yields in [terrain_yields.md](terrain_yields.md); kept here so settlement vs field class rules stay consistent.

---

## Building tier throughput (free colonist, per worker)

`VICEROY.EXE` stores three manufacturing rates **`3 / 6 / 8`**. In practice these align with the player-aid **Building Chart** “increases production” tiers and with observed **3 / 6 / (6→9)** processing:

| Tier | Building level (examples) | Free-colonist **output** / worker | Typical **input** / worker |
|------|---------------------------|----------------------------------:|---------------------------:|
| **1 — House / base** | `*'s House`, Carpenter’s Shop, Armory, Town Hall (bells), Church (crosses) | **3** | **3** (1:1) |
| **2 — Shop / mill** | `*'s Shop`, Lumber Mill, Rum Distillery, Fur Trading Post, Magazine | **6** | **6** (1:1) |
| **3 — Factory** | `* Factory`, Textile Mill, Iron Works, Arsenal (**Adam Smith** required) | **9** | **6** (1.5× output efficiency) |

**Exceptions / special cases**

| Building | Notes |
|----------|--------|
| **Lumber Mill** | Manual: **doubles** hammer output vs Carpenter’s Shop (3→**6**); still consumes lumber 1:1. |
| **Magazine** | Manual: **doubles** musket output vs Armory (3→**6** tools→muskets). |
| **Arsenal** | Manual: needs **half the tools** of Magazine/Armory to make the same muskets at factory tier; data tables use **9 muskets ← 9 tools** per worker (same 1:1 ratio as other factories, but half the tools Magazine would need for 9 muskets). |
| **Iron Works / luxury factories** | **6** raw → **9** finished (ore→tools, cotton→cloth, …). |
| **Printing Press** | **+50%** liberty bell production colony-wide (manual p. 58). |
| **Newspaper** | **+100%** liberty bell production colony-wide. |
| **Church / Cathedral** | Building provides **passive** crosses; workers add more (data: Church **+2** passive + **3**/worker; Cathedral **+3** + **6**/worker). Colony also generates **1** cross/turn base (manual p. 35). |
| **Town Hall** | Passive **+1** bell + **3**/worker (data); **Elder Statesman** ×2. |
| **Dock** | Enables fishermen on ocean/lake area tiles (no processing). |
| **Schoolhouse / College / University** | Teach skills (faculty 1 / 2 / 3); see Skills table below. |

---

## Processing chains (input → output)

Each row is one **recipe**; higher building tiers replace lower ones in the same chain (only the **best built** building of a slot chain is shown in the settlement view, but workers are assigned to a specific `building_type`).

| Output | Input | Tier-1 building | Tier-2 | Tier-3 (needs **Adam Smith**) |
|--------|-------|-------------------|--------|--------------------------------|
| **Hammers** | Lumber | Carpenter’s Shop | Lumber Mill | — |
| **Tools** | Ore | Blacksmith’s House | Blacksmith’s Shop | Iron Works |
| **Muskets** | Tools | Armory | Magazine | Arsenal |
| **Cloth** | Cotton | Weaver’s House | Weaver’s Shop | Textile Mill |
| **Cigars** | Tobacco | Tobacconist’s House | Tobacconist’s Shop | Cigar Factory |
| **Rum** | Sugar | Rum Distiller’s House | Rum Distillery | Rum Factory |
| **Coats** | Furs | Fur Trader’s House | Fur Trading Post | Fur Factory |
| **Crosses** | — | Church | — | Cathedral |
| **Liberty bells** | — | Town Hall (+ Printing Press / Newspaper bonuses) | — | — |

**Starter colonies** receive tier-1 free manufacturing: Town Hall, Carpenter’s Shop, Blacksmith’s House, Weaver’s / Tobacconist’s / Distiller’s / Fur Trader’s **House** (`colonies_grant_starters`).

---

## Skills chart (`@JOB` → workplace)

Indices match [`COLONIZE/NAMES.TXT`](../COLONIZE/NAMES.TXT) `@JOB` order and Col1 `profession` bytes.

### Outdoorsmen (area view only)

| @JOB | Index | Expert name | Produces | Teach school |
|------|------:|-------------|----------|--------------|
| Farmer | 0 | Expert Farmer | Food | S* |
| Sugar Planter | 1 | Master Sugar Planter | Sugar | C* |
| Tobacco Planter | 2 | Master Tobacco Planter | Tobacco | C* |
| Cotton Planter | 3 | Master Cotton Planter | Cotton | C* |
| Fur Trapper | 4 | Expert Fur Trapper | Furs | S* |
| Lumberjack | 5 | Expert Lumberjack | Lumber | S |
| Ore Miner | 6 | Expert Ore Miner | Ore | S |
| Silver Miner | 7 | Expert Silver Miner | Silver | S |
| Fisherman | 8 | Expert Fisherman | Food (fish) | S* |

\* = may also be learned from natives (manual Skills Chart).

School codes: **S** = Schoolhouse, **C** = College, **U** = University (manual player-aid footnote).

### Craftsmen (settlement buildings)

| @JOB | Index | Master / expert title | Workplace buildings | Converts |
|------|------:|----------------------|---------------------|----------|
| Carpenter | 13 | Master Carpenter | Carpenter’s Shop, Lumber Mill | Lumber → **Hammers** |
| Blacksmith | 14 | Master Blacksmith | Blacksmith’s House/Shop, Iron Works | Ore → **Tools** |
| Gunsmith | 15 | Master Gunsmith | Armory, Magazine, Arsenal | Tools → **Muskets** |
| Distiller | 9 | Master Distiller | Rum Distiller’s House/Distillery/Factory | Sugar → **Rum** |
| Tobacconist | 10 | Master Tobacconist | Tobacconist’s House/Shop, Cigar Factory | Tobacco → **Cigars** |
| Weaver | 11 | Master Weaver | Weaver’s House/Shop, Textile Mill | Cotton → **Cloth** |
| Fur Trader | 12 | Master Fur Trader | Fur Trader’s House, Trading Post, Factory | Furs → **Coats** |

Wrong-building assignment → free colonist rates (see [Matching skill vs wrong job](#matching-skill-vs-wrong-job)).

### Political / religious (settlement)

| @JOB | Index | Title | Building | Produces |
|------|------:|-------|----------|----------|
| Preacher | 16 | Firebrand Preacher | Church, Cathedral | **Crosses** (×2 when skilled) |
| Statesman | 17 | Elder Statesman | Town Hall | **Liberty bells** (×2 when skilled) |
| Teacher | 18 | Expert Teacher | Schoolhouse, College, University | Trains other colonists (not cargo) |

### Not manufacturing

| @JOB | Index | Role |
|------|------:|------|
| Colonist | 19 | Free colonist — generic worker |
| Pioneer / Soldier / Scout / Dragoon | 20–23 | Map units (Hardy Pioneer, Veteran Soldier, …) |
| Missionary | 24 | Missions (Church helps create missionaries) |
| Ind. Servant / Criminal / Convert | 25–27 | Class rates: servant **2**, criminal/convert **1** in buildings; convert **+1** vs free on tiles |

---

## Building chart (construction)

From manual player-aid **Building Chart** + [`NAMES.TXT`](../COLONIZE/NAMES.TXT) `@BUILDING`.  
**Ham** = hammers to complete; **Tools** = tools to finish. `NAMES.TXT` stores `tools(*10)` (e.g. `2` → **20** tools); the port multiplies by 10 on load.

| Building | Ham | Tools | Min pop | Effect (summary) |
|----------|----:|------:|--------:|------------------|
| **Town Hall** | 0 | 0 | 1 | Liberty bells |
| **Carpenter’s Shop** | 0† | 0 | 1 | Lumber → hammers |
| **Lumber Mill** | 52 | 0 | 3 | Doubles hammer production |
| **Blacksmith’s House** | 0† | 0 | 1 | Ore → tools |
| **Blacksmith’s Shop** | 64 | 20 | 4 | Increases tool production |
| **Iron Works** | 240 | 100 | 8 | Factory tool production (**Adam Smith**) |
| **Weaver’s House** | 0† | 0 | 1 | Cotton → cloth |
| **Weaver’s Shop** | 64 | 20 | 4 | Increases cloth production |
| **Textile Mill** | 160 | 100 | 8 | Factory cloth (**Adam Smith**) |
| **Tobacconist’s House** | 0† | 0 | 1 | Tobacco → cigars |
| **Tobacconist’s Shop** | 64 | 20 | 4 | Increases cigar production |
| **Cigar Factory** | 160 | 100 | 8 | Factory cigars (**Adam Smith**) |
| **Rum Distiller’s House** | 0† | 0 | 1 | Sugar → rum |
| **Rum Distillery** | 64 | 20 | 4 | Increases rum production |
| **Rum Factory** | 160 | 100 | 8 | Factory rum (**Adam Smith**) |
| **Fur Trader’s House** | 0† | 0 | 1 | Furs → coats |
| **Fur Trading Post** | 56 | 20 | 3 | Increases coat production |
| **Fur Factory** | 160 | 100 | 6 | Factory coats (**Adam Smith**) |
| **Armory** | 52 | 0 | 1 | Tools → muskets; enables artillery |
| **Magazine** | 120 | 50 | 8 | Doubles musket production |
| **Arsenal** | 240 | 100 | 8 | Factory muskets (**Adam Smith**) |
| **Stockade** | 64 | 0 | 3 | Defense +100% |
| **Fort** | 120 | 100 | 4 | Defense +150%; upgrade stockade |
| **Fortress** | 320 | 200 | 8 | Defense +200%; upgrade fort |
| **Dock** | 52 | 0 | 1 | Fishing on sea/lake tiles |
| **Drydock** | 80 | 50 | 6 | Ship repair |
| **Shipyard** | 240 | 100 | 8 | Ship construction |
| **Schoolhouse** | 64 | 0 | 4 | Teach (faculty 1) |
| **College** | 160 | 50 | 8 | Teach (faculty 2) |
| **University** | 200 | 100 | 10 | Teach all skills (faculty 3) |
| **Warehouse** | 80 | 0 | 1 | +100 storage |
| **Warehouse Expansion** | 80 | 20 | 1 | +100 storage |
| **Stable** | 64 | 0 | 1 | Horse breeding (≥2 horses + food surplus; Stable raises cap) |
| **Church** | 52 | 0 | 3 | Crosses; missionaries |
| **Cathedral** | 176 | 100 | 8 | More crosses |
| **Printing Press** | 52 | 20 | 1 | +50% liberty bells |
| **Newspaper** | 120 | 50 | 4 | +100% liberty bells |
| **Custom House** | 160 | 50 | 0 | Auto-sell (**Peter Stuyvesant**) — `europe_custom_house_autosell` |

† Starter “free” buildings: **0** hammer cost to appear at founding; Carpenter’s Shop is **39** hammers in `NAMES.TXT` if built later.

Bold entries on the manual chart = free at colony founding. **`**`** = requires **Adam Smith** in Congress.

---

## Production modifiers (sentiment & difficulty)

Documented in manual ch. independence / colony people view; applied in EOT production (field/craft/hammers/bells/crosses). SoL % via `colony_prod_sol_percent` (Col1 rebel fields, else nation liberty_bells/4). Full SoL catalog: [sons_of_liberty.md](sons_of_liberty.md). Difficulty Tory thresh: [difficulty.md](difficulty.md).

| Condition | Effect on **all** colony production |
|-----------|--------------------------------------|
| Sons of Liberty ≥ **50%** | **+1** to each production unit (field + manufacturing + bells/crosses) | Field, hammers, craft, bells/crosses wired (`colony_prod_sol_bonus`); Tory floor PARK |
| Sons of Liberty = **100%** | **+1** again (total **+2** at full SoL) | Same |
| Tory count vs threshold | DOS: `−⌊tories / (10−difficulty)⌋` then +sol latches (not a flat −1); thresh **10** Discoverer … **6** Viceroy | PARK — see [sons_of_liberty.md](sons_of_liberty.md) |

River / road / plow on **field** tiles add up to **+4** food/resources (manual Terrain Chart footnote) — see [terrain_yields.md](terrain_yields.md).

---

## UI: settlement badges vs Production tab

| UI element | Should show | Linux port today |
|------------|-------------|------------------|
| **Production tab** (multipurpose pane) | Every cargo good / shortfall this turn, plus hammers; slot grid fills the pane as type count changes (no crosses or bells) | Preview from [`colony_preview.c`](../src/core/colony_preview.c) via [`colony_production.c`](../src/core/colony_production.c); crosses/bells → people meters |
| **Production strip** above building colonists | Icon strip spanning the building = **sum** of assigned workers' output | Full building width; amount via `colony_prod_worker_building_output()` summed per workplace |
| **Construction Change list** | Buildable projects with upgrade chains, min population (`NAMES.TXT` min_colony), coastal docks, **Adam Smith** factories, **Peter Stuyvesant** Custom House | [`colonies_list_buildable()`](../src/core/colony.c) |

To fix badge/preview mismatches, both paths should share one function — implemented as **`colony_production.c`**:

```
effective_class = (profession matches recipe) ? skilled : free_colonist
output(worker, building) = tier_rate(building) × class_factor(effective_class) × sentiment_bonus(colony)
input(worker, building)  = output(...) × (factory ? 6/9 : 1)   // 1:1 except factory 6→9
```

`class_factor`: criminal/convert → manufacturing floor **1**; indentured → **2/3** of tier rate; free (and unmatched experts) → tier baseline; matched Master/Expert → ×2 on class-scaled baseline. **SoL sentiment** (+1/+2 per worker) applied in EOT craft/hammers/fields; preview craft may omit until Col1 bridged. Shortfalls appear when Σ input demand > warehouse stock + same-turn field intake for that cargo.

---

## Linux implementation map

| Concern | Module |
|---------|--------|
| Shared production rules | [`src/core/colony_production.c`](../src/core/colony_production.c) |
| Manufacturing recipes | [`src/core/colony_craft.c`](../src/core/colony_craft.c) |
| Turn production + hammers | [`src/core/turn.c`](../src/core/turn.c) |
| Production tab preview | [`src/core/colony_preview.c`](../src/core/colony_preview.c) |
| Settlement production strips | [`colony_screen_building_production_badge()`](../src/core/colony_screen.c) (one strip per building, full width) |
| Profession / skill storage | [`ColonizeColonist.profession`](../src/core/colony.h), [`UNITS_JOB_*`](../src/core/units.h) |
| Building definitions | [`colonies_load_buildings()`](../src/core/colony.c) ← `NAMES.TXT` |

---

## See also

- [terrain_yields.md](terrain_yields.md) — area / town-commons field production
- [manual_gap.md](manual_gap.md) — feature checklist vs manual
- [decomp_inventory.md](decomp_inventory.md) — EOT colony production pipeline
- [assets.md](assets.md) — colony screen layout and Production tab keys
