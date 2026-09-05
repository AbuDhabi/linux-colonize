# Terrain field yields (colony area / town commons)

Reference for what a map square can produce when worked as a field job, and
the colony-center **town commons** auto-harvest. SoL/Tory modifiers are
summarized under [Field composition order](#field-composition-order-fun_15eb_18ec);
full sentiment catalog: [sons_of_liberty.md](sons_of_liberty.md).
(Compressed 2026-09-05; derivation narratives in git history.)

## Sources

| Source | Role |
|--------|------|
| [`COLONIZE/NAMES.TXT`](../COLONIZE/NAMES.TXT) `@UNFORESTED`/`@FORESTED`/`@OTHER`/`@RESOURCE`/`@JOB` | **Authoritative** base yield grids and resource catalog values |
| `FUN_15eb_17fa` / `FUN_15eb_18ec` (`viceroy_unpacked.c` ~11717–11991) | **Authoritative** resource effect, expert/convert, lumber ×2, improvement stack, SoL on fields |
| `FUN_15eb_1f72` (~12474) | Town-commons composer, read directly |
| [`COLONIZE/Colonization.pdf`](../COLONIZE/Colonization.pdf) | Qualitative rules; printed Terrain Chart often ≠ `NAMES` — prefer `NAMES` + decomp |
| MAPEDIT resource class table (`mapedit_resource_type_by_terrain`, [`map.c`](../src/core/map.c)) | Which resource type a terrain may roll |
| [`test_colony_yield.c`](../tests/unit/test_colony_yield.c), `golden_colony_prod01/02/03` | Regressions; prod02/03 are real DOS captures |

Pedia / map indices: cleared land **0–7**, forests **8–23** (type =
`index & 7`), arctic / ocean / sea lane **24–26**, mountains / hills **27/28**.

---

## DS:`0x2f76` terrain-class record — full 16-byte layout (2026-08-21)

One table unifies everything previously chased piecemeal: 32 stride-`0x10`
records at `DS:0x2f76`, real classes `0..28` (`29..31` noisy, untrusted).
Recovered by byte-pattern search over `dosbox-x-dumps/*` (blob formula
`file_off = 0x88 + seg*16 + off` — HDR is `0x88`, not 8); rows 0..28
byte-identical across all 19 saves, so genuine static data. No live
session was needed — check existing dumps first.

| Offset | Content |
|-------:|---------|
| `+0x0` | Move cost (`k_map_dos_terr_cost`) — wired |
| `+0x1` | Founding score (`k_map_dos_terr_found_score`); doubles as combat terrain-defense bonus (`combat_strength.c`) — wired |
| `+0x2` | Pioneer clear/plow work-turns threshold — wired |
| `+0x3` | Colony-site desirability per neighbor tile, used only inside `FUN_521d_20e6`'s inline candidate-site picker (3 near-duplicate blocks). Values idx 0–28: `2,2,4,4,4,4,2,2,3,1,3,3,3,3,1,1,3,1,3,3,3,3,1,1,0,3,0,2,2`. **Not wired** — that picker is dead code in this port (same conclusion as port_plan T1.2). |
| `+0x4` | Labor/travel penalty subtracted from a colonist's candidate work-plot score in **`FUN_15eb_28c8`** (work-plot auto-assign, unported). Values: `12,12,10,15,15,21,12,14,18,12,18,12,12,12,12,7,18,12,18,12,12,12,12,7,0,9,9,24,24`. **Traps:** the earlier `FUN_129f_0008` attribution was a false collision (that body is an RLE decoder); the real owner was found via the `.asm`'s own XREF (`caseD_10`, physically in the `15eb` overlay), not a guess. `DS:0x8dd2` there is a proximity flag, not Indian-specific. Not wired. |
| `+0x5`..`+0xd` | **The `NAMES.TXT` field-yield grid itself** — `+0x5+job` (Farmer..Fisherman). `18ec`/`1f72` compute `job + terrain*0x10 + 0x2f7b`, and `0x2f7b == 0x2f76+5`; bytes match this file's tables cell-for-cell. Resolves the old "base yield table not in the decompile" mystery. |
| `+0x8` | Also `k_map_dos_terr_lumber_reward` (Pioneer clear reward) — same byte as the Cotton column; two unrelated consumers, not a bug. |
| `+0xe` | Type/graphic-looking index: unforested `idx+3`, forest `11+(idx&7)` — except pedia 15 (Rain, primary) reads 10 instead of 18 (idx 23 gets 18; quirk on that exact row, like the Rain 1/1 table bug below). Meaning unidentified; not wired. |
| `+0xf` | Zero for every real class in every save — padding. |

---

## Field jobs → cargo

Order matches `NAMES.TXT` yield columns and `@JOB`:

| Col | Job | Cargo |
|----:|-----|-------|
| 0 | Farmer | Food |
| 1 | Sugar Planter | Sugar |
| 2 | Tobacco Planter | Tobacco |
| 3 | Cotton Planter | Cotton |
| 4 | Fur Trapper | Furs |
| 5 | Lumberjack | Lumber |
| 6 | Ore Miner | Ore |
| 7 | Silver Miner | Silver |
| 8 | Fisherman | Food (fish) |

A **0** cell means that job produces nothing on that terrain.

---

## Base yields — unforested land (`@UNFORESTED`)

| Terrain | Idx | Food | Sugar | Tob. | Cotton | Furs | Lumber | Ore | Silver | Fish |
|---------|----:|-----:|------:|-----:|-------:|-----:|-------:|----:|-------:|-----:|
| Tundra | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 2 | 0 | 0 |
| Desert | 1 | 1 | 0 | 0 | 1 | 0 | 0 | 2 | 0 | 0 |
| Plains | 2 | 4 | 0 | 0 | 2 | 0 | 0 | 1 | 0 | 0 |
| Prairie | 3 | 2 | 0 | 0 | 3 | 0 | 0 | 0 | 0 | 0 |
| Grassland | 4 | 2 | 0 | 3 | 0 | 0 | 0 | 0 | 0 | 0 |
| Savannah | 5 | 3 | 3 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Marsh | 6 | 2 | 0 | 2 | 0 | 0 | 0 | 2 | 0 | 0 |
| Swamp | 7 | 2 | 2 | 0 | 0 | 0 | 0 | 2 | 0 | 0 |

---

## Base yields — forested (`@FORESTED`)

Forest type *N* clears permanently to unforested type *N*.

| Forest | Cleared becomes | Food | Sugar | Tob. | Cotton | Furs | Lumber | Ore | Silver | Fish |
|--------|-----------------|-----:|------:|-----:|-------:|-----:|-------:|----:|-------:|-----:|
| Boreal | Tundra | 1 | 0 | 0 | 0 | 3 | 2 | 1 | 0 | 0 |
| Scrub | Desert | 1 | 0 | 0 | 1 | 2 | 1 | 1 | 0 | 0 |
| Mixed | Plains | 2 | 0 | 0 | 1 | 3 | 3 | 0 | 0 | 0 |
| Broadleaf | Prairie | 1 | 0 | 0 | 1 | 2 | 2 | 0 | 0 | 0 |
| Conifer | Grassland | 1 | 0 | 1 | 0 | 2 | 3 | 0 | 0 | 0 |
| Tropical | Savannah | 2 | 1 | 0 | 0 | 2 | 2 | 0 | 0 | 0 |
| Wetland | Marsh | 1 | 0 | 1 | 0 | 2 | 2 | 1 | 0 | 0 |
| Rain | Swamp | 1 | 1 | 0 | 0 | 1 | 2 | 1 | 0 | 0 |

**Lumberjack:** DOS always doubles lumber after the resource effect, before
plow/road/river (`local_14 == 5` → `<<1` in `18ec`). Mixed lumber 3 in
NAMES = 6 in play, matching the printed chart. Wired in
[`colony_yield.c`](../src/core/colony_yield.c) at that pipeline position.

---

## Base yields — other (`@OTHER`)

| Terrain | Pedia | Food | Sugar | Tob. | Cotton | Furs | Lumber | Ore | Silver | Fish |
|---------|-------|-----:|------:|-----:|-------:|-----:|-------:|----:|-------:|-----:|
| Arctic | 24 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Ocean | 25 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 |
| Sea Lane | 26 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 |
| Mountains | 27 | 0 | 0 | 0 | 0 | 0 | 0 | 4 | 1 | 0 |
| Hills | 28 | 1 | 0 | 0 | 0 | 0 | 0 | 4 | 0 | 0 |

**Hills food is NAMES.TXT's 1** (settled 2026-09-03). The old
"player-confirmed 2" observation was base 1 + the unconditional farmer `+u`
from the improvement stack, which the table had wrongly absorbed before the
stack was ported literally; pinned by `farming/case3`'s expert Farmer on a
bare Hill = 4 (1 + expert 2 + farmer 1).

Colonies cannot be founded on mountains. Fishing on ocean/sea lane requires
colony **Docks** (`18ec` zeroes fish jobs without it).

---

## Special resources

### Catalog values (`NAMES.TXT` `@RESOURCE`)

Data labels only — the field effect is `FUN_15eb_17fa` below.

| Type | Name | Value |
|-----:|------|------:|
| 0 | Depleted Mine | 6 |
| 1 | Oasis | 3 |
| 2 | Wheat (Prime Food) | 4 |
| 3 | Prime Cotton | 6 |
| 4 | Prime Tobacco | 6 |
| 5 | Prime Sugar | 7 |
| 6 | Minerals | 4 |
| 7 | Fishery | 5 |
| 8 | Beaver | 6 |
| 9 | Game | 6 |
| 10 | Prime Timber | 6 |
| 11 | Prime Timber (duplicate row) | 6 |
| 12 | Silver Deposit | 12 |
| 13 | Ore Deposit | 6 |

### Allowed terrain class → resource type (MAPEDIT)

Class = `terrain & 0x1f`, except mountain → 27, hill → 28. Table value 0
remaps to Minerals (6); `-1` = never.

| Class | Terrain | Allowed resource |
|------:|---------|------------------|
| 0 | Tundra | Minerals (6) |
| 1 | Desert | Oasis (1) |
| 2 | Plains | Wheat (2) |
| 3 | Prairie | Prime Cotton (3) |
| 4 | Grassland | Prime Tobacco (4) |
| 5 | Savannah | Prime Sugar (5) |
| 6 | Marsh | Minerals (6) |
| 7 | Swamp | Minerals (6) |
| 8 | Boreal | Game (9) |
| 9 | Scrub | Oasis (1) |
| 10 | Mixed | Beaver (8) |
| 11 | Broadleaf | Game (9) |
| 12 | Conifer | Prime Timber (10) |
| 13 | Tropical | Prime Timber (10) |
| 14 | Wetland | Minerals (6) |
| 15 | Rain | Minerals (6) |
| 16–23 | (forest alt) | same as `index & 7` |
| 24 | Arctic | none |
| 25 | Ocean | Fishery (7) |
| 26 | Sea Lane | none |
| 27 | Mountains | Silver Deposit (12) |
| 28 | Hills | Ore Deposit (13) |

### Effect on a matching job (`FUN_15eb_17fa`)

Hardcoded **(resource, job) → bonus** if-chain — a resource can pair with
more than one job. Return −1 = **double** current yield; otherwise **add**.
Not `max(@RESOURCE, base)`.

| Resource | Job | Effect |
|----------|-----|--------|
| Oasis (1) | Farmer (0) | +2 |
| Wheat (2) | Farmer (0) | +2 |
| Game (9) | Farmer (0) | +2 |
| Game (9) | Fur Trapper (4) | +2 |
| Beaver (8) | Fur Trapper (4) | +3 (asm-confirmed 2026-09-03; an old port table had 2) |
| Prime Cotton (3) | Cotton Planter (3) | **double** |
| Prime Tobacco (4) | Tobacco Planter (2) | **double** |
| Prime Sugar (5) | Sugar Planter (1) | **double** |
| Prime Timber (10) | Lumberjack (5) | +2 |
| Minerals (6) | Ore Miner (6) | +3 |
| Ore Deposit (13) | Ore Miner (6) | +2 |
| Minerals (6) | Silver Miner (7) | +1 |
| Silver Deposit (12) | Silver Miner (7) | +2 |
| Fishery (7) | Fisherman (8) | +3 |

A matching-skill expert **doubles an additive bonus** before it's added
(`18ec` ~11909). The double (−1) path is `yield <<= 1` unconditionally (not
expert-gated). Ported byte-exact as `colony_yield_resource_effect()` — the
old single `resource → job` map structurally couldn't express Game pairing
with both Farmer and Fur Trapper (Farmer-on-Game got zero). The
expert-doubles-additive half needs worker context and lives in
`colony_yield_for_worker`.

---

## Field composition order (`FUN_15eb_18ec`)

Authoritative pipeline for one surround work-plot (verified against the
full body, ~11771-11992):

1. Base from terrain×job table (`DS:0x2f7b`, NAMES-loaded).
2. **Fisherman only:** coastal distance mod — count ocean/sea-lane among
   the fished tile's 8 neighbors: `>= 8` → −2, `>= 6` → −1, else +1.
   (The decompile shows 6 branches; 3 are genuinely unreachable in the
   binary — verified in raw asm, correctly omitted from the port.
   The −2 case is itself unreachable for any assignable tile: the colony
   center is always among a work-plot's 8 neighbors and is never ocean —
   a DOS quirk, kept as-is.) `colony_yield_fisherman_distance_mod`;
   applies to experts too (post-×2, flat).
3. Early terrain/FA tweaks (incl. Fur Trapper's own pre-multiplier road
   +1 / river +1 (+2 major) adds, job 4 only, ~11840-11850 — wired).
4. Clamp negative → 0.
5. **SoL/Tory mod** if > 0: `yield += mod`. The mod is zeroed outright for
   AI colonies (`byte[colony+0x1a] >= 4` or per-nation `0x543f` byte
   nonzero) — fields skip the Tory penalty for AI while
   manufacturing/bells/crosses do not. `colony_prod_sol_bonus_field`.
6. **Expert:** food/fish → `+2` flat **and re-add the positive SoL mod a
   second time**; other jobs → `yield <<= 1`. Order matters: SoL folds in
   *before* the doubling (player-confirmed: Expert Ore Miner
   Hills+road+SoL1 = 12; the old flat-external-add gave 11).
7. **Special resource** via `17fa` (double, or add ×2 for expert).
8. **Lumberjack:** `<<= 1`.
9. **Plow/road/river** stack (below).
10. Fish without Docks → 0. **Henry Hudson** (FF 8) doubles Fur Trapper
    inside this same function (port applies it post-hoc in
    `turn.c`/`colony_preview.c` — same result with one worker/tile).
11. **Convert** +1 on allowed jobs.
12. If SoL/Tory mod < 0: `yield += mod`, floor 0.

**Zero-base gate (2026-09-03, `farming/case4`):** the positive-SoL fold,
the expert branch, and the improvement stack are all gated on a nonzero
table base — expert Farmer on Mountains = **0** food (the ungated port
invented 3). Same capture confirmed Beaver grants no Farmer bonus.

### SoL fold: `local_1c` == `colony_prod_sol_bonus_field()`

Decoded 2026-08-24 (~11866-11889):

```
local_1e = FUN_15eb_0274();                                    // colony SoL% (= colony_prod_sol_percent, incl. Bolivar +20 human)
local_e  = (pop * (100 - local_1e) + 50) / 100;                // Tory share (pop = colony+0x1f)
divisor  = human ? (10 - difficulty) : 10;  AI → local_e = 0;  // same gate as manufacturing_worker_calc_1d4c.md
local_1c = -(local_e / divisor)
         + (colony+0x1c & 0x04 ? 1 : 0)                        // SOL_50 latch
         + (colony+0x1c & 0x02 ? 1 : 0);                       // SOL_100 latch
```

Field-for-field identical to `colony_prod_sol_bonus_field`'s return. The
expert food/fish re-add (step 6) re-adds this *same variable* — a real DOS
double-count. **Trap fixed:** the port's re-add used to reconstruct the
value from `colony_flags` latch bits only, which undercounts whenever the
Tory term is nonzero; it now re-adds `sol_bonus` itself.

### Expert / convert

| Worker | Rule |
|--------|------|
| Matching expert, food/fish | **+2 flat** (asm-confirmed, not ×2) + SoL re-add |
| Matching expert, other field jobs | ×2 |
| Mismatched skill | Free-colonist yield |
| Indian convert | **+1** only on jobs {0,1,2,3,4} or Fisherman — **not** lumber/ore/silver. Asm-verified byte-for-byte (`15eb:1cd6-1d06`) on user challenge: real DOS behavior, whatever community memory says. Building-side converts (1/3 rate) are a separate, matching rule. |

### Plow / road / river stack (literal port, 2026-09-03)

Unit size `u = 2` if (matching non-food/fish expert) or Lumberjack
(matching or not); else 1. All terms **add**:

| Condition | Jobs | Add |
|-----------|------|----:|
| Farmer (job 0) | Farmer | +`u`, **unconditional** — any skill, no plow gate (asm 15eb:1c32-1c40 is a bare `job==0` test) |
| Runtime mask `0x0a` (road) | job > 3 | +`u` |
| Runtime bit `0x40` = **plow** (resolved 2026-09-03) | job < 4 | +`u` |
| Terrain river bit `0x40` | any job | +`u` |
| Major river (terrain `0x80`) when the stack so far == `u` | | +`u` again |

Wired verbatim in `colony_yield_pipeline` (verified against asm
15eb:1c16-1c9c), replacing older curve-fit shapes. Consequences:

- Every old "unconditional farmer +1" sighting and the Hills "2" were this
  farmer term; Hills base is NAMES' 1.
- **Farmer major river does NOT double** (`farming/case1+2`): the farmer
  `+u` lands first, so the stack is already `2u` when the river add
  arrives and the major clause never fires. Fisherman (no farmer/plow
  term) genuinely doubles on major (Lake+major = 6 = base 3 + coastal +1
  + river 2).
- The runtime `0x40` bit is **plow**, closing the old "unidentified
  runtime-array bit / two river signals" question (the earlier conflation
  of the port's own `MAP_LAYER2_FA_ROAD == 0x40` constant with DOS's bit
  was a mistake, since retracted). The `0x0a` road mask is independently
  confirmed by the AI movement-cost subsystem.
- Road magnitude uses the same per-job bucket as river: furs/lumber base
  2, ore/silver/crops 1 (pinned by Hudson-owning player data: Fur Trapper
  Mixed+road+SoL2, free 14 / expert 28 — both solve exactly only with
  road base 2 × Hudson ×2).

Silver on mountains without a deposit/road can be forced to 0 or 1
(`18ec` ~11925–11938).

---

## Town commons (colony center tile)

Auto-worked: no colonist, no expert/convert, no docks gate. Composer
`FUN_15eb_1f72` read directly; its base table is `DS:0x2f76+5` (same as
field yields — confirmed byte-identical, so the formula is exact, not
approximated). One table bug found on the way: `k_forested`'s Rain row
had Food/Sugar 2/2; NAMES (and Paramaribo's real capture) say **1/1**.

### Food

```
food = class_base(pedia)         (0: pedia 24; 1: pedia 1/9/17; 2: pedia 8-23 or 27/28; else 3)
     + 2/1 at Discoverer/Explorer difficulty
     + 1   if plowed (runtime 0x40 bit)
     + 2   if resource is Oasis(1)/Wheat(2)/Game(9)   (Prime Timber excluded)
     + 1   per SOL_50 / SOL_100 latch bit
floor 0
```

**No river term on commons food** (2026-09-03): the food block reads only
the plow bit; the terrain river value feeds the secondary alone. (The old
phantom river term and the missing farmer field term had been cancelling
each other in the goldens — a curve-fit trap.) Plow is +1, not +2
(player-pinned via Guadeloupe/Vlissingen). Class base is a 4-way pedia
split, not flat (Recife: Savannah class 3 → food 3). SoL term = the two
latch bits, **not** the general signed `sol_bonus`.

### Secondary commodity

DOS runs a **max-over-jobs search** (2026-09-03), not a fixed per-terrain
job table:

```
per job in jobs 1-7, skipping Lumberjack (never Farmer/Fisherman):
  score = table[pedia][job]
        + resource effect at its real 17fa magnitude (e.g. Minerals+Ore +3)
        (×2 instead on a DOUBLE match: Prime Cotton/Tobacco/Sugar)
secondary = max score (strictly greater wins; earlier job keeps ties)
          + 1/2 if river (minor/major)
          + 1 per SOL_50 / SOL_100 latch bit
          + 1 at Discoverer difficulty only
floor 0
```

**No plow term, no flat road** (asm-confirmed absent — both were
pre-composer-read port guesses, removed). Prime Timber never applies.
**Trap:** the commons resource read must ignore the settlement bit
(`map_resource_type_for_yield`) — the center always carries it, and
`map_resource_type_at` returned −1 there, silently erasing every commons
resource. Player-pinned: Swamp center + Minerals makes **5 ore** (Ore 2 +
Minerals 3 beats Sugar 2), where the fixed table produced 2 sugar.

No-resource outcome per terrain (what the search picks unshifted):

| Pedia | Terrain | Secondary job |
|------:|---------|----------------|
| 0 | Tundra | Ore Miner |
| 1 | Desert | Ore Miner |
| 2 | Plains | Cotton Planter |
| 3 | Prairie | Cotton Planter |
| 4 | Grassland | Tobacco Planter |
| 5 | Savannah | Sugar Planter |
| 6 | Marsh | Tobacco Planter |
| 7 | Swamp | Sugar Planter |
| 8-23 | Forest (`&7`) | Sugar Planter if Rain (`&7==7`), else Fur Trapper |
| 24-26 | Arctic/Ocean/Sea Lane | none |
| 27 | Mountains | Silver Miner (uninhabitable, moot) |
| 28 | Hills | Ore Miner |

### Worked examples (`unit_colony_yield` fixtures, flags/sol = 0)

| Tile | Base | Modifiers | Secondary |
|------|-----:|-----------|-----------:|
| Scrub Forest | 2 (Fur) | — | 2 furs |
| Hills | 4 (Ore) | +2 resource | 6 ore |
| Prairie + minor river | 3 (Cotton) | +1 river | 4 cotton |
| Broadleaf + Game | 2 (Fur) | +2 Game | 4 furs |
| Hills, SOL_50 | 4 (Ore) | +2 resource, +1 latch | 7 ore |
| Hills, both latches | 4 (Ore) | +2 resource, +2 latch | 8 ore |

Real-DOS confirmations with zero free parameters: New Holland Savannah
`3+2latch=5` sugar, Guadeloupe plowed Swamp `2+2=4` (plow contributes
nothing to secondary), Curacao Broadleaf `2+2=4` furs (commons is its only
furs source), Paramaribo Rain `1+2=3` sugar (after the Rain 1/1 fix).

---

## Manual Terrain Chart vs `NAMES.TXT`

The printed chart often shows post-modifier lumber (Plains forested 6 =
NAMES Mixed 3 × lumberjack ×2). Prefer **NAMES + `18ec`** for
implementation.

---

## Port status (field yields)

| Rule | Status in [`colony_yield.c`](../src/core/colony_yield.c) |
|------|----------------------------------------------------------|
| Base NAMES grids | Wired (Hills food = NAMES' 1; the farmer stack term explains the old "2") |
| Resource effect `17fa` | `colony_yield_resource_effect()`, byte-exact incl. multi-job pairs; Beaver+Fur +3 |
| Lumberjack ×2 | Wired at the correct pipeline position |
| Expert food/fish +2 + SoL re-add | Wired; re-add uses `sol_bonus` itself; SoL folds before expert doubling |
| Convert job whitelist | Exact gate |
| Plow/road/river stack | Wired verbatim (2026-09-03) — no divergence left |
| Fisherman distance modifier | Real 3-case ladder from raw asm |
| Fisherman needs Docks | `has_docks` param threaded through all callers |
| SoL AI zero-out | `colony_prod_sol_bonus_field`, both field call sites |
| Town commons | Exact (`golden_colony_prod01/02/03` green) |
| Zero-base gate | Wired (expert on 0-base terrain = 0) |

---

## Explicitly excluded here

- Full SoL / Tory production math — [sons_of_liberty.md](sons_of_liberty.md)
- Building manufacturing — [building_production.md](building_production.md)
- Combat / movement columns from the Terrain Chart
