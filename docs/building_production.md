# Settlement building production

Reference for what colonists produce **inside** colony buildings (settlement view), which `@JOB` skills apply, and how that relates to the Production tab. Field / area yields: [terrain_yields.md](terrain_yields.md). Sentiment: [sons_of_liberty.md](sons_of_liberty.md).

## Sources

| Source | Role |
|--------|------|
| [`COLONIZE/NAMES.TXT`](../COLONIZE/NAMES.TXT) `@BUILDING`, `@JOB` | **Authoritative** construction hammers / tools×10 / min_colony; profession names and school tier |
| `FUN_15eb_1d4c` (per-worker manufacturing/production value) + `FUN_15eb_15c6` (upgrade depth 0/1/2) | **Authoritative** DOS manufacturing composer — deep peel: [`manufacturing_worker_calc_1d4c.md`](../original_sources_annotated/turn/manufacturing_worker_calc_1d4c.md). Tier rates 3/6/9, class-scale, the SoL/Tory term's tier-interaction and the factory-tier *input* ratio are **DOS-confirmed and wired** |
| — | `FUN_15eb_1d4c` = `15eb:1d4c`-`15eb:1f71` (549 bytes; `FUN_0000_7bfc` in `viceroy_overlays.*`), a 9-way switch on the worker's `@JOB` (9-17). **Work from the `.asm` / `ndisasm`, not the `.c`:** the `.asm` export just misses its inline jump table (re-disassemble the raw bytes), while the `.c` pseudocode in both `viceroy_unpacked_2.c` and `viceroy_overlays.c` merges in unrelated sound/timer-driver code. Byte-exact: scaling `CX = 100 − SoL%`, `AX = (byte[colony+0x1f] * CX + 50) / 100` (`+0x1f` = population), class gate `byte[bx+0x1a] < 4` / `DS:0x543f` stride `0x34`; sub-calls `FUN_15eb_0e18` occupation, `0e52` profession, `0274` colony SoL% |
| [`COLONIZE/Colonization.pdf`](../COLONIZE/Colonization.pdf) | Manual ch. 6 workplace rules, Production view, shortfalls; Building Chart **effects** (prefer NAMES for costs) |
| [`COLONIZE/README.TXT`](../COLONIZE/README.TXT) | Colony **Space** = one free production cycle; Cathedral min pop **8** (v3 fix) |
| [`colony_eot_production.md`](../original_sources_annotated/turn/colony_eot_production.md) | EOT phase order (`FUN_364b_0688`) |

**Retracted:** `VICEROY.EXE` @ `0x16103` is **not** a `3,6,8` tier-rate table (false cite).

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
4. **Hammers** — carpenters consume **lumber** toward `building_in_production`, **1:1 with the SoL-adjusted hammer count** (a high-SoL carpenter banking 14 hammers eats 14 lumber, not his sol-free base) — user-verified on `hammers_lumber.SAV`, port_bugs.md #163. This turn's field lumber is available: `FUN_15eb_1f72` raw 12683 passes gross hammers as lumber demand to `FUN_15eb_0b96`; `FUN_15eb_0b52` raw 10122-10138 computes unmet demand after gross lumber and starting stock, and the hammer output's input map at DS:0x2a2[16] is lumber (audit #896/#918). Hammers bank on **every** tick, whatever the season: `FUN_364b_0688` Phase L has no season term (port_bugs.md #466). The old Spring-only freeze rested on `colony-prod-tests/COLONY00→01_no-transports`, where no colony has a carpenter staffed (occupation 13), so its unchanged hammers prove nothing.
5. **Crosses / liberty bells** — immigration / independence counters (not warehouse cargo).

Manufacturing before hammers so ore→tools and cotton→cloth see same-turn field intake.

The **Custom House** auto-sell is part of step 1/3's cargo loop (DOS Phase B), so it runs *before* the food-growth test and before construction spends tools — port_bugs.md #1023.

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

**DOS-confirmed** 2026-08-15 for the six shared craft professions (skill-match flag doubles post-tier, via a compiler tail-merge into the Carpenter case body — see deep peel). Carpenter/Preacher's doubling in DOS is gated by a **colony-wide "owns the upgraded building" check** (Lumber Mill / Cathedral), not a per-worker skill-match flag; the port does the same: `base = skilled ? 6 : class_tag`, `+ sol_bonus`, then ×2 if the colony owns the upgrade (`colonies_has_building_row`, once per colony). Statesman's doubling *is* a direct skill-match flag.

Field experts: food/fish **+2**, other jobs **×2** — [terrain_yields.md](terrain_yields.md) (not blanket ×2).

---

## Building tier throughput (free colonist, per worker)

**DOS-confirmed** 2026-08-15 (see deep peel) — matches Building Chart “doubles” / factory **1.5×** efficiency exactly. DOS upgrade depth is `FUN_15eb_15c6` → **0 / 1 / 2** (house / shop / factory) / `FUN_15eb_039e` (owned-buildings-along-chain count, 1/2/3). Exact byte table in the EXE is **not** at `0x16103`.

| Tier | Depth | Building level (examples) | Free **output** / worker | Typical **input** / worker |
|------|------:|---------------------------|-------------------------:|---------------------------:|
| **1 — House / base** | 0 | `*'s House`, Carpenter’s Shop, Armory, Town Hall (bells), Church | **3** | **3** (1:1) |
| **2 — Shop / mill** | 1 | `*'s Shop`, Lumber Mill, Rum Distillery, Fur Trading Post, Magazine | **6** | **6** (1:1) |
| **3 — Factory** | 2 | `* Factory`, Textile Mill, Iron Works, Arsenal (**Adam Smith**) | **9** | **6** (1.5× — `input = (total_out * 2) / 3`, one floor divide over the colony total; **except the muskets chain**, see below) |

**Exceptions / special cases**

| Building | Notes |
|----------|--------|
| **Lumber Mill** | Manual: **doubles** hammers vs Carpenter’s Shop (3→**6**); lumber 1:1. |
| **Magazine** | Manual: **doubles** muskets vs Armory (3→**6**). |
| **Arsenal** | Factory-tier muskets **output** (9 per free colonist), but the tools **input** is 1:1 — **9 out / 9 in** (port_bugs.md #910). The muskets pair is the only chain DOS emits through `FUN_15eb_0b96` instead of `FUN_15eb_0bd4`: `FUN_15eb_1f72` raw 12689 closes the ledger with `FUN_15eb_0b96(0xe, gross[muskets] @DS:0x8de6)`, and both the factory `(G<<1)/3` discount (raw 10171) and the `(U*3)/2` unmet rescale (raw 10175-10180) live only inside `0bd4`. There is no `0bd4(0xe, 0xf)` call anywhere. The earlier “6 tools → 9 muskets” reading here was a port-side inference from the manual’s “half the tools” prose, not decomp, and is **wrong**. Armory 3←3, Magazine 6←6, Arsenal 9←9. |
| **Iron Works / luxury factories** | **6** raw → **9** finished. |
| **Printing Press** | **+50%** liberty bells colony-wide (manual). |
| **Newspaper** | **+100%** liberty bells colony-wide. |
| **Church / Cathedral** | Passive + worker crosses — **DOS-confirmed** 2026-08-15 (`FUN_15eb_1f72`, see [`manufacturing_worker_calc_1d4c.md`](../original_sources_annotated/turn/manufacturing_worker_calc_1d4c.md)): colony base **+1** cross/turn, **+1** independently if Church built, **+1** independently if Cathedral built — Church and Cathedral give the *same* +1 passive, not a scaled +2/+3 (that was manual/wiki-sourced and wrong). Worker rate: **3**/worker unskilled, **6** Firebrand Preacher, SoL folded in, then **doubled** (6/12) if the colony owns Cathedral specifically (not a flat Church-3/Cathedral-6 split — the doubling is colony-wide and stacks with skill). **William Penn** (FF `0x15`, the `15eb:1eaf` far call = `FUN_15eb_3960`'s overlay-split tail) is per Preacher worker, `v += v >> 1` right after the Cathedral doubling (the Preacher body falls through into it), so skilled + Cathedral + Penn = 18, unskilled = 9; the colony base / passive crosses never get it. |
| **Town Hall** | Worker rate **3**/worker unskilled, Elder Statesman ×2 (`FUN_15eb_1d4c`). The colony-wide sequence is `FUN_15eb_1f72` raw 12603-12650, on the bells word DS:0x8dec, in this exact order: worker loop → **`+= 1` unconditional** (raw 12619 — a bare add with **no** Town Hall `038e(9)` test, unlike the crosses word just above it, which does call `038e(0x25)`/`038e(0x26)`; a colony with no Town Hall row still earns the passive bell) → Jefferson (`3960(nation,0x0f)`) `+= v>>1` → Paine (`3960(nation,0x11)`) `+= (tax*v)/100` → AI subsidy (`3960(nation,0x12)` + the AI/non-human table gate) `+= (pop+3)/5` → `+= byte[0xa892]` (cleared to 0 two statements earlier, a no-op) → Newspaper (`038e(0x14)`) `v <<= 1` **else** Printing Press (`038e(0x13)`) `+= v>>1`. port_bugs.md #926/#927. The SoL term folds per Statesman worker only, before his ×2 (the `1d4c` Statesman body is `v = base + sol; if (skill) v <<= 1`, no FF call); Jefferson, Paine, the subsidy and Press/Newspaper act once on the combined worker + passive total (matched exactly on 7 player-reported colonies, `dutch-reports.SAV`). So an unworked Town Hall gets nothing from Jefferson (`1 >> 1 = 0`). Newspaper and Press are exclusive: a Newspaper colony also owns the Press row, so an additive sum double-bonuses. |
| **Docks** | Enables fishermen on ocean/lake surrounds (no processing). |
| **Schoolhouse / College / University** | Teach (faculty 1 / 2 / 3) — see [Education](#education-fun_364b_0688-phases-fg). |

---

## Education (`FUN_364b_0688` phases F–G)

Source: `viceroy_unpacked.c` ~57502-57589; GAME.TXT `@TRAINFAIL` / `@TRAINCRIMINAL` /
`@TRAININDENTURED` / `@TRAINPROFESSION`. Ported in full 2026-09-04 (port_bugs.md #380 — the
previous port could not graduate anyone).

**Teacher.** Any colonist whose *work slot* is a school building and whose *specialty*
has an @JOB school level of 1–3. The specialty is what gets taught; there is no
"teacher" profession. Level 4 (@JOB 18 Teacher, @JOB 19 Colonist, Indentured Servant,
Petty Criminal, Indian Convert) cannot teach.

**Time.** From the **teacher's** profession level, not the building: level 1 → 4 turns,
level 2 → 6, level 3 → 8. The building tier is enforced only at assignment
(`colonies_assign_workplace` → `@NEEDCOLLEGE` / `@NEEDUNIVERSITY`). The counter
(`turns_in_job`, DOS `FUN_281f_0d1c`/`0a7e`) ticks for every colonist every turn and is
zeroed the moment a teacher qualifies — whether or not a student was available.

**Students.** Every colonist **anywhere in the colony**, not just ones placed in the
school: professions `0x13`/`0x1c` (free colonist), `0x19` (indentured), `0x1a`
(criminal). Indian Converts (`0x1b`) are **not** students. One is drawn at random
(`FUN_281f_04d4(0, n-1)`) and removed from the pool.

**Outcome**, in priority order — the first two consume the teacher's turn just like a
specialty graduation:

| Student was | Becomes | Popup |
|-------------|---------|-------|
| Petty Criminal (`0x1a`) | Indentured Servant (`0x19`) | `@TRAINCRIMINAL` (DOS tag `0xdf1`) |
| Indentured Servant (`0x19`) | Free Colonist (`0x1c`) | `@TRAININDENTURED` (`0xdff`) |
| Free Colonist | **the teacher's own profession** | `@TRAINPROFESSION` (`0xe0f`) |

At most **3** teachers graduate per colony per tick (`local_6e < 3`). A ready teacher
with an empty student pool raises `@TRAINFAIL` (`0xde7`) and ends the pass. All four
popups are suppressed when the "report when colonists trained" option bit
(`0x5384 & 0x80`) is set.

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
| Teacher | 18 | Expert Teacher | Schoolhouse / College / University | **Occupation slot, not a specialty.** DOS marks a colonist as teaching by setting his *work slot* to 18 (`FUN_281f_0c0e == 0x12`); @JOB 18's own school level is **4**, so a colonist whose *specialty* is "Expert Teacher" can neither teach nor be taught. Unskilled teacher → `@NOTEACHER`; job tier above the school → `@NEEDCOLLEGE`/`@NEEDUNIVERSITY` |

> **"Expert Teacher" the colonist type was cut from the final DOS game.** The devs removed it before release: the Europe recruit-pool remap (`FUN_38fd_46d4`) folds @JOB 18 into Master Carpenters, schools can't produce it (a graduate takes the teacher's own specialty), its school level 4 blocks it from teaching or studying, and the labor report skips it. Only the NAMES.TXT string, sprite 99 and this occupation-slot id survive as leftovers. Any DOS code path that appears to create, hire or specially handle an Expert Teacher *unit* is a dead end — do **not** port it.

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
| Fort | 120 | 100 | 3 | Defense ×2.5 (`local_1a=6`, +150% as in the manual); fortify does not stack — [combat.md](combat.md) |
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
| Capitol | 400 | 100 | 16 | **Cut — never buildable** (see below) |
| Capitol Expansion | 400 | 100 | 16 | **Cut — never buildable** (see below) |
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

**Cut rows.** DOS's own "can this colony build it" gate `FUN_15eb_3650`
hard-zeroes three `@BUILDING` file indices regardless of colony state:
`0x0a` and `0x0b` (the two unfinished Town Hall upgrades — `PEDIA.TXT` names
`0x0b` "COLONIAL ASSEMBLY") and `0x1e` (**Capitol**). `0x1f` (Capitol
Expansion) is not zeroed there but is unreachable behind the Capitol as its
prerequisite. `PEDIA.TXT` keeps title-only stubs for all four, which is the
giveaway. The port carries the same block (`colonies_building_is_buildable`),
hides Capitol / Capitol Expansion from the Colonizopedia building list (as DOS
does: user-observed, they never appear in the DOS game), and
has no AI preference for them. `+0x96 capitol_level` and the destroy-a-Capitol
arm stay ported — the field is real, it just never grows.

Starter colonies grant several houses / Carpenter’s Shop / Town Hall without spending the chart cost at founding; `NAMES` costs apply if rebuilt later.

### Unit projects (construction codes 42–48)

`building_in_production` doubles as a *unit* project slot: raw codes 42–48 =
`@UNIT` rows 11–17 (`FUN_15eb_32f8`; Man-O-War, row 18, is out of range), cost
from the `@UNIT` cost / tools columns via `FUN_15eb_33aa` (`cost × 0x20` with
a 0x28 floor, `tools × 10`). Decode and cost arithmetic:
[units.md](units.md#unit-construction-raw-code-decode),
[units.md](units.md#units_build_project_info).

| Code | `@UNIT` | Name | Ham | Tools | Gate (`FUN_15eb_3650` raw 13714-13735) |
|-----:|--------:|------|----:|------:|---|
| 42 | 11 | Artillery | 192 | 40 | `has_building(3)` = **Armory bit, literally** |
| 43 | 12 | Wagon Train | 40 | 0 | per-nation cap only (see below) |
| 44 | 13 | Caravel | 128 | 40 | `has_building(8)` = **Shipyard** |
| 45 | 14 | Merchantman | 192 | 80 | Shipyard |
| 46 | 15 | Galleon | 320 | 100 | Shipyard |
| 47 | 16 | Privateer | 256 | 120 | Shipyard |
| 48 | 17 | Frigate | 512 | 200 | Shipyard |
| — | 18 | Man-O-War | — | — | **not buildable** (out of decode range) |

Ships need the Shipyard and *nothing else*: no Docks, no Drydock, no
population test and no coastal test of their own (the Shipyard's own
`@BUILDING` row carries the coastal requirement and min pop 8).

Completion is `FUN_364b_0114` (raw 56897), reached from the colony EOT at raw
57742 once `hammers >= need`: the tools deficit warning fires first (a human
sees popup id `0xea1`; an **AI nation simply has its tools stock set to the
requirement**), tools are debited, then kind 2 spawns the unit via
`FUN_281f_095c(unit_index, colony_nation, colony_x, colony_y)` — on the colony
tile, ships included — bumps `unit_count[nation][unit_index]` (`DS:0x924c`,
stride 0x13), raises `colony+0x1c | 0x80` and zeroes `colony+0x92` (hammers).
DOS never clears `building_in_production` on completion.

#### Building bits are cumulative — `has_building` is one bit

Asked because the Artillery gate looked like it wanted an Armory/Magazine/
Arsenal fold. It does not:

* `FUN_15eb_035e` (raw 9545) is a plain single-bit test —
  `bits[n>>3] & 1<<(n&7)` over the colony's `+0x84` bitfield. No chain fold.
  (`FUN_15eb_039e` / `03d6` exist and *do* walk the chain via the `@BUILDING`
  predecessor byte `DS:0x8f85 + idx*0xc`, but they **count** tiers and are
  used elsewhere, not by any gate.)
* the completion writer `FUN_15eb_1030(idx, 1)` (raw 11182) only ORs the new
  bit in — **it never clears the tier below**.
* `FUN_15eb_3650`'s building arm refuses an upgrade whose predecessor byte is
  set but not owned, so you cannot reach Magazine without an Armory.

So bits are **cumulative** and a normally-grown Arsenal colony still has the
Armory bit: `has_building(3)` and the chain fold agree. Real saves confirm —
over 977 colonies in `original_saves` the `armory` mask is only ever 0/1/3 and
`docks` only 0/1/3/7. They disagree only where a lone upper bit is real, which
**does** happen: pillage clears one tier's bit on its own, and those saves
carry `carpenters_shop` = 2 (×16), `printing_press` = 2 (×14), `church` = 2
(×2) — see `save_format_map.md`. DOS refuses Artillery in such a colony, so
the port now gates on the Armory bit literally (2026-09-17). Import/export
were already bit-exact and cumulative, chain position *i* = bit *i*; no save
change. Same reading covers the Shipyard gate (`has_building(8)`), which never
consults Docks/Drydock.

#### Wagon Train per-nation cap (ported 2026-09-17)

`FUN_15eb_3650` (raw 13736-13740) and `FUN_364b_0114` (raw 56926-56933) both
compare `colony_counts[nation]` (`DS:0x9298 + n`) against
`unit_type_counts[nation][12]` (`DS:0x924c + n*0x13 + 12`) — both the
`FUN_4962_0018` census window, i.e. **wagons < colonies**. The listing site
just drops the project; the completion site is louder: `FUN_281f_09ae(0,
colony_count)` fills `%NUMBER0`, pops `@NOMOREWAGONS` (tag `0xd62`), ORs
`+0x1c | 0x80`, and **returns** — the project is *not* cleared and the hammers
are *not* zeroed, so a capped colony re-pops the message every turn until a
new colony is founded. `FUN_364b_0114` also `++unit_type_counts[n][idx]` on
every spawn (raw 56938), which is what stops two wagons finishing in one EOT.

Port map: `units_build_code_to_index` / `units_build_project_info`
(`units.c`), `colonies_wagon_cap_reached` /
`colonies_nation_colony_count_census` / `colonies_unit_project_available` /
`colonies_list_buildable` / `colonies_set_construction_ex` /
`colonies_try_complete_unit_construction` (`colony_build.c`),
`turn_run_colony_unit_construction` (`turn_colony.c`). `ColoniesBuildableOpts.col1`
carries the census to the gate. Tests: `unit_colonies` cases
`unit_ship_construction`, `unit_wagon_cap_and_armory_gate`.

AI colonies pick unit projects through the `FUN_5952_035e` cascade (`ai_euro_5952_set_unit_project`, `ai_euro_colony_jobs.c`; port_bugs.md #483).

Manual chart often listed shop min-pop **4** and Church hammers **52** — **wrong vs NAMES** (shops min-pop **1**; Church **64**).

---

## Production modifiers (sentiment)

Every production unit takes the signed SoL/Tory net mod `−⌊tories/(10−diff)⌋`
+1 per SoL latch (≥50%, ≥100%), `colony_prod_sol_bonus`, EOT and preview —
formula in [sons_of_liberty.md](sons_of_liberty.md#production-modifier), Tory
threshold by difficulty in [difficulty.md](difficulty.md). A negative mod
reduces output (then clamp ≥ 0) for manufacturing, hammers, bells and crosses;
field yields have their own fold order and AI zero-out
([terrain_yields.md](terrain_yields.md)).

---

## UI: settlement badges vs Production tab

| UI element | Should show | OpenCol port today |
|------------|-------------|------------------|
| **Settlement badges** | Building slots | Workplace output-type + amount (Town Hall / Church include free passives even when empty). The amount is the staffed workers' **capacity** (`craft_capacity`, pre-stock-clamp), not this tick's input-clamped actual, and it folds the colony's SoL. Only Town Hall shows bells: Printing Press / Newspaper have no worker slot and draw no badge |
| **Production tab** | Every cargo good / shortfall + hammers | [`colony_preview.c`](../src/core/colony_preview.c) via [`colony_production.c`](../src/core/colony_production.c); crosses/bells → people meters. The raw input good's own row carries the unmet input demand as its shortfall (Cotton 5 produced / 5 short beside Cloth), town-commons secondary yield included; Horses shows `potential − bred` (DOS scratch `0x8e6a`, report-only). The preview mirrors the EOT tick (FF bonuses, Hudson, horse breeding, AI food subsidy `difficulty >> 1`); `smoke_colony_preview01` guards it |
| **Production strip** | Sum of assigned workers’ output | `colony_prod_building_display_output_sol()` → `colony_prod_worker_building_output_ctx()` |
| **Construction Change list** | Buildable projects, min-pop, coastal docks, Adam Smith, Stuyvesant; owned refuse `@ALREADYHAVE` / `@NOMOREWAREHOUSE` | [`colonies_list_buildable()`](../src/core/colony_build.c) + `colonies_emit_already_have_chrome` |

Shared formula (port, `colony_prod_manufacturing_output` — DOS-confirmed):

```
tag = class_tag(profession)              ; 3 free / 2 indentured / 1 criminal-convert
v = tag + sol_bonus                       ; sol_bonus signed — Tory penalty reduces output
if tier >= shop:    v += tag
if tier == factory: v += v >> 1           ; ×1.5, floor
if skill matches:   v *= 2                ; whole running total, sol_bonus included
output = max(v, 0)
input = (G << 1) / 3 at factory tier, else G  ; G = the colony's summed gross for the
                                              ; finished good, one truncating divide
                                              ; (colony_prod_chain_input_for_total_output);
                                              ; muskets chain 1:1 at every tier
```

### Factory input

The factory discount is real and tracks the **SoL-adjusted** output, not the
base rate: Textile Mill, free colonist, +2 sentiment made 12 cloth from 8
cotton (player-confirmed, Viceroy; `(12*2)/3 = 8`, where the base rate would
give 6). `FUN_15eb_0bd4` (raw 10159-10182) → `0b96` → `0b52` write the demand
word, `FUN_15eb_0b0c` (raw 10102-10110) nets `gross - demand - unmet`. Only the
five `0bd4` chains (ore→tools, tobacco→cigars, cotton→cloth, furs→coats,
sugar→rum) get the discount; tools→muskets is emitted through `0b96` directly
(`colony_prod_chain_input_is_flat`, port_bugs.md #910). A factory gross of 1
demands `(1*2)/3 = 0` and is produced free (#913).

### Partial input (2026-09-23, port_bugs.md #897 / #898)

Output when the warehouse cannot cover the full demand is **not** a
proportional rescale. `FUN_15eb_0bd4` (raw 10159-10183) computes the unmet
word and back-converts it, and Phase B (`FUN_364b_0688`, raw 57238-57253)
applies the net:

```
D = colony gross demand   ; (G << 1) / 3 at factory tier, else G
U = D - (stock + this tick's input production)   ; when positive, else 0
if (U != 0 && G != D)                  ; factory tier only
    U' = (U == D) ? G : (U * 3) / 2
out = G - U'                           ; clamped at 0
stock[in] -= D                         ; clamped at 0
```

So a factory with G = 9 (D = 6) and stock S yields
S=0→0, 1→2, 2→3, 3→5, 4→6, 5→8, ≥6→9. House/shop tiers are 1:1 (D = G), the
back-conversion never fires and `out = min(stock, G)`.

**Controller gate**: for a colony whose owner is not the human seat
(`3 < nation || DS:0x543f[nation] != 0`, raw 57241-57243) Phase B composes
`gross - demand` and drops the `- unmet[input]` term entirely — an AI factory
always yields full capacity and its input stock merely clamps at 0. Same gate
as the AI food subsidy.

The Phase K "Need sugar/ore/..." crumbs (raw 57696-57728, port_bugs.md #911)
are seven independent tests `unmet[input] != 0 && FUN_281f_0b50(out) == 0`,
i.e. the **finished** good's net (gross − demand − unmet), not the raw good's
stock. `unmet` comes from this tick's staffed demand, so an unstaffed building
with no input stays silent, and a colony that converted its last stored input
still produces and stays silent (#899). Lumber derives its unmet from the
Phase A hammer demand and the pre-debit stock (#918).

`sol_bonus` folds in *before* tier/skill scaling — it is not a flat post-hoc
add on either output or input. Badges (area-view field tiles and settlement
buildings) fold it the same way the Production tab does; `sol_bonus=0` is only
for callers that want the un-modified base rate.

### Horse breeding

`FUN_15eb_1f72`'s horse tail (viceroy_unpacked.c ~12649-12690, asm
15eb:2300-2392), `colony_prod_horse_breed`:

```
potential = horses < 2 ? 0 : ceil(horses / divisor) * 2   ; divisor 25 with a Stable
                                                          ; (038e(0x11)), else 50
bred      = min(potential,
                ceil(max(0, food_gross - pop*2) / 2),
                warehouse_cap - horses)
horses += bred; food -= bred                              ; shortfall = potential - bred
```

The static read alone suggested DOS applies the uncapped `potential`; that
overshoots all 13 colonies of `golden_colony_prod01`/`02` by exactly
`potential - bred`, while the capped figure matches all 13. Which overlay
callee does the capping is not resolved statically.

---

## OpenCol implementation map

| Concern | Module |
|---------|--------|
| Shared production rules | [`colony_production.c`](../src/core/colony_production.c) |
| Manufacturing recipes | [`colony_craft.c`](../src/core/colony_craft.c) |
| Turn production + hammers | [`turn_production.c`](../src/core/turn_production.c) |
| Production tab preview | [`colony_preview.c`](../src/core/colony_preview.c) |
| Settlement strips | [`colony_screen.c`](../src/core/colony_screen.c) |
| Building definitions | [`colonies_load_buildings()`](../src/core/colony.c) ← `NAMES.TXT` |

---

## See also

- [terrain_yields.md](terrain_yields.md) — area / town-commons field production
- [sons_of_liberty.md](sons_of_liberty.md) — SoL / Tory production mod
- [manual_gap.md](manual_gap.md) — feature checklist
- [colony_eot_production.md](../original_sources_annotated/turn/colony_eot_production.md) — EOT phases
