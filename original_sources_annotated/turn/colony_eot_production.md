# Colony EOT production (`FUN_364b_0688`)

Layer D section map for the per-colony end-of-turn production / SoL /
construction tick. Active colony via `DS:0x8542`. Caller: nation EOT
`FUN_3844_00f2` → thunk `291f_0950`.

Decomp: `viceroy_unpacked.c` **57152–57951** (~800 lines). Next sibling:
`FUN_364b_1aec` @57955.

Orchestration: [`between_turns.md`](between_turns.md) ·
[`docs/turn_between_players.md`](../../docs/turn_between_players.md) ·
[`docs/building_production.md`](../../docs/building_production.md).

**Port status:** Linux **Partial** — spine in `turn_run_colony_production` /
`turn_produce_one_colony` (`src/core/turn.c`); shared rules in
`colony_production.c` / `colony_craft.c`. **Birth + starve-kill Done** (I–J).
**AI dump-sell Done** thin (O). **Education F–H Done** thin. **Phase D SoL chrome Done** thin. **Inefficient-gov chrome Done** thin.
K / P msgs **mapped**; port **Done** thin (was stale here — see Deep K /
Linux correspondence below; 2026-08-24 demand-gate fix).

## Sibling — `FUN_364b_03f6` (coastal fort fire)

Full map: [`coastal_fort_fire.md`](coastal_fort_fire.md). Nested here at
`57227` (`291f_09ce`); Linux SETUP after all colony production (reshape).

## Colony offsets touched

| Offset | Role |
|--------|------|
| `+0x1a` | Owner nation |
| `+0x1c` | `colony_flags`: sol_100=`0x02`, sol_50=`0x04`, inefficient_gov=`0x08`, build_complete=`0x80` |
| `+0x1f` | Population |
| `+0x90` | `cargo_produced_mask` |
| `+0x92` | Hammers bank |
| `+0x94` | Build-menu project index |
| `+0x95` | Warehouse expansion level |
| `+0x97` | Ore/silver depletion counter |
| `+0x9a..` | `stock[16]` (food @ `+0x9a`; tools @ `+0xb6`) |
| `+0xc2/+0xc4`, `+0xc6/+0xc8` | Rebel SoL accumulators |

Scratch: `DS:−0x7238` (gross), `−0x71f6` (reserve). Net: `281f_0b50` → `15eb_0b0c`.

## Phase / LAB map

| Phase | Lines (approx) | What |
|-------|----------------|------|
| **A — Bind + side pulses** | 57223–57236 | Clear `0xa898`; bind `09e6`; nation `0582`; **fort fire** `09ce`→`03f6`; compose `0c22`; bells `0b50(0x12)` → `09f8`→`4345_0a22`; warehouse `0d3a`; clear `+0x90` |
| **B — Apply cargo 0..15** | 57238–57348 | Net yield → stock; food difficulty bonus; Custom House sell>99 leave 50; produced-mask; surplus vs cap |
| **C — SoL accumulators** | 57349–57414 | SoL % `0c86`; rebel pairs `+0xc6`/`+0xc2` |
| **D — SoL / Tory / starve flags** | 57415–57485 | Latch bits 2/4/8; msgs `0xd8a`…`0xddd` |
| **E — Surplus re-clamp** | 57486–57501 | Re-min surplus |
| **F — Education scan** | 57502–57539 | Teachers / schoolhouse students |
| **G — Education graduate** | 57540–57589 | Pair students; set job; msgs |
| **H — Random skill** | 57590–57614 | LCG specialty discover |
| **I — Birth** | 57615–57622 | Food≥200 → spawn Free Colonist |
| **J — Starvation kill** | 57623–57695 | `8e5a`/`8e32` food scratch; `@FOODLOW` / kill / abandon |
| **K — Build advisories** | 57696–57728 | Missing building msgs |
| **L — Hammers / construction** | 57730–57787 | Add hammers; spend tools; complete `097a`→`0114` |
| **M — Crosses** | 57788–57789 | Scratch → nation crosses |
| **N — Farmer pressure** | 57790–57805 | Count farmers vs food |
| **O — AI dump-sell + trim** | 57806–57873 | Non-human sell; spoilage clamp |
| **P — Spoilage msgs** | 57874–57931 | Multi-cargo / century dialogs |
| **Q — Depletion** | 57932–57944 | `+0x97` wrap → map feature 4 |
| **R — Epilogue** | 57945–57950 | Flush colony screen; `DS:0x34a=−1` |

## Major callees (thunk → real)

| Role | Thunk | Real |
|------|-------|------|
| Bind colony | `281f_09e6` | `15eb_002c` |
| Fort fire | `291f_09ce` | `364b_03f6` |
| Compose yields | `281f_0c22` | `15eb_3956` |
| Net yield | `281f_0b50` | `15eb_0b0c` |
| Bells + FF | `291f_09f8` | `4345_0a22` |
| Warehouse cap | `281f_0d3a` | `15eb_0a50` |
| Custom House gate | `291f_09c0` | `364b_0636` |
| Sell + tax | `291f_0a2e` | `38fd_1dfa` |
| SoL % | `281f_0c86` | `15eb_0274` |
| Spawn / remove | `095c` / `0a9c` | `1427_06b4` / `15eb_0d04` |
| Complete build | `291f_097a` | `364b_0114` |
| Depletion tile | `291f_0988` | `364b_033a` |

## Deep — B / C / D formulas

### B — cargo apply (57238–57348)

- Per cargo `0..15`: net = `0b50(cargo)`; AI/Indian uses scratch gross−reserve
  (`−0x7238` / `−0x71f6`) instead of live net.
- Food (`cargo==0`) AI: `+= difficulty>>1` (`0x53a6`).
- Add into `stock[+0x9a+2*c]`; floor 0.
- Custom House (`09fc(0x12)` + mask): if stock>99 and eligible → sell
  `stock−50` via `0a2e` (leave 50). Linux: `europe_custom_house_autosell`.
  **Full read 2026-08-28 (57257–57330):**
  - Human colonies are shut while `colony+0x1b & 3` (enemy armed ship /
    MoW nearby); AI colonies ignore that.
  - Gate by controller: human → `281f_0cfe` = `15eb_0302` (colony `+0x8a`
    bit per cargo = `custom_house_bits`); AI → `291f_09c0` = `364b_0636`
    (deny Food/Lumber/Horses/Tools/Muskets; Ore also denied when building
    3 or `0x8de4`/`0x8de6` set). The Lumber term never applied to the human
    — this was the P4.4 "Lumber sells anyway" contradiction.
  - Price `291f_09ea` = `38fd_0040` = `euro_price − 1`; gross = price·amt;
    tax = `(tax_rate·gross)/100` unless WoI (`0x5382&1`); net = gross−tax.
  - `0aba` treasury += net; `0a2e`→`38fd_1dfa` ledger (see
    `europe_nation_eot.md`); nation `+0x22` (royal_money) += tax, `+0x26`
    (cumulative net trade income) += net.
  - Human only: message box assembled 0056/006a/0074/007e/0088/07d4
    (colony, amt, cargo, gross, tax%, tax, net) + sound `0x78` when
    `0xa897` set. Linux: one OK popup from `europe->status` per colony.
- OR `cargo_produced_mask` (`+0x90`) when net>0; surplus clamp vs warehouse
  `0d3a` → `aiStack_e4[c]` caps.

### C — SoL accumulators (57349–57414)

- SoL % = `0c86` → `local_b8`.
- Rebel/Tory pair words `+0xc6/+0xc2` (and siblings) accumulate from bells
  vs Tory pressure; decade-crossing chrome gated by prior SoL decade
  (`local_8e`).

### D — SoL / Tory / starve latches (57415–57485)

| Condition | Flag `+0x1c` | Msg (human) |
|-----------|--------------|-------------|
| SoL ≥50 and bit4 clear | OR **0x04** (sol_50) | `0xd8a` |
| SoL ≥100 and bit2 clear | OR **0x02** (sol_100) | `0xd98` |
| SoL &lt;95 with bit2 set | AND ~0x02 | `0xda7` |
| SoL &lt;50 with bit4 set | AND ~0x04 | `0xdb4` |
| Decade up / down | chrome only | `0xdc1` / `0xdc8` |
| Tory pressure ≥ difficulty band | OR / clear **0x08** | `0xdd1` / `0xddd` → port `@INEFFICIENT`/`@EFFICIENT` on that same Col1 bit3 (`COLONIZE_COLONY_FLAG_INEFFICIENT_GOV`) |

Bit3 is the Tory latch above in the port too (2026-09-04). Linux's own
food-vs-need reading, previously `COLONIZE_COLONY_FLAG_STARVATION` on this bit,
is the runtime-only `ColonizeColony.food_shortfall_latch`. `colony_prod_refresh_sol_flags` covers sol_50/100
**one-step** (majority then unanimous on separate ticks). Human chrome:
`turn_emit_sol_phase_d_chrome` in `turn.c`.

## Deep — F / G / H education

### F — scan (57502–57539)

For each colonist `i < pop`:

- `0d1c` → turns-in-job counter; `0c0e` → specialty; `0c54` → current job.
- Jobs **0x19 / 0x1a / 0x1c / 0x13** (schoolhouse student band) → append to
  student list `aiStack_126` (`local_c4++`).
- Specialty **0x12** (teacher) and &lt;3 teacher slots: map student-job →
  schoolhouse level via table `job*8 + −0x715a`; need turns 4/6/8 by level;
  if turns-in-job ≥ need → push teacher job into `aiStack_7e[local_6e++]`.
- `0a7e` refresh colonist chrome.

### G — graduate (57540–57589)

For each pending teacher slot:

- If no students: msg `0xde7` and break.
- Pick random student index `04d4(0, c4−1)`.
- If student job **0x1a** → set job **0x19** (`0cae`); msg `0xdf1`.
- Else if **0x19** → set **0x1c**; msg `0xdff`.
- Else set job = teacher’s target from `aiStack_7e`; subst name `−0x715e`;
  msg `0xe0f`.
- Compact student list.

### H — random skill (57590–57614)

Per colonist: skip Treasure job `0x1b`; skip if `0c9a` says already skilled;
specialty in `1..4` and nation skill-flag at `specialty−0x6bd0` clear:

- Threshold RNG: base 99; job 0x19 → 199; job 0x1a → +200.
- On roll 0: set skill flag; `0cae` assign specialty; msg `0xe1f`.

**Port:** F–G **Done** thin (`turns_in_job` + Teacher in Schoolhouse/College/
University); ladder Criminal→Indentured (`@TRAINCRIMINAL`), Indentured→Free
(`@TRAININDENTURED`); Free/Convert → teacher `field_job` specialty if set, else
Farmer or Carpenter (`@TRAINPROFESSION`); no-students status + `@TRAINFAIL`
chrome **Done** thin.
H random field skill **Done** thin (Free Colonist / Indentured Servant /
Petty Criminal on field 0..4, `dos_rng_range(rng, 0, 99|199|299)` → that
profession + `@TRAINPROFESSION` chrome; **2026-08-24 fix:** was Free-Colonist-
only and used an ad hoc non-DOS PRNG instead of the shared turn `rng` — now
uses `dos_rng_range` like DOS's `FUN_281f_04d4` and applies the Indentured
(199)/Criminal (299) denominators; no `rng` (e.g. deterministic-production
test callers) → phase skipped rather than mis-reading `dos_rng_range(NULL,
...)`'s "returns lo" convention as a guaranteed hit). Field-job lower bound
still ported as the wider `0..4` rather than DOS's literal `1..4` (see
`specialty` vs `field_job` ambiguity above — unresolved, not guessed).
Nation skill-flags / deep school-job tables **PARKED**. `0x5384|0x80` gates
education msgs.

## Deep — J food scratch / `@FOODLOW` (57623–57636)

Compose (`FUN_15eb_0b52`) writes per-cargo:

| DS | Meaning |
|----|---------|
| `0x8e32` | `max(0, consumption − production)` — eating into stores |
| `0x8e5a` | `max(0, consumption − stock − production)` — true starve deficit |

`@FOODLOW` (`0xe5e`) only when **`8e5a == 0`** and **`8e32 != 0`** and
**stock < `8e32 × 4`** (and report bit `0x5384&0x40` clear). Surplus harvest
(`production ≥ consumption` → `8e32==0`) never warns. Linux:
`food_shortfall = consumed − field_food` with the same stock gate.

## Deep — K build advisories (57696–57728)

Gated `!(0x5384 & 0x20)`. Scratch demand words vs missing net yield:

| Scratch | Probe `0b50` | Msg |
|---------|--------------|-----|
| `0x8e64` | cargo 0x10 (hammers/tools class) ==0 | `0xe66` |
| `0x8e60` | 0xb | `0xe6d` |
| `0x8e5e` | 0xa | `0xe74` |
| `0x8e5c` | 9 | `0xe7c` |
| `0x8e62` | 0xc | `0xe86` |
| `0x8e66` | 0xf == 0xe (paired) | `0xe8b` |
| `0x8e76` | 0xf ==0 | `0xe8f` |

**Port:** hammers-zero / tools / lumber/ore/food / rum/cigars/cloth/coats /
muskets(+tools paired) status **Done** thin; `0x5384` report-bit gates **Done**
thin (show when bit clear).

**2026-08-24 fix — scratch-demand net-yield probe resolved statically (no
live capture needed):** traced `FUN_15eb_0bd4`/`FUN_15eb_0b96`/`FUN_15eb_0b52`
(all in `viceroy_unpacked.c` near `10102`–`10182`) and their one real call
site (`~12680`–`12689`, the food/lumber/tools/raw-material demand composer
that runs before this phase): `local_6` (the "consumption" DOS feeds into
each raw good's demand word) is derived from *this tick's actual tier-scaled
worker output* for the paired finished good (`FUN_15eb_0bd4(raw, output)` —
`(1,9)`=sugar→rum, `(2,10)`=tobacco→cigars, `(3,0xb)`=cotton→cloth,
`(4,0xc)`=furs→coats, `(6,0xe)`=ore→tools; lumber/tools set directly via
`FUN_15eb_0b96(5, …)`/`FUN_15eb_0b96(0xe, …)`) — i.e. **demand is zero
whenever nobody is staffed producing that output**, not merely "the building
exists." Separately proved the probe's own "net output of the finished good
== 0" half always reduces to `stock[in_cargo] == 0` for this game's recipe
ratios (output tier ≥ input tier for every recipe here, so any nonzero input
yields ≥1 output) — so that half of the port's existing `stock==0` check was
already right; only the gate was wrong. Fixed: `colony_craft_demand_mask`
(`colony_craft.c`) reruns the same tier-scaled recipe pass
`colony_craft_one_colony` already does this tick and reports which raw goods
have a real (staffed, sol_bonus-folded) input requirement; `turn.c`'s Phase K
block now gates ORE/CANESUGAR/TOBACCO/COTTON/FURS/TOOLS(+muskets) on that
instead of `turn_building_name_has`. Lumber/hammers kept its existing
building-name gate (interacts with the separate Autumn hammers-freeze,
not disentangled here) and food is unaffected (not a craft recipe). Fixes a
real false-positive: a colony with an unstaffed starter Blacksmith's House
and 0 ore used to nag "Need ore." every turn even though nobody was trying to
make tools; DOS stays silent (demand word is 0) in that case. Regression:
`tests/unit/test_turn.c` "build advisory K …" blocks updated to staff the
recipe's colonist (previously left `building_type=-1` — encoded the old,
wrong "building merely exists" behavior).

**2026-08-24 fix (round 3) — lumber disentangled from the Autumn freeze:**
the "kept its existing building-name gate" line above turned out to be more
separable than it looked. The Autumn freeze (see the hammers block comment
in `turn.c`) only blocks hammers *production*/lumber *consumption* on Autumn
ticks — it says nothing about whether the "Need lumber." message itself
should also go quiet then, and that specific question is still open (no
located write site for the DOS demand word this message mirrors, `DS:0x8de8`
— checked both `.c` and `.asm` exports of `viceroy_unpacked`/`viceroy_overlays`,
only reads/compares turned up, no `MOV [0x8de8], ...`). What *is* resolved:
the message's per-tick gate (independent of season) had the exact same
"building exists" vs. "someone is staffed" bug as the five goods above.
`colony_prod_colony_hammers`'s `out_lumber_use` output (already
sol-bonus-independent, computed by summing `colony_prod_hammers_worker` over
staffed Carpenter's Shop/Lumber Mill workers) is the same "this tick's real
tier-scaled demand" signal `colony_craft_demand_mask` provides for the craft
recipes, just from the hammers pipeline instead of `colony_craft.c` (lumber
isn't a craft recipe, so it can't reuse that mask directly). `turn.c` Phase K
now gates `Need lumber.` on `out_lumber_use > 0` instead of
`turn_building_name_has(..., "Carpenter")`, without touching the Autumn-freeze
gate on hammers production itself. Regression: `test_turn.c` new unstaffed-
Carpenter's-Shop block (0 lumber, no queued project → no "lumber" in status).

## Deep — O / P AI dump-sell + spoilage msgs

### O — AI dump-sell + trim (57806–57873)

For cargo `1..15` with surplus `stock − warehouse_cap > 0`:

- **Non-human** owner: sell surplus via `291f_0a2e`; muskets (0xf) convert
  batches of 50 into Europe block counter; horses (8) add full surplus to
  Europe horses word; credit Europe gold ledgers `+0xbc/+0x7c/+0x2a`.
- Then spoilage: if surplus &gt; reserved cap `aiStack_e4[c]`, subtract excess
  (lose `local_74` if ≥2) or clamp stock to cap.

Linux: `colonies_apply_warehouse_spoilage` (trim); `europe_ai_colony_dump_sell`
**Done** thin (`nation_horses[]` / `nation_musket_batches[]` on EuropeScreen).

### P — spoilage msgs (57874–57931)

- If any cargo spoiled (`local_72`): single-cargo subst vs multi `0xeb4`;
  warehouse_level&gt;1 bumps string; dialog `09dc`.
- Human century-crossing stock msgs `0xebb` / tip `0xec7` once
  (`5387|2`) when stock crosses 100s.

**Port:** msgs **Done** thin (human Europe status line); multi-type → "goods"
phrasing **Done** thin; full dialogs PARKED; century tip **Done** thin
(cross 100s → status + `tut3.nr6` once-latch); trim **Done**.

## Linux correspondence

| DOS | Linux |
|-----|-------|
| A fort fire (nested) | Separate SETUP — [`coastal_fort_fire.md`](coastal_fort_fire.md) |
| A bells/FF | [`nation_ticks_bells_ff.md`](nation_ticks_bells_ff.md) |
| B cargo apply | `turn_produce_one_colony` + craft/yield — **Partial** (AI food `difficulty>>1` **Done**) |
| B Custom House | `europe_custom_house_autosell` **Done** |
| C/D SoL flags | `colony_prod_refresh_sol_flags` (one-step sol_50/100); food starve reshape |
| C SoL accumulators | `colony_prod_tick_rebel_accumulators` **Done** (shrink÷64 + pop×2 + bells; WoI crown half-negative) |
| D SoL chrome | Latch + decade `@REBELMAJORITY`/`@REBELUNANIMOUS`/`@TORY*`/`@SONSUP`/`@SONSDOWN` **Done** thin (`turn_emit_sol_phase_d_chrome`); report_rebel_majorities / report_sons gates; VGA PARKED |
| D Tory inefficient | `@INEFFICIENT`/`@EFFICIENT` **Done** thin (`turn_emit_inefficient_gov_chrome`); latch on Col1 bit3 as in DOS, so it survives save/load; `report_inefficient_government` gate |
| L hammers | `colony_prod_colony_hammers` + complete; `@BUILT` chrome **Done** thin |
| O spoilage trim | `colonies_apply_warehouse_spoilage` |
| O AI dump-sell | `europe_ai_colony_dump_sell` **Done** thin |
| F–H education | `turn_produce_one_colony` **Done** thin |
| Horse breed | `turn_produce_one_colony` **Done** thin (Stable cap) |
| P spoilage msgs | Europe status + century tip + `tut3.nr6` latch **Done** thin; `@SPOIL1`–`4` section matrix **Done** thin; century `@CARGOREADY0`–`2` (at-cap → 1/2) **Done** thin |
| K | hammers/tools/raw Europe status + `0x5384` gates **Done** thin; `@LUMBER`/`@ORE`/`@TOOLS` + craft `@COTTON`/`@TOBACCO`/`@CANESUGAR`/`@FURS` chrome **Done** thin; tools-short `@NEEDTOOLS0` / partial `@NEEDTOOLS` **Done** thin |
| I birth / J starve-kill | **Done** (`turn_produce_one_colony`); birth `@NEWCOLONIST` chrome **Done** thin; `@FOODLOW` when `8e32≠0` (prod&lt;consume) and stock &lt; `8e32×4` and not starving (`8e5a==0`) **Done**; first-latch `@FOOD1`/`@FOOD2` (autumn→FOOD2) **Done** thin; kill when still short and food was 0 at turn start (`local_6c`) `@STARVE1`/`@STARVE2` (autumn→STARVE2) **Done** thin; last colonist `@VANISH` + abandon **Done** thin; **Easy-difficulty no-kill mercy ported 2026-08-19** (decomp ~57641–57647: `difficulty<2` → never kills before year 1520, then `dos_rng_range(0, 2-difficulty)!=0` cancels the kill — 2/3 odds at Discoverer, 1/2 at Explorer; `turn_produce_one_colony`/`turn_run_colony_production` now take a `ColonizeDosRng* rng` param) |
| Q depletion | wrap + `MAP_LAYER2_SUPPRESS` **Done**; `@DEPLETION` chrome **Done** thin |
