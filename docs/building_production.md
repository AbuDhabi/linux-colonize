# Settlement building production

Reference for what colonists produce **inside** colony buildings (settlement view), which `@JOB` skills apply, and how that relates to the Production tab. Field / area yields: [terrain_yields.md](terrain_yields.md). Sentiment: [sons_of_liberty.md](sons_of_liberty.md).

## Sources

| Source | Role |
|--------|------|
| [`COLONIZE/NAMES.TXT`](../COLONIZE/NAMES.TXT) `@BUILDING`, `@JOB` | **Authoritative** construction hammers / tools×10 / min_colony; profession names and school tier |
| `FUN_15eb_1d4c` (building/manufacturing yield) + `FUN_15eb_15c6` (upgrade depth 0/1/2) | **Authoritative** DOS manufacturing composer — decomp of `1d4c` is messy; tier rates below are **provisional** until a clean peel |
| [`COLONIZE/Colonization.pdf`](../COLONIZE/Colonization.pdf) | Manual ch. 6 workplace rules, Production view, shortfalls; Building Chart **effects** (prefer NAMES for costs) |
| [`COLONIZE/README.TXT`](../COLONIZE/README.TXT) | Colony **Space** = one free production cycle; Cathedral min pop **8** (v3 fix) |
| [`colony_eot_production.md`](../original_sources_annotated/turn/colony_eot_production.md) | EOT phase order (`FUN_364b_0688`) |

**Removed:** earlier claim that `VICEROY.EXE` @ `0x16103` embeds tier bytes `3,6,8` — that offset is **not** a rate table (false cite).

When the manual Building Chart disagrees with `NAMES.TXT` on **construction** costs or min population, prefer **`NAMES.TXT`**.

---

## Scope: settlement vs area

| Location | View | Produces |
|----------|------|----------|
| **Area** (8 surround tiles + town commons) | Area view | Raw commodities — [terrain_yields.md](terrain_yields.md) |
| **Buildings** | Settlement view | Processed goods, hammers, muskets, crosses, liberty bells, teaching |

Up to **three** colonists may work the same building (schools: teachers + students).

The printed manual says converts are “unwilling to work inside manufacturing concerns” (p. 33); that is **misleading**. Converts **can** be assigned to buildings — same manufacturing floor as petty criminals. On **tiles** they can be better than free colonists (job whitelist) — [terrain_yields.md](terrain_yields.md).

---

## Turn order (settlement production)

Aligns with [`colony_eot_production.md`](../original_sources_annotated/turn/colony_eot_production.md) / manual Production view:

1. **Harvest** — town-commons auto-yield + assigned field workers → warehouse.
2. **Food** — each colonist eats **2** food; surplus toward growth (**200** → new free colonist). Starvation latch + loss on later turns.
3. **Manufacturing** — processing buildings convert **input → output** from warehouse (including same-turn harvest). Unmet input → **shortfalls**.
4. **Hammers** — carpenters consume **lumber** toward `building_in_production`.
5. **Crosses / liberty bells** — immigration / independence counters (not warehouse cargo).

Manufacturing before hammers so ore→tools and cotton→cloth see same-turn field intake.

---

## Worker output: colonist class

For **manufacturing** jobs. Free-colonist house baseline is **3** (provisional / port).

| Colonist type | `@JOB` index | House-tier units / worker (port) |
|---------------|-------------:|---------------------------------:|
| Petty criminal | 26 | **1** (`tier/3`) |
| Indian convert | 27 | **1** (`tier/3`; **can** work buildings) |
| Indentured servant | 25 | **2** (`tier*2/3`) |
| Free colonist (unskilled) | 19 | **3** |

At shop/factory tiers the port scales the same way: criminal/convert → **2 / 3**; indentured → **4 / 6**; free → **6 / 9**. **DOS class scaling inside `FUN_15eb_1d4c` not fully re-peeled** — treat shop/factory criminal floors as port behavior until confirmed.

### Matching skill vs wrong job

| Situation | Production treated as |
|-----------|------------------------|
| Skill matches workplace (e.g. Master Blacksmith in a smithy) | Expert / Master — **×2** manufacturing output and input |
| Skill does **not** match | **Free colonist** rates for that assignment |

Field experts: food/fish **+2**, other jobs **×2** — [terrain_yields.md](terrain_yields.md) (not blanket ×2).

---

## Building tier throughput (free colonist, per worker)

**Provisional** rates used by the port and consistent with Building Chart “doubles” / factory **1.5×** efficiency. DOS upgrade depth is `FUN_15eb_15c6` → **0 / 1 / 2** (house / shop / factory). Exact byte table in the EXE is **not** at `0x16103`.

| Tier | Depth | Building level (examples) | Free **output** / worker | Typical **input** / worker |
|------|------:|---------------------------|-------------------------:|---------------------------:|
| **1 — House / base** | 0 | `*'s House`, Carpenter’s Shop, Armory, Town Hall (bells), Church | **3** | **3** (1:1) |
| **2 — Shop / mill** | 1 | `*'s Shop`, Lumber Mill, Rum Distillery, Fur Trading Post, Magazine | **6** | **6** (1:1) |
| **3 — Factory** | 2 | `* Factory`, Textile Mill, Iron Works, Arsenal (**Adam Smith**) | **9** | **6** (1.5× — `input = (out*6+8)/9` in port) |

**Exceptions / special cases**

| Building | Notes |
|----------|--------|
| **Lumber Mill** | Manual: **doubles** hammers vs Carpenter’s Shop (3→**6**); lumber 1:1. |
| **Magazine** | Manual: **doubles** muskets vs Armory (3→**6**). |
| **Arsenal** | Factory-tier muskets; port uses same **9 out / 6 in** factory rule as other factories (not 9←9). Manual “half the tools” prose aligns with **6** tools → **9** muskets vs Magazine’s 6←6. |
| **Iron Works / luxury factories** | **6** raw → **9** finished. |
| **Printing Press** | **+50%** liberty bells colony-wide (manual). |
| **Newspaper** | **+100%** liberty bells colony-wide. |
| **Church / Cathedral** | Passive + worker crosses — port: Church passive **+2** + **3**/worker; Cathedral **+3** + **6**/worker; colony base **+1** cross/turn. Marked **port / manual-aligned; deep peel pending**. |
| **Town Hall** | Port: passive **+1** bell + **3**/worker; Elder Statesman ×2. |
| **Docks** | Enables fishermen on ocean/lake surrounds (no processing). |
| **Schoolhouse / College / University** | Teach (faculty 1 / 2 / 3). |

---

## Processing chains (input → output)

| Output | Input | Tier-1 | Tier-2 | Tier-3 (**Adam Smith**) |
|--------|-------|--------|--------|-------------------------|
| **Hammers** | Lumber | Carpenter’s Shop | Lumber Mill | — |
| **Tools** | Ore | Blacksmith’s House | Blacksmith’s Shop | Iron Works |
| **Muskets** | Tools | Armory | Magazine | Arsenal |
| **Cloth** | Cotton | Weaver’s House | Weaver’s Shop | Textile Mill |
| **Cigars** | Tobacco | Tobacconist’s House | Tobacconist’s Shop | Cigar Factory |
| **Rum** | Sugar | Rum Distiller’s House | Rum Distillery | Rum Factory |
| **Coats** | Furs | Fur Trader’s House | Fur Trading Post | Fur Factory |
| **Crosses** | — | Church | — | Cathedral |
| **Liberty bells** | — | Town Hall (+ Press / Newspaper) | — | — |

**Starter colonies** receive tier-1 free manufacturing: Town Hall, Carpenter’s Shop, Blacksmith’s House, Weaver’s / Tobacconist’s / Distiller’s / Fur Trader’s **House**.

---

## Skills chart (`@JOB` → workplace)

Indices match `NAMES.TXT` `@JOB` and Col1 `profession` bytes.

### Outdoorsmen (area view)

| @JOB | Index | Expert name | Teach |
|------|------:|-------------|-------|
| Farmer | 0 | Expert Farmers | S* |
| Sugar Planter | 1 | Master Sugar Planters | C* |
| Tobacco Planter | 2 | Master Tobacco Planters | C* |
| Cotton Planter | 3 | Master Cotton Planters | C* |
| Fur Trapper | 4 | Expert Fur Trappers | S* |
| Lumberjack | 5 | Expert Lumberjacks | S |
| Ore Miner | 6 | Expert Ore Miners | S |
| Silver Miner | 7 | Expert Silver Miners | S |
| Fisherman | 8 | Expert Fishermen | S* |

\* may also be learned from natives (manual). School: **S** Schoolhouse, **C** College, **U** University. NAMES school field **1–4** (4 = unlearnable).

### Craftsmen (settlement)

| @JOB | Index | Title | Buildings | Converts |
|------|------:|-------|-----------|----------|
| Distiller | 9 | Master Distiller | Rum Distiller’s House / Distillery / Factory | Sugar → Rum |
| Tobacconist | 10 | Master Tobacconist | Tobacconist’s House / Shop / Cigar Factory | Tobacco → Cigars |
| Weaver | 11 | Master Weaver | Weaver’s House / Shop / Textile Mill | Cotton → Cloth |
| Fur Trader | 12 | Master Fur Trader | Fur Trader’s House / Trading Post / Factory | Furs → Coats |
| Carpenter | 13 | Master Carpenter | Carpenter’s Shop, Lumber Mill | Lumber → Hammers |
| Blacksmith | 14 | Master Blacksmith | Blacksmith’s House / Shop / Iron Works | Ore → Tools |
| Gunsmith | 15 | Master Gunsmith | Armory, Magazine, Arsenal | Tools → Muskets |

### Political / religious

| @JOB | Index | Title | Building | Produces |
|------|------:|-------|----------|----------|
| Preacher | 16 | Firebrand Preacher | Church, Cathedral | Crosses (×2 skilled) |
| Statesman | 17 | Elder Statesman | Town Hall | Liberty bells (×2 skilled) |
| Teacher | 18 | Expert Teacher | Schoolhouse / College / University | Trains colonists |

### Not manufacturing

| @JOB | Index | Role |
|------|------:|------|
| Colonist | 19 | Free colonist |
| Pioneer … Dragoon | 20–23 | Map units |
| Missionary | 24 | Missions |
| Ind. Servant / Criminal / Convert | 25–27 | Class rates above; convert field rules in [terrain_yields.md](terrain_yields.md) |

---

## Building chart (construction) — from `@BUILDING`

**Ham** = hammers; **Tools** = `tools(*10)` from NAMES ×10 on load (e.g. `2` → **20**). **Min pop** = `min_colony`. Synced from [`NAMES.TXT`](../COLONIZE/NAMES.TXT) `@BUILDING`.

| Building | Ham | Tools | Min pop | Effect (summary) |
|----------|----:|------:|--------:|------------------|
| Stockade | 64 | 0 | 3 | Defense +100% |
| Fort | 120 | 100 | 3 | Defense +150% |
| Fortress | 320 | 200 | 8 | Defense +200% |
| Armory | 52 | 0 | 1 | Tools → muskets; artillery |
| Magazine | 120 | 50 | 8 | Doubles musket production |
| Arsenal | 240 | 100 | 8 | Factory muskets (**Adam Smith**) |
| Docks | 52 | 0 | 1 | Fishing on sea/lake tiles |
| Drydock | 80 | 50 | 4 | Ship repair |
| Shipyard | 240 | 100 | 8 | Ship construction |
| Town Hall | 64 | 0 | 1 | Liberty bells |
| Town Hall (row 2) | 64 | 50 | 4 | Upgrade chain (NAMES) |
| Town Hall (row 3) | 120 | 100 | 8 | Upgrade chain (NAMES) |
| Schoolhouse | 64 | 0 | 4 | Teach (faculty 1) |
| College | 160 | 50 | 8 | Teach (faculty 2) |
| University | 200 | 100 | 10 | Teach (faculty 3) |
| Warehouse | 80 | 0 | 1 | +100 storage |
| Warehouse Expansion | 80 | 20 | 1 | +100 storage |
| Stable | 64 | 0 | 1 | Horse breeding |
| Custom House | 160 | 50 | 1 | Auto-sell (**Peter Stuyvesant**) |
| Printing Press | 52 | 20 | 1 | +50% liberty bells |
| Newspaper | 120 | 50 | 4 | +100% liberty bells |
| Weaver’s House | 64 | 0 | 1 | Cotton → cloth |
| Weaver’s Shop | 64 | 20 | 1 | Increases cloth |
| Textile Mill | 160 | 100 | 8 | Factory cloth (**Adam Smith**) |
| Tobacconist’s House | 64 | 0 | 1 | Tobacco → cigars |
| Tobacconist’s Shop | 64 | 20 | 1 | Increases cigars |
| Cigar Factory | 160 | 100 | 8 | Factory cigars (**Adam Smith**) |
| Rum Distiller’s House | 64 | 0 | 1 | Sugar → rum |
| Rum Distillery | 64 | 20 | 1 | Increases rum |
| Rum Factory | 160 | 100 | 8 | Factory rum (**Adam Smith**) |
| Capitol | 400 | 100 | 16 | Late-game capitol |
| Capitol Expansion | 400 | 100 | 16 | Capitol upgrade |
| Fur Trader’s House | 56 | 0 | 1 | Furs → coats |
| Fur Trading Post | 56 | 20 | 1 | Increases coats |
| Fur Factory | 160 | 100 | 6 | Factory coats (**Adam Smith**) |
| Carpenter’s Shop | 39 | 0 | 1 | Lumber → hammers |
| Lumber Mill | 52 | 0 | 3 | Doubles hammers |
| Church | 64 | 0 | 3 | Crosses; missionaries |
| Cathedral | 176 | 100 | 8 | More crosses |
| Blacksmith’s House | 64 | 0 | 1 | Ore → tools |
| Blacksmith’s Shop | 64 | 20 | 1 | Increases tools |
| Iron Works | 240 | 100 | 8 | Factory tools (**Adam Smith**) |

Starter colonies grant several houses / Carpenter’s Shop / Town Hall without spending the chart cost at founding; `NAMES` costs apply if rebuilt later.

Manual chart often listed shop min-pop **4** and Church hammers **52** — **wrong vs NAMES** (shops min-pop **1**; Church **64**).

---

## Production modifiers (sentiment)

Full catalog: [sons_of_liberty.md](sons_of_liberty.md). Tory thresh by difficulty: [difficulty.md](difficulty.md).

| Condition | Effect | Port |
|-----------|--------|------|
| SoL ≥ **50%** / **100%** | +1 / +2 per production unit | Wired (`colony_prod_sol_bonus`); EOT + preview |
| Tory floor | `−⌊tories/(10−diff)⌋` + sol latches | **PARK** |

---

## UI: settlement badges vs Production tab

| UI element | Should show | Linux port today |
|------------|-------------|------------------|
| **Production tab** | Every cargo good / shortfall + hammers | [`colony_preview.c`](../src/core/colony_preview.c) via [`colony_production.c`](../src/core/colony_production.c); crosses/bells → people meters |
| **Production strip** | Sum of assigned workers’ output | `colony_prod_worker_building_output()` |
| **Construction Change list** | Buildable projects, min-pop, coastal docks, Adam Smith, Stuyvesant | [`colonies_list_buildable()`](../src/core/colony.c) |

Shared formula (port):

```
effective_class = (profession matches recipe) ? skilled : free_colonist
output(worker, building) = tier_rate(building) × class_factor(effective_class) + sentiment
input(worker, building)  = output × (factory ? 6/9 : 1)
```

`class_factor`: criminal/convert → `tier/3`; indentured → `tier*2/3`; free → tier; matched Master → ×2 after class scale. Sentiment applied in EOT and preview.

---

## Port status (manufacturing)

| Rule | Status |
|------|--------|
| Tier rates 3 / 6 / 9 | Port wired; DOS cite = `15eb_15c6` depth + `15eb_1d4c` (peel incomplete) |
| Factory input 6→9 | Port wired; provisional vs DOS |
| Class /3 and *2/3 | Port wired; DOS unconfirmed |
| Construction costs / min_pop | Loaded from NAMES — matches table above |
| Church / TH passives | Port wired; deep peel pending |
| False `0x16103` EXE table | **Retracted** |

---

## Linux implementation map

| Concern | Module |
|---------|--------|
| Shared production rules | [`colony_production.c`](../src/core/colony_production.c) |
| Manufacturing recipes | [`colony_craft.c`](../src/core/colony_craft.c) |
| Turn production + hammers | [`turn.c`](../src/core/turn.c) |
| Production tab preview | [`colony_preview.c`](../src/core/colony_preview.c) |
| Settlement strips | [`colony_screen.c`](../src/core/colony_screen.c) |
| Building definitions | [`colonies_load_buildings()`](../src/core/colony.c) ← `NAMES.TXT` |

---

## See also

- [terrain_yields.md](terrain_yields.md) — area / town-commons field production
- [sons_of_liberty.md](sons_of_liberty.md) — SoL / Tory production mod
- [manual_gap.md](manual_gap.md) — feature checklist
- [colony_eot_production.md](../original_sources_annotated/turn/colony_eot_production.md) — EOT phases
