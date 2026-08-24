# Settlement building production

Reference for what colonists produce **inside** colony buildings (settlement view), which `@JOB` skills apply, and how that relates to the Production tab. Field / area yields: [terrain_yields.md](terrain_yields.md). Sentiment: [sons_of_liberty.md](sons_of_liberty.md).

## Sources

| Source | Role |
|--------|------|
| [`COLONIZE/NAMES.TXT`](../COLONIZE/NAMES.TXT) `@BUILDING`, `@JOB` | **Authoritative** construction hammers / tools×10 / min_colony; profession names and school tier |
| `FUN_15eb_1d4c` (per-worker manufacturing/production value) + `FUN_15eb_15c6` (upgrade depth 0/1/2) | **Authoritative** DOS manufacturing composer — deep peel: [`manufacturing_worker_calc_1d4c.md`](../original_sources_annotated/turn/manufacturing_worker_calc_1d4c.md). Tier rates 3/6/9, class-scale, the SoL/Tory term's tier-interaction, and the factory-tier 6→9 *input* ratio (including its own SoL-fold, resolved by play — see fix row below) are all now **DOS-confirmed and wired** (2026-08-15) |
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
| **Church / Cathedral** | Passive + worker crosses — **DOS-confirmed** 2026-08-15 (`FUN_15eb_1f72`, see [`manufacturing_worker_calc_1d4c.md`](../original_sources_annotated/turn/manufacturing_worker_calc_1d4c.md)): colony base **+1** cross/turn, **+1** independently if Church built, **+1** independently if Cathedral built — Church and Cathedral give the *same* +1 passive, not a scaled +2/+3 (that was manual/wiki-sourced and wrong). Worker rate: **3**/worker unskilled, **6** Firebrand Preacher, **doubled** (6/12) if the colony owns Cathedral specifically (not a flat Church-3/Cathedral-6 split — the doubling is colony-wide and stacks with skill). |
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
input(worker, building) = output × (factory ? 6/9 : 1)   ; input folds sol_bonus the
                                                          ; same way output does — see
                                                          ; the "Factory input 6→9" fix
                                                          ; row below (2026-08-15,
                                                          ; player-confirmed)
```

Note `sol_bonus` folds in *before* tier/skill scaling — it is not a flat post-hoc add on either output or input, and callers that want the un-modified base rate (settlement badges) pass `sol_bonus=0` for both.

---

## Port status (manufacturing)

| Rule | Status |
|------|--------|
| Tier rates 3 / 6 / 9 | **DOS-confirmed** (2026-08-15) — `15eb_1d4c`'s shared craft body reproduces 3/6/9 exactly from `class_tag=3` (free colonist) through house→shop→factory tier math; see deep peel |
| Factory input 6→9 | **Resolved 2026-08-15, player-confirmed (Viceroy) — the discount is real, and it tracks the actual SoL-adjusted output, not the flat base rate.** Earlier RE hit a dead end (`FUN_15eb_1d4c` only computes *output*; a "paired good" table looked promising but turned out to be the K-advisory warning-message system, a red herring; the real stock-consumption code was never found in `FUN_364b_0688`). Settled by play instead: Textile Mill, free colonist, +2 sentiment — 12 cloth/turn output, 8 cotton/turn consumed (23 produced, 15 surplus colony-wide). `colony_prod_tier_input_for_output(FACTORY, 12) = (12*6+8)/9 = 8` — exact match; the un-modified-base-output reading (`(9*6+8)/9=6`) the port used to force would've been wrong. Fixed: `colony_prod_manufacturing_input` takes `sol_bonus` and threads it into the output calc it derives input from, same as the output side already did. Regression: `test_turn.c` (Iron Works, free colonist, sol_bonus=2 → output 12/input 8; sol_bonus=0 → input still 6, confirming the base-rate half wasn't broken). |
| Class /3 and *2/3 | **DOS-confirmed** (2026-08-15) — `1d4c`'s `local_12` tag (2=indentured, 1=criminal/convert, 3=free) is the exact numerator/3 of the port's `class_factor`; see deep peel |
| Construction costs / min_pop | Loaded from NAMES — matches table above |
| Church / TH passives | Settlement badge + totals Done; Church/Cathedral passive **DOS-confirmed** 2026-08-15 — see fix row below; Town Hall passive deep peel still pending |
| **2026-08-15 fix:** Church/Cathedral passive crosses were wiki-sourced, not DOS-confirmed, and wrong | `colony_prod_church_passive_crosses` returned +2 (Church) / +3 (Cathedral) from the manual/wiki. Found `FUN_15eb_1f72` (the nation-level bells/crosses composer, right after `FUN_15eb_1d4c` — same function whose Printing Press ×1.5 / Newspaper ×2 bell multipliers and Jefferson(15)/Paine(17) FF-index checks matched the port exactly, lending confidence to this read): colony crosses = 1 (unconditional) + 1 *independently* if Church built + 1 *independently* if Cathedral built. Church and Cathedral are worth the **same** +1 passive, not a scaled +2/+3. Fixed; both now return 1. Full peel (including the still-unresolved bells side): [`nation_crosses_bells_1f72.md`](../original_sources_annotated/turn/nation_crosses_bells_1f72.md). Regression: new `test_turn.c` Church-vs-Cathedral parity check (no prior test exercised Cathedral specifically). |
| False `0x16103` EXE table | **Retracted** |
| **2026-08-15 fix:** SoL/Tory term flat-add vs. DOS tier-fold | `colony_prod_manufacturing_output` used to add `sol_bonus` as a flat, **positive-only** term after tier+skill math — house/shop tier were numerically identical to DOS by coincidence, but factory tier missed the ×1.5 rounding, skilled workers missed the ×2, and every Tory *penalty* (negative `sol_bonus`) was silently dropped instead of reducing output. Fixed: `sol_bonus` is now a signed parameter folded in exactly where `FUN_15eb_1d4c` does (before tier/skill math). Callers that intentionally show the un-modified base rate (settlement badges, input-side consumption) pass `0`. Regression: `test_turn.c` factory-tier sol-fold + Tory-penalty-clamp block |
| **2026-08-15 fix (same bug, three more spots):** Tory penalty dropped for hammers/bells/crosses/field-yield preview | The same `sol_b > 0` guard (drop every Tory penalty, only apply SoL bonuses) was also in `turn.c`'s hammers block and `turn_count_bells_and_crosses_for_nation` (bells/crosses), and in `colony_preview.c`'s mirrors of all three plus its field-yield loop (that one wasn't even guarded the same way as `turn.c`'s already-correct unconditional field-yield add — a preview-only regression from copying the pattern). All five fixed: `sol_b != 0` guard, signed multiply/add, then clamp to ≥0. |
| **2026-08-15 fix, bells specifically:** SoL not folded before skill doubling | Bells had the same "add after skill ×2" issue manufacturing had. Fixed properly this time: `colony_prod_bells_worker` takes `sol_bonus` and folds it in *before* the Statesman ×2, matching `FUN_15eb_1d4c`'s Statesman body exactly (no building-flag complexity to work around — Statesman's DOS body is `tag+local_e` then double-on-skill, nothing else). `colony_prod_colony_bells_ff` now takes `sol_bonus` and threads it per-worker instead of the old external "count workers, multiply, add after" — the external mechanism is gone entirely for bells. Regression: `test_turn.c` unit-level `colony_prod_bells_worker` check + updated nation-tick Tory-penalty test (7→5 under a −1 penalty, not 6 — the 6 was this same fix's own first, incomplete pass two rounds ago) |
| **2026-08-15 fix: hammers/crosses, done properly** | Implemented the restructure flagged above. `colony_prod_crosses_worker`/`colony_prod_hammers_worker` now take `(sol_bonus, colony_has_upgrade)` and compute the real DOS shape: `base = skilled ? 6 : class_tag`, `+ sol_bonus`, **then** doubled only if the caller says the colony owns the upgraded building (Cathedral / Lumber Mill — checked once per colony via `colony_prod_building_built`, not derived from which building the worker happens to occupy). `colony_prod_colony_crosses_ff` gained a `sol_bonus` parameter and threads it + the Cathedral-owned flag per worker; `colony_prod_colony_hammers` gained the same, and separately tracks a sol-free `lumber_use` (lumber consumption still shouldn't scale with SoL) alongside the sol-adjusted hammer count. The old external "count matching workers, multiply, add after" mechanism in `turn.c`/`colony_preview.c` is gone for both, same as bells. Badges pass `sol_bonus=0, colony_has_upgrade=false` (unchanged simplified behavior). Regression: 4 direct unit checks (`test_turn.c`) covering exactly the cases that differ from the pre-fix numbers — unskilled+upgrade and skilled+upgrade, both crosses and hammers — all matched the hand-derived DOS values on first run. |
| **2026-08-15 fix:** Production tab bells/crosses vs EOT | `colony_preview_compute` used the plain `colony_prod_colony_bells`/`_crosses` (no FF bonus) and a flat one-shot SoL add, while the real EOT nation tick (`turn_count_bells_and_crosses_for_nation`) applies Jefferson/Paine/Penn and adds SoL **per bell/cross worker**. Preview under-counted for colonies with those FFs or >1 worker. Fixed: preview now calls the `_ff` variants with the same nation FF lookups and the same per-worker SoL loop as `turn.c`. Regression: Phase C block in `test_turn.c` (`colony_preview_compute` w/ Jefferson granted). |
| **2026-08-15 fix:** Production tab fur trapper vs Henry Hudson | `colony_preview_compute`'s field-worker loop never checked `FF_HENRY_HUDSON`, so the Production tab showed half the real fur yield for a colony that owns Hudson (`turn_produce_one_colony` in `turn.c` doubles fur trapper output). Fixed: preview now applies the same doubling before the SoL add. Regression: new `test_turn.c` block (`AMER2.MP` site with fur yield, doubles stock exactly, preview matches). |
| **2026-08-15 fix:** Production tab hammers hidden without a queued project | `colony_preview_compute` only computed the Hammers row when `building_in_production >= 0`, but `turn_produce_one_colony`'s hammers block (turn.c "TURN5→6" comment) banks hammers and consumes lumber every turn a Carpenter is staffed, project or not — the preview silently under-informed the player (row just missing) whenever no Construction item was selected. Fixed: preview now mirrors the same three-way branch (queued project / no project but lumber available / no lumber, bank at raw output) as turn.c. Regression: new `test_turn.c` block (Carpenter's Shop, no project, checks both preview and the real EOT tick agree: hammers=3, lumber 10→7). |
| **2026-08-15 fix:** Production tab missing horse breeding | `colony_preview_compute` never simulated the ≥2-horses + food-surplus breeding turn.c does every EOT tick (Stable cap 4 vs 2), so the Production tab showed neither the Horses row nor the extra Food consumption from breeding. Fixed: preview now runs the same breed calc (incl. Stable check) and folds it into `goods[HORSES]`/`goods[FOOD]`/`food_net`. Regression: existing `test_turn.c` "colony horse breed" block extended to assert `colony_preview_compute` predicts the exact bred amount before the real tick runs. |
| **2026-08-15 fix:** William Penn crosses bonus — flat colony-total multiply vs. real per-Preacher-worker fold | `colony_prod_colony_crosses_ff` used to apply Penn's "+50% cross production" as a flat post-hoc `crosses * 150 / 100` on the whole colony total — including the colony-wide base/passive crosses (colony base +1, Church/Cathedral +1) that DOS's Penn check never touches, since it lives *inside* Preacher's own per-worker body (`FUN_15eb_1d4c`), not the nation-aggregate composer. Traced a previously-unresolved far call in that body (`15eb:1eaf CALL 0x1981:0x0000`) to `FUN_15eb_3960` (the same per-nation FF-ownership primitive already confirmed for Jefferson/Paine), reached via an overlay-segment-split tail; its arg `0x15`=21=`FF_WILLIAM_PENN`. Preacher's body falls through into this check *unconditionally* after the Cathedral doubling (Carpenter's body doesn't — it `JMP`s straight out), so Cathedral (×2) and Penn (×1.5) stack multiplicatively per worker, up to ×3 total for a skilled Preacher in a Cathedral colony whose nation owns Penn — not the flat ×1.5-of-everything the port did before. Fixed: `colony_prod_crosses_worker` takes `bool nation_has_penn` and applies `v += v >> 1` right after the Cathedral doubling; `colony_prod_colony_crosses_ff` takes `bool nation_has_penn` (was `int crosses_bonus_pct`) and threads it into each Preacher worker's own call instead of multiplying the returned total. Regression: `test_turn.c` direct `colony_prod_crosses_worker` checks for skilled+Cathedral+Penn (18) and unskilled+Cathedral+Penn (9); `test_founding_fathers.c`'s Penn fixture recomputed (12 → 11 — no Cathedral in that fixture, so only the skilled worker's own 6→9 gets it, not the old flat ×1.5 of everything). Full peel: [`manufacturing_worker_calc_1d4c.md`](../original_sources_annotated/turn/manufacturing_worker_calc_1d4c.md) Preacher-body section. |
| **2026-08-15 fix:** AI bells subsidy missing entirely | Player-confirmed on Viceroy difficulty: an AI colony's free-colonist Statesman nets 5 colony bells vs. 3 for a human colony in the same setup — a real, non-Founding-Father DOS effect the port never had. Traced to `FUN_15eb_1f72`'s bells composer: `bells += (pop+3)/5` on the Town Hall passive, gated by flag `0x12` (numerically = 18 = `FF_SIMON_BOLIVAR`, almost certainly coincidental reuse of the same per-nation flag-test primitive for an unrelated AI-difficulty bit — Bolivar's real effect is SoL +20%, unrelated to bells) AND the same AI/non-human table gate `colony_prod_sol_bonus_field` already uses. The arithmetic was already asm-read; only whether it was real and worth porting was in doubt — the player observation settled that. Fixed: `colony_prod_colony_bells_ff` takes `bool nation_is_ai` and adds `(pop+3)/5` right after the Town Hall +1 passive (all 3 call sites — turn.c's nation tick, colony_preview.c, and the SoL rebel-accumulator tick — pass `col1->player[nation_id].control != 0`). Regression: `test_founding_fathers.c` direct call (colonist_count=2 → subsidy=1, baseline 7→8). Full peel: [`nation_crosses_bells_1f72.md`](../original_sources_annotated/turn/nation_crosses_bells_1f72.md) item 4. |
| **2026-08-15 fix:** field-yield SoL fold order and expert road/river doubling | Player-confirmed on Viceroy: Expert Ore Miner, Hills+road+sentiment(+1) = 12 ore; Free Colonist, same tile = 6 — exactly ×2 at every step. The port's old field pipeline computed base+road/river, doubled for a matching expert, then added the signed SoL/Tory mod *externally, flat, after* `colony_yield_for_worker` returned (`turn.c`/`colony_preview.c`) — right for the free colonist (6) but wrong for the expert (predicted 11, not 12), because DOS folds a *positive* mod in **before** expert doubling (so it gets swept up by the ×2) and doubles the road/river bonus itself for a matching non-food/fish expert (or any Lumberjack) — neither of which the port did. Fixed: `colony_yield_for_worker` moved from `colony_production.c` to `colony_yield.c` (it needs static pipeline helpers private to that file) and gained a `sol_bonus` parameter; the full corrected step order lives in the new `colony_yield_pipeline`. `turn.c`/`colony_preview.c` no longer add SoL externally. `colony_yield_for_tile` (profession-less) is now a thin wrapper over the same pipeline. Also wired in the same pass (asm-confirmed, not independently player-tested): expert doubles the *additive* half of a special-resource bonus. Regression: `test_colony_yield.c` (Ore Miner case above); one pre-existing `test_units.c` road/lumberjack test needed its hardcoded `+1` updated to `+2` (Lumberjack's road/river unit is unconditionally 2, matching-expert or not — a real behavior change, not a new bug). Full derivation: [`terrain_yields.md`](terrain_yields.md) "Plow / road / river stacking" and the "Expert" step-6 note above it. |
| **2026-08-15 fix:** road bonus magnitude for fur/lumber | Player-confirmed on Viceroy: Expert Fur Trapper, Mixed Forest+road+sentiment(+2), Henry Hudson owned = 28 furs; Free Colonist, same tile = 14 — the port's SoL-fold/expert-road-doubling fix (above) alone predicted 24/12, short by exactly Hudson's known ×2 divided differently than expected. Ruled out a special resource (player-confirmed none present). Solved to `colony_yield_road_bonus` needing the same fur/lumber `+2` magnitude bucket `colony_yield_river_bonus` already had (river's own comment already flagged this split as "FreeCol classic/Col1", not DOS-confirmed) instead of the flat `+1` every other road job got — combined with Hudson's already-existing external ×2, both equations solve exactly (free 7×2=14, expert 14×2=28). Fixed: `colony_yield_road_bonus` now matches `colony_yield_river_bonus`'s per-job split. Regression: `test_colony_yield.c` (Fur Trapper case, pre-Hudson half: free=7/expert=14); `test_units.c`'s road/lumberjack test updated again (`+2`→`+4`, Lumberjack shares the fur/lumber bucket). Full derivation: [`terrain_yields.md`](terrain_yields.md) "Plow / road / river stacking". |
| **2026-08-15, closed (not a bug):** Jefferson/Press/Newspaper on the passive Town-Hall bell | Long-open question, finally resolved by re-reading `FUN_15eb_1f72`'s own literal order: `bells=1` then Jefferson's check runs *immediately*, before Paine, the AI subsidy, or anything else can inflate the value — so `bells>>1` is always `0` (`1>>1=0`), for *every* colony configuration, not just the ones tested. Whichever way DOS's bytes literally read, Jefferson's effect on the passive bell is provably unobservable. Player re-test on an AI colony (which an earlier, mistaken suggestion of mine proposed as a potential disambiguator) confirmed: still 1 bell, exactly as this proof predicts. Closed — no code change, and none is needed; this doesn't touch the already-correct per-worker Statesman Jefferson bonus. Full writeup: [`nation_crosses_bells_1f72.md`](../original_sources_annotated/turn/nation_crosses_bells_1f72.md). |
| **2026-08-15 fix:** settlement-view badges missing Henry Hudson | `colony_screen_draw_area_overlays` (minimap field-tile badges) and `colony_screen_draw_jobs_popup` (job-assignment dropdown) called `colony_yield_for_worker` directly with no Henry Hudson doubling, unlike the Production tab — an earlier pass flagged plumbing `col1` into `colony_screen_render`'s signature as "a larger, separate refactor, not attempted." Turned out to be overstated: `col1` was *already* a `colony_screen_render` parameter (used for `colony_screen_refresh_preview`) — the actual gap was just two internal static helpers not receiving it. Fixed: both now take `col1`, apply the same `yld*=2` Hudson check `turn.c`/`colony_preview.c` already do. SoL still intentionally excluded from badges (existing design, see UI table above — this fix is Hudson-only). |
| **2026-08-24 fix:** Phase K "ran out of raw material" chrome gated on building presence, not real demand | Resolved the "scratch-demand net-yield probe" lead statically (no live capture) — traced `FUN_15eb_0bd4`/`_0b96`/`_0b52` and their call site: DOS's demand word for a raw good is derived from *this tick's actual staffed worker output* of the paired finished good, not from "does the building exist." A colony with an unstaffed starter Blacksmith's House and 0 ore used to nag "Need ore." every turn even though nobody was making tools; DOS stays silent. Also proved the probe's "net output == 0" half already reduces to the port's existing `stock==0` check for every recipe ratio in this game — so only the gate needed fixing. Fixed: new `colony_craft_demand_mask` (`colony_craft.c`) reruns the same tier-scaled recipe pass as `colony_craft_one_colony`; `turn.c` Phase K now gates ORE/CANESUGAR/TOBACCO/COTTON/FURS/TOOLS(+muskets) on it instead of `turn_building_name_has`. Lumber/hammers and food unchanged (lumber interacts with the separate Autumn hammers-freeze; food isn't a craft recipe). Full derivation: [`colony_eot_production.md`](../original_sources_annotated/turn/colony_eot_production.md) Deep K. Regression: `test_turn.c` "build advisory K …" blocks updated to staff the recipe colonist (previously `building_type=-1`, encoding the old wrong behavior). |

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
