# `FUN_15eb_28c8` — colonist work-plot job scoring (deep map, RE complete)

Deep annotation per `original_sources_annotated/README.md` Layer D. Split out
of [`ai_port_plan.md`](../../docs/ai_port_plan.md) **T1.17** (itself split
from **T1.14**'s decoding of the `DS:0x2f76+4` labor/travel-penalty term,
which is this function's own owner — see
[`terrain_yields.md`](../../docs/terrain_yields.md)'s `+0x4` row). Already in
`FUNCTION_CATALOG.md` ("Score/assign best work-plot job for colonist (trial
1068+18ec+06d2)", 254 lines) but never linked to any `.c`/`.md` file before
this doc — that's the gap this closes.

**Status: every accessor in the call graph now has a real identity (2026-08-22
session). No C port exists yet** — writing one needs a golden fixture to
verify against (none currently exercises colonist auto-job-assignment) and
is real, separate, risk-bearing work; not attempted blind. This doc is the
prerequisite the project's own convention expects before that port, not the
port itself.

Source: `viceroy_unpacked.c:12908-13161` (canonical flattened export, clean,
no `WARNING:` markers).

```c
undefined2 __cdecl16far FUN_15eb_28c8(undefined2 param_1, int param_2)
```

- `param_1` = colonist slot index (0-based, within the *active* colony,
  `*(int*)0x8542` — the same "bound active colony" global
  `move_scoring_land.md`'s `FUN_15eb_002c` targets elsewhere in this
  overlay).
- `param_2` = job-search mode: `< 0` full 9-job search (auto-assign "best
  job"); `>= 0` score only that one job index (a "what would job N score
  here" probe, used by human UI hover/preview per the AI-vs-human branch
  below); `< -1` a "just vacate, don't rescore" signal (see tail).
- Returns `1` if a (possibly unchanged) assignment stands, `0` if the
  colonist was vacated with nothing better found. Side outputs at
  `DS:0x8dbe` (winning tile's `18ec` yield) / `0x8dc0` (winning score).

## Call sites

Two, both already resolved: `FUN_15eb_2ea0` (`viceroy_unpacked.c:13162`,
iterates every colonist slot not already claimed by a mission-adjacent
allocation, calling `28c8(slot, 0xffff)` — i.e. always full-search mode,
`0xffff` as `int` is `-1`) and at least one direct call from the colony
UI/assignment flow not yet traced (out of scope for this pass — the
`2ea0` batch-driver is the one that matters for "auto-assign colonists").

## Accessor identities (all resolved this session)

| DOS symbol | Identity | Source |
|---|---|---|
| `*(int*)0x8542` | Active/bound colony pointer | Established project-wide (`move_scoring_land.md` etc.) |
| `colony+0x1f` | Colonist count | Established project-wide |
| `colony+slot+0x20` | Colonist slot's work-plot **job** (0-8 outdoor, `0xd`+ building jobs, `0xff`=unassigned) | `FUN_15eb_0e18`, this session |
| `colony+slot+0x40` | Colonist slot's **profession** type (falls through to `unit+0x315b` — `col1_save.h`'s already-named `profession` field — when the slot index exceeds the in-colony count) | `FUN_15eb_0e52`, this session |
| `colony+0x9a+job*2` | Current headcount doing `job` (9 `int`s, jobs 0-8) | Read directly in `28c8`'s own body |
| `colony+0x70+plot` | Work-plot occupant byte (colonist slot id or `0xff`) | `FUN_15eb_06d2`'s write target; matches `colony.h`'s own `tiles[]` field conceptually (see Fidelity below) |
| `colony+0x95` | Colony level byte (`0` → pop cap 100; else `(level+1)*100`) | `FUN_15eb_0a50`, this session |
| `colony+0x1a` | Owning nation index (0-3 Euro) | Already used this way throughout this overlay |
| `DS:0x8dc6` | "current colony index" (distinct from the colony *pointer* at `0x8542`) — feeds the building-bitmask table's row | `FUN_15eb_038e` |
| `DS:0x5dca`, stride `0xca` | Per-colony building-ownership bitmask, indexed `[colony_idx][bit]` | `FUN_15eb_035e`; **already modeled in Linux** as `ColonizeColony.has_building[COLONIZE_BUILDING_TYPES_MAX]` (`colony.h`) — no raw bitmask math needs porting, just `has_building[@BUILDING_index]` |
| `FUN_15eb_039e(building_idx)` | Census: does the colony have `building_idx` (walks a stride-`0xc` linked list at `DS:-0x707a`, testing `FUN_15eb_035e` per node) | Trivially `has_building[building_idx]` in Linux — the linked-list walk is DOS's own building-inventory representation, not a separate concept |
| `FUN_15eb_0470()` | Workable-plot **tier**: `min(has-Town-Hall-lvl2-count, 2) + 2` → `{2,3,4}` | This session; see Fidelity below |
| `DS:0x329[tier]` | Tile count for that tier: `{2:0, 3:4, 4:8, [tier]:...}` — real indices used are `[2]=8`, `[3]=12`, `[4]=20` | Byte-searched `dosbox-x-dumps/find_memory`, this session, cross-calibrated against the known `DS:0x2f76` cost table (`HDR=0x88`, `DS=0x237d`) |
| `colony+0xde+i` / `colony+200+i` (`i`=0..tile_count-1) | Candidate work-plot's `(dx,dy)` offset pair from the colony center | Read directly; matches the standard 8-tile-ring-then-outward-ring Colonization layout |
| `FUN_13e4_003a(x,y)` | Tile `(x,y)` → terrain-class index (0-28) | Independently resolved in `terrain_yields.md`'s own `+0x4` dig — same call, confirmed via raw `.asm` XREF, not just this session's guess |
| `*(byte*)(terrain_class*0x10+0x2f7a)` | Labor/travel penalty for that terrain, `+0x18` forest bonus at low difficulty with no adjacent settlement | **Fully decoded**, `terrain_yields.md` `DS:0x2f76` table `+0x4` column, 29 real values |
| `FUN_15eb_18ec(x,y,&out,0)` | Field-yield composer for `(x,y)` (already ported, `colony_yield.c`) | `terrain_yields.md` |
| `FUN_1000_84fc` / relation family | Not used here — `28c8` uses `-0x6e74`/`-0x6e34`/`0x9180` continent×nation matrices instead | See next rows |
| `*(byte*)(continent+nation*0x10-0x6e74)` / `-0x6e34` | Per-continent×per-nation matrix pair (same shape as the already-resolved `23000` family) | Not independently re-derived this pass — flagged, not guessed |
| `0x9180` | Per-nation byte, compared against `-0x6e7c` term | Not independently re-derived this pass |
| `0x917c` | Euro-nation wealth rank | **Already named**, `VICEROY_DS_EURO_WEALTH_RANK` (`T1.7`) |
| `FUN_281f_07b4`/`FUN_1000_84fc(0x181f,0x8d52,param_4)`-style relation calls | Not present in this function — no diplomatic-relation term found in `28c8` itself (unlike `152e`'s formula) | — |
| `FUN_15eb_1f72()` | Colony crosses/bells **composer** — called bare (side-effecting refresh) before scoring starts | Already documented and partially ported, `nation_crosses_bells_1f72.md` |
| `FUN_15eb_0924(i)` / `FUN_15eb_0902(unit)` | Out-of-colony colonist roster fallback: `i`-th qualifying unit on the nation's own unit list (`DS:0x8d78`/`FUN_1427_004a`), filtered by `FUN_15eb_08e6` | `FUN_15eb_08e6` is the already-named `DS:0x30e` per-unit-type profession-slot gate (`T1.10`) |
| `FUN_15eb_1376(job)` | Census: colonists currently doing `job` (loops `colony+0x1f` slots via `FUN_15eb_0e18`) | Trivial once `_0e18` known |
| `FUN_15eb_06d2(x,y,job_or_0xff)` | Provisionally **write** `job_or_0xff` into the work-plot occupant byte, plus (only on a real job, only the tile's first-ever assignment) roll a hidden-resource discovery | This session — see "Side effect" below |
| `FUN_15eb_0544(nation)` | Per-nation treasury/gold accessor | `indian_trade_2820.md` |
| `FUN_15eb_0668(x,y)` | Mark tile `MAP_LAYER2_PURCHASED` | `euro_unit_act.md`'s own T1.8 XREF sweep, same address |
| `FUN_137f_0314`/`_03e4`/`_0228` | Generic tile-record find/create family | `terrain_yields.md`'s river-bit dig |
| `FUN_137f_02a0(x,y)` | `continent_id(x,y)` | Already named Linux accessor, `T1.8` |

## Structure (real control flow, not paraphrased)

1. **Setup.** `local_c=1` (default "assignment stands"). `bVar1` = is the
   acting nation human-*and*-not-forced-AI (`nation<4 && !DS:0x543f[nation]`
   — the already-known human/AI polarity byte, `0`=AI per `4528`'s
   corrected reading). `local_6 = FUN_15eb_0a50()` (population cap).
2. **If AI-controlled** (`!bVar1`): count colonists already at a "senior"
   profession tier (`DS:0x5235`-family `>1`) into `local_14`; `local_e =
   FUN_137f_02a0(colony_x,colony_y)` (continent id).
3. `iVar4 = FUN_15eb_0e18(param_1)` — colonist's *current* job (kept as the
   fallback to restore if nothing better is found). `FUN_15eb_1068(param_1,
   0x12)` — temporarily vacate the colonist (job `0x12` looks like a
   "between assignments" sentinel, not `0xff` — not independently
   confirmed). `FUN_15eb_1f72()` — refresh colony bells/crosses (side
   effect, return value unused).
4. **Gate**: proceeds to full rescoring unless several conditions hold
   (population near cap with room to spare, `param_2>=0` single-job-probe
   mode, human-controlled, or a `FUN_15eb_1376(0xd)==0`-and-thin-colony
   case, or an "unhappy" flag `colony+0x94<0`) — else short-circuits to
   `FUN_15eb_1068(param_1,0xd)` (assign the fixed job `0xd`) and returns.
5. **Main loop**: for each of `FUN_15eb_0470()`'s tile-tier count of
   candidate work-plots (see Fidelity below) not already worked by *this*
   colonist and passing terrain/ownership gates: provisionally
   `FUN_15eb_06d2`-write the colonist onto the tile, then for each of 9
   jobs (`local_34` 0-8, or just `param_2` if `>=0`):
   - `FUN_15eb_1068(param_1, local_34)` (try the job), `FUN_15eb_18ec` for
     the field yield.
   - AI mode clamps the trial yield against remaining population-cap
     headroom (`local_6 - colony[0x9a+job*2]`, floor 1).
   - Base score `= yield*8 - (abs(dx)-7... )` a dx/dy-derived distance-ish
     term (not fully decoded this pass, minor).
   - Doubles if this is the colonist's *current* job (sticky preference).
   - **AI full-search branch** (`param_2<0`): jobs 0/8 (the "generalist"
     slots) get a labor-penalty-scaled term via the `DS:0x2f76+4` table
     (`FUN_13e4_003a` terrain lookup) *and* a continent×nation matrix
     term; other jobs get a throttle-table term (`DS:0x84BC`-style, same
     family `T4.4` resolved for `2820`) plus a wealth-rank/RNG boost for
     established (non-fresh) colonies. Final score combines a distance/
     danger term (`FUN_15dc_00e0`, continent stance comparison) and a
     mid/late-game terrain-scarcity penalty.
   - Track the best `(score, job, tile)` seen.
   - `FUN_15eb_06d2(x,y,0xffff)` — undo the provisional write.
6. **Commit**: if a best candidate was found, `FUN_15eb_06d2` writes it for
   real (with the side-effecting discovery roll now live) and
   `FUN_15eb_1068` assigns the job; else restores the original job
   (`iVar4`) and returns `0`.

## Side effect: `FUN_15eb_06d2`'s first-work resource discovery

Confirmed real, not a guess: when `06d2` writes a *real* job (not the
`0xff` clear it also uses to undo provisional scoring writes) to a plot
that has never been worked before (`DS:tile-0x7262 != -1` gate, a per-tile
"already worked once" latch, set to `0xff` after), it rolls a
difficulty-scaled bonus (`FUN_281f_0d78`, compared against
`FUN_15eb_0544(nation)`'s treasury) — on a hit, credits `indian_state+5`
and calls `FUN_15eb_0596`; on a miss, `FUN_281f_0d6c` grants the colony a
flat production/gold bonus scaled by difficulty and continent stance. This
reads like Colonization's "found a hidden resource" mechanic — **not
independently confirmed against `NAMES.TXT`/fandom docs**, flagged as a
strong hypothesis. Genuinely separate from the scoring loop itself — a
faithful port could implement scoring first and this discovery roll as a
follow-up slice, matching this project's usual "port the clean part,
park the rest" discipline.

## Fidelity: the 8-tile Linux model already matches DOS's common case

`ColonizeColony` (`colony.h`) hardcodes `COLONIZE_COLONY_FIELD_TILES = 8`
(the immediate ring only). DOS's own tile count is tier-dependent via
`DS:0x329[tier]`, byte-searched this session
(`dosbox-x-dumps/find_memory`, `HDR=0x88`, `DS=0x237d`, cross-calibrated
against the known `DS:0x2f76` cost-column bytes before trusting the read):

| Tier | Condition | Tile count |
|---|---|---|
| 2 | `@BUILDING` index 10 ("Town Hall", 2nd of 3 tiers) count `== 0` — the default, Town-Hall-level-1 case | **8** |
| 3 | Town Hall level-2 count `== 1` | 12 |
| 4 | Town Hall level-2 count `>= 2` (capped) | 20 |

**Tier 2's tile count is exactly 8** — identical to `colony.h`'s existing
constant. So the current Linux data model isn't an approximation of DOS's
typical colony, it *is* DOS's typical colony, byte-for-byte; only colonies
that have built Town Hall level 2+ need more storage than `tiles[8]`
provides. A first port can faithfully cover the tier-2 case entirely
within the existing struct; outer-ring support for upgraded Town Halls is
a clean, separately-scoped future item (needs `colony.h` layout work, a
save-format-adjacent change, worth a deliberate decision rather than a
silent side effect of this port).

## Tile-offset table — resolved (2026-08-22, same byte-search)

`colony+0xde+i` / `colony+200+i` are NOT per-colony save fields despite the
`colony+...` framing in the raw decompile — they're two small **static**
`DS`-relative arrays (`DS:0xde` dx, `DS:0xc8` dy), confirmed via the same
`dosbox-x-dumps/find_memory` byte-search (`HDR=0x88`, `DS=0x237d`):

```
idx:  0    1   2   3    4    5    6    7    8   9   10   11
dx:  -1    0   1   0   -1   -1    1    1   -2   0    2    0
dy:   0    1   0  -1   -1    1    1   -1    0   2    0   -2

idx:  12   13   14   15   16   17   18   19
dx:   -2   -2    2    2   -1    1   -1    1
dy:   -1    1   -1    1   -2   -2    2    2
```

**Correction 2026-09-06d — the two rows are swapped above.** `DS:0xc8` is
**dx** and `DS:0xde` is **dy**, not the other way round. Every consumer agrees:
`FUN_4d56_1b3a` (81723-81733), `FUN_2f2b_0b97` (47537-47539),
`FUN_15eb`'s build gate (13677-13678) and `FUN_4cc6_03f8` (81040-81041) all
compute `x = colony[0] + DS:0xc8[i]`, `y = colony[1] + DS:0xde[i]`. With the
swap applied the dumped bytes are exactly `ai.c`'s `k_ring_dx`/`k_ring_dy` in
`ai_indian_village_threat`. The mislabel was harmless so far (the two tables
are the same multiset, and both consumers only needed the *set*), but it
matters the moment an index is paired with `colony.tiles[i]` — as `1b3a`
phase 3 does. This paragraph's "DOS's own enumeration order differs from
colony.h's tiles[] convention" claim is therefore **unverified**: 28c8 and
1b3a index `colony+0x70+i` with the same `i` they index these tables with,
so DOS's order *is* `tiles[]`'s order by construction. `colony.h`'s
`k_field_dx/dy` is golden-validated (the colony production goldens), so if the
two really disagree it is this dump's index order that is off — do not use the
byte listing above to map a `tiles[]` index onto a direction without
re-deriving it.

Indices 0-7 = the immediate 8-ring (any order; independently confirmed as
the *set* of the standard N/NE/E/SE/S/SW/W/NW neighbors — DOS's own
enumeration order differs from `colony.h`'s `tiles[]` convention but
covers the identical 8 tiles, order only matters for score-tie-breaking).
Indices 8-19 extend to the full 5×5-square-minus-corners (21 tiles minus
center = 20), exactly matching tier 4's `DS:0x329[4]=20` count from above
— a clean, independent geometric confirmation of both tables at once (not
just plausible-looking, provably the classic Colonization work-radius
layout). `iVar5=dx+2`/`iVar12=dy+2` bias these into `0..4` array indices
for the 5×5 runtime caches (`-0x7210`/`-0x7262`) `28c8` also reads — those
two caches are themselves confirmed **runtime state** (filled once per
colony view by `FUN_15eb_268e`/`FUN_15eb_23f2`, `FUNCTION_CATALOG.md` rows
immediately before `28c8`), not static data, so there's nothing further to
byte-search there.

## Remaining genuinely open terms (sharpened, not resolved)

Everything below is a real, specific, bounded unknown — not a "needs more
general RE" placeholder:

- **`bVar2`'s population gate** (`*(int*)(colony+0x9a) <= FUN_15eb_0a50()`,
  unindexed) — reads oddly next to the *indexed* `colony+job*2+0x9a`
  per-job-headcount usage a few lines later (index 0 would double as "this
  scalar"). Two readings not distinguished: (a) `colony+0x9a` is total
  population, coincidentally aliasing job-0's slot, or (b) it's genuinely
  job-0 (Food) headcount specifically. Doesn't affect the 9-job scoring
  math itself, only whether the AI full-search branch's "colony below
  population cap" shortcut fires — a minor gating nuance, not core
  fidelity.
- **`-0x71ce`/`-0x71a6`**, word arrays indexed `job*2` — gate whether
  `local_38` starts at `local_4` vs `local_4+2`, and whether `local_1e`
  doubles. Purpose not hypothesized yet (need more context around their
  other use sites, not searched this pass).
- ~~`-0x6e74`/`-0x6e34`/`0x9180`/`-0x6e7c`~~ **resolved by cross-reference,
  same session — not the `23000` family, the already-fully-solved G-table
  one.** `-0x6e74` = `unit_value_sum_by_continent` (Euro-side "development
  level," stride `0x10`, `DS:0x918c`) and `-0x6e34` = its Indian-side
  sibling (per tribe-type×continent Brave combat-value sum, `DS:0x91cc`),
  both already fully traced in `euro_g_table_0a60.md` with an existing
  Linux accessor (`ai_euro_continent_stance_at`, per this project's own
  earlier `T1.9` note). `0x9180` = `land_combat_totals[4]`, per-nation Euro
  combat-value total (`save_format_map.md` offset 32). `-0x6e7c` =
  `tribe_data_9184`, per-tribe Brave combat-value sum (`save_format_map.md`
  offset 565). So `local_3c` isn't a mystery term — it's a real **military
  development/danger comparison** (candidate tile's continent Euro
  strength vs. Indian strength, then own-nation vs. tribe strength),
  reusing the SAME G-table machinery `euro_g_table_0a60.md` already fully
  decoded and this project already ported (`ai_euro_continent_stance_at`).
  A port can likely reuse that existing accessor directly rather than
  re-deriving the raw byte comparisons.
- **`local_24`'s exact relationship to `local_34`** — `FUN_15eb_18ec`'s
  3rd arg is an out-param (`&local_24`), and later code indexes by
  `local_24` (not `local_34`) for the per-job headcount/throttle lookups.
  Linux's own `colony_yield_for_tile(map,x,y,field_job)` (already-ported
  equivalent) takes an explicit job and has no comparable out-param — a
  port substituting `local_34` for `local_24` throughout is a plausible,
  reasonable approximation (the two are very likely always equal in
  practice) but not independently confirmed.
- Job-6 (Ore) bonus's building indices **cross-validate the whole model**:
  `FUN_15eb_039e(0x28)` + `FUN_15eb_039e(3)*2` = `has_building` counts for
  `@BUILDING` rows 40 ("Blacksmith's Shop") and 3 ("Armory") — both real
  ore-consuming industries, a sensible AI preference (mine more ore when
  you can use it) that independently confirms job index 6 = Ore in this
  function's own 0-8 numbering (matches `terrain_yields.md`'s
  Food/Sugar/Tobacco/Cotton/Furs/Lumber/Ore/Silver/Fish order exactly).

## C port status

**2026-08-22, resumed session — a reference-only structural port now
exists.** `ai_euro_28c8_colonist_job_score_structural` (`ai_euro.c`, next
to `ai_euro_colony_free_farmer_field`/`ai_euro_try_farmer_field_assign`)
scores all 8 field jobs across the tier-2 8-tile ring using the terms this
doc lists as resolved: field yield (`colony_yield_for_tile`), the `+0x4`
labor/travel penalty (new `map_dos_terr_labor_penalty_byte` accessor added
to `map.c`/`map.h` this pass — the byte table above was already decoded,
just not yet in code), the population-cap headroom clamp
(`warehouse_level`), and current-job sticky doubling. It deliberately
leaves every term in "Remaining genuinely open terms" above unimplemented
rather than guessed. **Not wired** — address-taken only in
`ai_euro_colony_goals`, matching `ai_euro_5d04_nation_planning_structural`'s
own reference-only convention; still no golden fixture to verify the
weighted formula against, so this stays undeployed. Doesn't cover
building-job assignment (DOS job `>=0xd`) or the human single-job-probe/
early-shortcut gate.

## Not attempted this pass

- The discovery-roll's exact odds/payout formula (`FUN_281f_0d78`/
  `_0d6c`/`_0596`'s own internals) — parked as a separate slice per above.
- Cross-checking whether `ai_euro.c`'s existing hand-tuned colonist-job
  heuristic should ever be replaced by a real port of this — a Tier 3
  "flip the switch" decision, not attempted or recommended here.
- Building a golden fixture for colonist auto-assignment, which would let
  the structural port above actually be wired and verified.
