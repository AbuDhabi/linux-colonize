# Settlement building production

Reference for what colonists produce **inside** colony buildings (settlement view), which `@JOB` skills apply, and how that relates to the Production tab. Field / area yields: [terrain_yields.md](terrain_yields.md). Sentiment: [sons_of_liberty.md](sons_of_liberty.md).

## Sources

| Source | Role |
|--------|------|
| [`COLONIZE/NAMES.TXT`](../COLONIZE/NAMES.TXT) `@BUILDING`, `@JOB` | **Authoritative** construction hammers / tools×10 / min_colony; profession names and school tier |
| `FUN_15eb_1d4c` (per-worker manufacturing/production value) + `FUN_15eb_15c6` (upgrade depth 0/1/2) | **Authoritative** DOS manufacturing composer — deep peel: [`manufacturing_worker_calc_1d4c.md`](../original_sources_annotated/turn/manufacturing_worker_calc_1d4c.md). Tier rates 3/6/9, class-scale, and the SoL/Tory term's tier-interaction are now **DOS-confirmed and wired** (2026-08-15); the factory-tier 6→9 *input* ratio remains open (deep peel Open questions) |
| — | **2026-08-15: corruption resolved, not just re-described.** There was never real file corruption — the `.asm` text export just never followed `1d4c`'s own indirect jump table (data sitting inline after the `JMP`, a routine case for a disassembler to miss, not damage). Manually extracted the raw bytes and re-disassembled with `ndisasm`; every resolved address lands exactly on the tool's own labels (byte-accurate). Full function now readable end to end: `15eb:1d4c`-`15eb:1f71` (549 bytes — the earlier "497" was a slip, not the `0x7bfc`-`0x7e21` address itself, which checks out exactly as this function's `FUN_0000_7bfc` alias in the sibling `viceroy_overlays.*` export, linear-addressed), a 9-way switch on the worker's `@JOB` profession (9-17) that matches the port's own Craftsmen/Carpenter/Preacher/Statesman split profession-for-profession. The percentage-scaling formula (`CX = 100 − SoL%`; `AX = (byte[bx+0x1f] * CX + 50) / 100`) and the class-gate (`byte[bx+0x1a] < 4`, `0x34`-stride table at `DS:0x543f`) are confirmed byte-exact, and the three prior sub-calls are now identified (`FUN_15eb_0e18`=profession, `FUN_15eb_0e52`=workplace, `FUN_15eb_0274`=colony SoL%) — not "6cc8/6d02/6124" as an earlier pass cited (that citation didn't match either export's naming and wasn't chased further this pass, but is no longer necessary — see below). **Also found:** the `.c` pseudocode export for this symbol is unreliable **in both** `viceroy_unpacked_2.c` and `viceroy_overlays.c` (both independently merge in unrelated sound/timer-driver code for cases it doesn't actually have — a shared bug in whatever switch-recovery step both exports went through, not random noise) — work from the `.asm`, not the `.c`, for this symbol. Remaining unknowns (what `byte[bx+0x1f]`/`0x1a`/`0x53a6`/`0x543f` and `local_12`/`local_e`'s origin mean, and whether the shared craft body's tier bonus is really 3/6/9) are enumerated as next steps in the deep-peel doc — normal RE backlog now, not a blocker |
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

For **manufacturing** jobs. Free-colonist house baseline is **3** — **DOS-confirmed** 2026-08-15, see [`manufacturing_worker_calc_1d4c.md`](../original_sources_annotated/turn/manufacturing_worker_calc_1d4c.md).

| Colonist type | `@JOB` index | House-tier units / worker (port) |
|---------------|-------------:|---------------------------------:|
| Petty criminal | 26 | **1** (`tier/3`) |
| Indian convert | 27 | **1** (`tier/3`; **can** work buildings) |
| Indentured servant | 25 | **2** (`tier*2/3`) |
| Free colonist (unskilled) | 19 | **3** |

At shop/factory tiers the port scales the same way: criminal/convert → **2 / 3**; indentured → **4 / 6**; free → **6 / 9**. **DOS-confirmed** 2026-08-15 — traced `FUN_15eb_1d4c`'s class tag (indentured=2, criminal/convert=1, free=3, i.e. exactly numerator/3) through the tier arithmetic for all three tiers; matches this table exactly.

### Matching skill vs wrong job

| Situation | Production treated as |
|-----------|------------------------|
| Skill matches workplace (e.g. Master Blacksmith in a smithy) | Expert / Master — **×2** manufacturing output and input |
| Skill does **not** match | **Free colonist** rates for that assignment |

**DOS-confirmed** 2026-08-15 for the six shared craft professions (skill-match flag doubles post-tier, via a compiler tail-merge into the Carpenter case body — see deep peel). Carpenter/Preacher's doubling in DOS is actually gated by a **colony-wide "owns the upgraded building" check**, not a per-worker skill-match flag — converges to the same numbers as the port's model in every buildable colony shape (upgrades replace, never stack), so left as-is; Statesman's doubling *is* a direct skill-match flag, no divergence.

Field experts: food/fish **+2**, other jobs **×2** — [terrain_yields.md](terrain_yields.md) (not blanket ×2).

---

## Building tier throughput (free colonist, per worker)

**DOS-confirmed** 2026-08-15 (see deep peel) — matches Building Chart “doubles” / factory **1.5×** efficiency exactly. DOS upgrade depth is `FUN_15eb_15c6` → **0 / 1 / 2** (house / shop / factory) / `FUN_15eb_039e` (owned-buildings-along-chain count, 1/2/3). Exact byte table in the EXE is **not** at `0x16103`.

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
| Teacher | 18 | Expert Teacher | Schoolhouse / College / University | Trains colonists; unskilled → `@NOTEACHER`; job tier vs school → `@NEEDCOLLEGE`/`@NEEDUNIVERSITY` |

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
| Stockade | 64 | 0 | 3 | Defense ×2 (`157e` local_1a=4); +fortify → ×2.5 — [combat.md](combat.md) |
| Fort | 120 | 100 | 3 | Same Stockade ladder in `015e` (wiki +150% ≈ fortified) — [combat.md](combat.md) |
| Fortress | 320 | 200 | 8 | Defense ×3 (`local_1a=8`); fortify does not stack — [combat.md](combat.md) |
| Armory | 52 | 0 | 1 | Tools → muskets; artillery |
| Magazine | 120 | 50 | 8 | Doubles musket production |
| Arsenal | 240 | 100 | 8 | Factory muskets (**Adam Smith**) |
| Docks | 52 | 0 | 1 | Fishing on sea/lake tiles |
| Drydock | 80 | 50 | 4 | Ship repair (`@REFIT` on combat bit7 clear) |
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
| Tory floor | `−⌊tories/(10−diff)⌋` + sol latches | **Wired** (same helper; AI thresh 10) |

---

## UI: settlement badges vs Production tab

| UI element | Should show | Linux port today |
|------------|-------------|------------------|
| **Settlement badges** | Building slots | Workplace output-type + amount (Town Hall / Church include free passives even when empty) |
| **Production tab** | Every cargo good / shortfall + hammers | [`colony_preview.c`](../src/core/colony_preview.c) via [`colony_production.c`](../src/core/colony_production.c); crosses/bells → people meters |
| **Production strip** | Sum of assigned workers’ output | `colony_prod_worker_building_output()` |
| **Construction Change list** | Buildable projects, min-pop, coastal docks, Adam Smith, Stuyvesant; owned refuse `@ALREADYHAVE` / `@NOMOREWAREHOUSE` | [`colonies_list_buildable()`](../src/core/colony.c) + `colonies_emit_already_have_chrome` |

Shared formula (port, `colony_prod_manufacturing_output` — DOS-confirmed 2026-08-15):

```
tag = class_tag(profession)              ; 3 free / 2 indentured / 1 criminal-convert
v = tag + sol_bonus                       ; sol_bonus signed — Tory penalty reduces output
if tier >= shop:    v += tag
if tier == factory: v += v >> 1           ; ×1.5, floor
if skill matches:   v *= 2                ; whole running total, sol_bonus included
output = max(v, 0)
input(worker, building) = base_output(no sol_bonus) × (factory ? 6/9 : 1)
```

Note `sol_bonus` folds in *before* tier/skill scaling — it is not a flat post-hoc add, and callers that want the un-modified base rate (settlement badges, input-side consumption) pass `sol_bonus=0`.

---

## Port status (manufacturing)

| Rule | Status |
|------|--------|
| Tier rates 3 / 6 / 9 | **DOS-confirmed** (2026-08-15) — `15eb_1d4c`'s shared craft body reproduces 3/6/9 exactly from `class_tag=3` (free colonist) through house→shop→factory tier math; see deep peel |
| Factory input 6→9 | Still provisional — the *output* ladder (3/6/9) is confirmed; the input-side 6/9 ratio (`colony_prod_tier_input_for_output`) wasn't traced this pass (that's warehouse-consumption bookkeeping, a separate code path from `1d4c`'s output calc) |
| Class /3 and *2/3 | **DOS-confirmed** (2026-08-15) — `1d4c`'s `local_12` tag (2=indentured, 1=criminal/convert, 3=free) is the exact numerator/3 of the port's `class_factor`; see deep peel |
| Construction costs / min_pop | Loaded from NAMES — matches table above |
| Church / TH passives | Settlement badge + totals Done; deep peel pending |
| False `0x16103` EXE table | **Retracted** |
| **2026-08-15 fix:** SoL/Tory term flat-add vs. DOS tier-fold | `colony_prod_manufacturing_output` used to add `sol_bonus` as a flat, **positive-only** term after tier+skill math — house/shop tier were numerically identical to DOS by coincidence, but factory tier missed the ×1.5 rounding, skilled workers missed the ×2, and every Tory *penalty* (negative `sol_bonus`) was silently dropped instead of reducing output. Fixed: `sol_bonus` is now a signed parameter folded in exactly where `FUN_15eb_1d4c` does (before tier/skill math). Callers that intentionally show the un-modified base rate (settlement badges, input-side consumption) pass `0`. Regression: `test_turn.c` factory-tier sol-fold + Tory-penalty-clamp block |
| **2026-08-15 fix:** Production tab bells/crosses vs EOT | `colony_preview_compute` used the plain `colony_prod_colony_bells`/`_crosses` (no FF bonus) and a flat one-shot SoL add, while the real EOT nation tick (`turn_count_bells_and_crosses_for_nation`) applies Jefferson/Paine/Penn and adds SoL **per bell/cross worker**. Preview under-counted for colonies with those FFs or >1 worker. Fixed: preview now calls the `_ff` variants with the same nation FF lookups and the same per-worker SoL loop as `turn.c`. Regression: Phase C block in `test_turn.c` (`colony_preview_compute` w/ Jefferson granted). |
| **2026-08-15 fix:** Production tab fur trapper vs Henry Hudson | `colony_preview_compute`'s field-worker loop never checked `FF_HENRY_HUDSON`, so the Production tab showed half the real fur yield for a colony that owns Hudson (`turn_produce_one_colony` in `turn.c` doubles fur trapper output). Fixed: preview now applies the same doubling before the SoL add. Regression: new `test_turn.c` block (`AMER2.MP` site with fur yield, doubles stock exactly, preview matches). |
| **2026-08-15 fix:** Production tab hammers hidden without a queued project | `colony_preview_compute` only computed the Hammers row when `building_in_production >= 0`, but `turn_produce_one_colony`'s hammers block (turn.c "TURN5→6" comment) banks hammers and consumes lumber every turn a Carpenter is staffed, project or not — the preview silently under-informed the player (row just missing) whenever no Construction item was selected. Fixed: preview now mirrors the same three-way branch (queued project / no project but lumber available / no lumber, bank at raw output) as turn.c. Regression: new `test_turn.c` block (Carpenter's Shop, no project, checks both preview and the real EOT tick agree: hammers=3, lumber 10→7). |
| **2026-08-15 fix:** Production tab missing horse breeding | `colony_preview_compute` never simulated the ≥2-horses + food-surplus breeding turn.c does every EOT tick (Stable cap 4 vs 2), so the Production tab showed neither the Horses row nor the extra Food consumption from breeding. Fixed: preview now runs the same breed calc (incl. Stable check) and folds it into `goods[HORSES]`/`goods[FOOD]`/`food_net`. Regression: existing `test_turn.c` "colony horse breed" block extended to assert `colony_preview_compute` predicts the exact bred amount before the real tick runs. |
| **Known, accepted gap (not fixed):** settlement-view per-tile/per-job yield badges (`colony_screen_draw_area_overlays`, `colony_screen_draw_jobs_popup` in `colony_screen.c`) call `colony_yield_for_worker`/`colony_yield_for_tile` directly and don't have `col1`/FF context plumbed through the render call chain — so unlike the Production tab, these numbers don't include Henry Hudson's fur ×2 (or SoL, by existing design — see UI table above). Plumbing `col1` into `colony_screen_render`'s signature to fix this is a larger, separate refactor; not attempted this pass. |

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
