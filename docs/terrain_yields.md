# Terrain field yields (colony area / town commons)

Reference for what a map square can produce when worked as a field job (or, for the colony center, as the automatic **town commons** harvest). Sons of Liberty / Tory modifiers are summarized under [Field composition order](#field-composition-order-fun_15eb_18ec); full sentiment catalog: [sons_of_liberty.md](sons_of_liberty.md).

## Sources

| Source | Role |
|--------|------|
| [`COLONIZE/NAMES.TXT`](../COLONIZE/NAMES.TXT) `@UNFORESTED` / `@FORESTED` / `@OTHER` / `@RESOURCE` / `@JOB` | **Authoritative** base yield grids and resource **catalog values** |
| `FUN_15eb_17fa` / `FUN_15eb_18ec` (`viceroy_unpacked.c` ~11717–11991) | **Authoritative** special-resource effect, expert/convert, lumber ×2, plow/road/river stacking, SoL ± on fields |
| [`COLONIZE/Colonization.pdf`](../COLONIZE/Colonization.pdf) | Qualitative rules (commons dual-produce, Prime Timber exception, plow/road/river intent). Printed Terrain Chart often **≠** `NAMES` — prefer `NAMES` + decomp |
| MAPEDIT resource class table (`mapedit_resource_type_by_terrain` in [`map.c`](../src/core/map.c)) | Which special resource **type** a terrain class may roll |
| Col1 fixtures / [`test_colony_yield.c`](../tests/unit/test_colony_yield.c) | Town-commons dual-produce — **empirically calibrated**, peel pending |

Pedia / map indices: cleared land **0–7**, forests **8–23** (type = `index & 7`), arctic / ocean / sea lane **24–26**, mountains / hills as classes **27 / 28**.

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

A **0** in a yield cell means that job produces nothing on that terrain.

---

## Base yields — unforested land (`@UNFORESTED`)

Cleared / never-forested tiles. Columns: Farmer … Fisherman. Synced from `NAMES.TXT`.

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

Forest type *N* clears permanently to unforested type *N* (Boreal→Tundra, …, Rain→Swamp).

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

**Lumberjack note:** DOS always doubles lumber after the resource effect, before plow/road/river (`local_14 == 5` → `<<1` in `FUN_15eb_18ec`, confirmed at this exact pipeline position by direct read). So Mixed forest lumber **3** in NAMES becomes **6** in play — matching the printed Terrain Chart. **2026-08-15 fix:** [`colony_yield.c`](../src/core/colony_yield.c) now applies this ×2 at the same pipeline position.

---

## Base yields — other (`@OTHER`)

| Terrain | Pedia-ish | Food | Sugar | Tob. | Cotton | Furs | Lumber | Ore | Silver | Fish |
|---------|-----------|-----:|------:|-----:|-------:|-----:|-------:|----:|-------:|-----:|
| Arctic | 24 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Ocean | 25 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 |
| Sea Lane | 26 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 |
| Mountains | — | 0 | 0 | 0 | 0 | 0 | 0 | 4 | 1 | 0 |
| Hills | — | **1** | 0 | 0 | 0 | 0 | 0 | 4 | 0 | 0 |

**Hills food:** `NAMES.TXT` lists Farmer **1**. Prefer that. The port / some Col1 center fixtures use **2** (chart / FreeCol-shaped override) — **divergent**; document overrides as such, do not silently “fix” NAMES.

Colonies cannot be founded on mountains (manual). Fishing on ocean/sea lane needs a colony **Docks** before fishermen can work those surrounds (`FUN_15eb_18ec` zeros fish jobs &gt;7 without dock building).

---

## Special resources

### Catalog values (`NAMES.TXT` `@RESOURCE`)

These numbers are **data labels**, not the field-yield effect. Effect is `FUN_15eb_17fa` (below).

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

Class = `terrain & 0x1f`, except mountain → **27**, hill → **28**. Table value `0` remaps to type **6** (Minerals); `-1` = never.

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

Hardcoded **(resource, job) → bonus**. Return **−1** means **double** the current yield; otherwise **add** the value. Not `max(@RESOURCE, base)`.

| Resource | Job | Effect |
|----------|-----|--------|
| Oasis (1) | Farmer (0) | +2 |
| Wheat (2) | Farmer (0) | +2 |
| Game (9) | Farmer (0) | +2 |
| Game (9) | Fur Trapper (4) | +2 |
| Beaver (8) | Fur Trapper (4) | +3 |
| Prime Cotton (3) | Cotton Planter (3) | **double** |
| Prime Tobacco (4) | Tobacco Planter (2) | **double** |
| Prime Sugar (5) | Sugar Planter (1) | **double** |
| Prime Timber (10) | Lumberjack (5) | +2 |
| Minerals (6) | Ore Miner (6) | +3 |
| Ore Deposit (13) | Ore Miner (6) | +2 |
| Minerals (6) | Silver Miner (7) | +1 |
| Silver Deposit (12) | Silver Miner (7) | +2 |
| Fishery (7) | Fisherman (8) | +3 |

When the worker’s **skill matches** the job, an additive resource bonus is itself **×2** before being added (`18ec` ~11909–11912). Double (`−1`) path does `yield <<= 1` **unconditionally** (not gated on expert match — confirmed from a direct read of `FUN_15eb_18ec`, not just this table).

**2026-08-15 fix:** `colony_yield_for_tile` now uses `colony_yield_resource_effect(resource, field_job)`, a byte-exact port of `FUN_15eb_17fa`'s full if-chain (verified by direct read, not just this table) — every `(resource, job)` pair from the table above, including the ones a single `resource → job` map structurally couldn't express (Game(9) pairs with **both** Farmer +2 and Fur Trapper +2; the old `colony_yield_resource_job()` could only return one job per resource, so Farmer-on-Game got **zero** bonus, not just a wrong number). **Still not done:** the expert-doubles-additive-bonus rule above — needs profession context, which `colony_yield_for_tile` doesn't have (used by AI/job-suggestion callers with no specific worker); would need threading into `colony_yield_for_worker` instead, deferred pending the SoL-ordering work below since they're entangled (the same expert-match flag drives the plow/road/river unit size too — see below).

Examples (free colonist, no other mods):

| Tile | Port (current) | DOS |
|------|-------------:|----:|
| Plains + Prime Cotton (cotton) | 6 | 4 (double of 2) |
| Hills + Minerals (ore) | 4 | 4+3 = 7 |
| Ocean + Fishery (fish) | 5 | 3+3 = 6 |
| Mountain + Silver Deposit | 12 | 1+2 = 3 (then other mods) |

---

## Field composition order (`FUN_15eb_18ec`)

Simplified pipeline for one surround work-plot (authoritative order —
**2026-08-15: re-verified by a direct read of `FUN_15eb_18ec`'s full body**,
`viceroy_unpacked.c:11771-11992`; the function turned out to be
substantially more involved than this "simplified" list, with two pieces
not previously documented at all — see below):

1. Base from terrain×job table at DS `0x2f7b` (NAMES-loaded).
2. **Fisherman only** (job > 7): a distance/enclosure modifier via
   `FUN_15eb_173e`/`FUN_15eb_16fe` — counts how many of the 8 neighbors of
   the fished tile are themselves Ocean/Sea Lane. The decompiled C shows a
   6-way `local_4 < 8/6/4/3/1` cascade, but **2026-08-15: verified against
   the raw asm (not decompiler noise this time) that 3 of those 6 branches
   are genuinely unreachable in the DOS binary itself** — each is only
   entered after already proving `count < 6`, then immediately re-tested
   against `count >= 6`, impossible. Real effective ladder: `count >= 8` →
   **−2** (fully open ocean); `count >= 6` → **−1**; else → **+1** (sheltered
   coastal tile). **Fixed:** ported as `colony_yield_fisherman_distance_mod`
   in `colony_yield.c` (the 3 dead branches correctly omitted — porting
   unreachable code changes nothing observable). Regression: new
   `test_turn.c` synthetic-map check (open-ocean vs. sheltered, exact
   3-point swing).

   **The `count >= 8` (`−2`) case is itself unreachable for any tile a real
   colonist can be assigned to fish**, raised and confirmed on a user
   question: a colony can never be founded on ocean, and every one of a
   colony's 8 field-work positions has the colony center as one of *its own*
   8 neighbors (verified by direct enumeration — for any offset `(dx,dy)`
   with `dx,dy ∈ {-1,0,1}` not both 0, `(-dx,-dy)` is itself a valid offset,
   so the center is always among the 8 neighbors of every surrounding tile).
   So the center — guaranteed non-ocean — always counts against the full-8
   threshold. This looks like a genuine DOS quirk (the distance function is
   generic over any two tiles, not aware it's only ever called this way) —
   kept exactly as ported, matching this project's north star of DOS
   fidelity over invented "fixes"; not a reachability bug worth guarding
   against, since it can never fire.
3. Early terrain/FA tweaks (incl. fur-specific road/river nibbles, job==4 only).
4. Clamp negative → 0.
5. **SoL / Tory mod** if `mod > 0`: `yield += mod`. The mod itself can be
   forced to **0 outright** (not just a different divisor) when
   `byte[colony+0x1a] >= 4` or the per-nation `0x543f` table byte is nonzero
   — the *same* gate `FUN_15eb_1d4c` uses only to pick the divisor (10 vs
   `10-difficulty`), but here it zeroes the whole mod instead — **field
   yields skip the Tory penalty entirely for AI colonies**, while
   manufacturing/bells/crosses/hammers do not. **2026-08-15 fix:**
   `colony_prod_sol_bonus_field` (new function) now applies this for both
   field-yield call sites; `colony_prod_sol_bonus` (building contexts) is
   unchanged. See [sons_of_liberty.md](sons_of_liberty.md).
6. **Expert:** food/fish → `yield += 2` (and re-add the positive SoL mod a
   second time — confirmed at this exact spot: `if (food/fish) { yield += 2;
   if (mod > 0) yield += mod; }`); other jobs → `yield <<= 1`.
7. **Special resource** via `17fa` (double or add; expert doubles additive).
8. **Lumberjack:** `yield <<= 1`.
9. **Plow / road / river** stack (below).
10. **2026-08-15 fix:** Fish without Docks → 0; Henry Hudson FF check (`FUN_15eb_3960(nation, 8)`)
    doubles Fur Trapper yield **in this same function** — i.e. DOS applies
    Hudson here, not as a separate post-hoc step the way the port's
    `turn.c`/`colony_preview.c` currently do (same final number when there's
    only one field worker per tick, not independently verified to diverge
    otherwise).
11. **Convert** +1 on allowed jobs.
12. If SoL/Tory `mod < 0`: `yield += mod` (floor at 0).

### Expert / convert

| Worker | Rule | Port |
|--------|------|------|
| Matching expert, food or fish | **+2** (not ×2), plus the SoL mod re-add above | **2026-08-15 fix** — flat +2 wired; the SoL mod re-add is still not (needs the SoL-threading work first, deferred) |
| Matching expert, other field jobs | **×2** | Wired |
| Mismatched skill | Free-colonist yield | Wired |
| Indian convert | **+1** if job ∈ {0,1,2,3,4} or job &gt; 7 (fisherman); **not** lumber/ore/silver | **2026-08-15 fix, re-verified against raw asm on user question** — `colony_yield_for_worker` gates on the exact whitelist. First pass read this from the `.c` decompile only (`FUN_15eb_18ec` ~11974-11979); re-checked byte-for-byte against `viceroy_unpacked.asm` (`~15eb:1cd6-1d06`) after a user asked whether converts really lose the bonus on lumber/ore/silver specifically — confirmed exact, not decompiler noise: `CMP local_14,0/2/3/1/4` (`JZ` to the `INC`) then `CMP local_14,8; JL` (skip unless ≥8, i.e. Fisherman only from that point up.) Lumber(5)/Ore(6)/Silver(7) are the only 3 field jobs with no matching branch. Real DOS behavior, not a port bug, whatever the manual/community memory says — the *manufacturing* (building) side of converts (1/3 the free-colonist rate, confirmed separately in `manufacturing_worker_calc_1d4c.md`) is unaffected by this and already matches the "converts work buildings poorly" expectation exactly. |

### Plow / road / river stacking

Unit size `u = 2` if (matching expert and not food/fish) **or** lumberjack; else `u = 1`. Bonuses **add** (they stack):

| Condition | Jobs | Add |
|-----------|------|----:|
| Farmer path (job 0) | Farmer | +`u` (plow-shaped; see decomp ~11950) |
| `layer2 & 0x0a` (FA / plow-road mask) | job &gt; 3 (fur, lumber, ore, silver) | +`u` |
| `layer2 & 0x40` (river) | job &lt; 4 (food + cash crops) | +`u` |
| Terrain river bit `0x40` | (adds again) | +`u` |
| Major river (`0x40|0x80`) when only one unit so far | | +`u` again |

**Port:** plow +1 on crops; road +1 on fur/lumber/ore/silver; river magnitudes FreeCol-shaped; **road and river do not stack** (max of one) — **divergent** from DOS stacking.

**2026-08-15: one blocker resolved, a bigger one found underneath.**

*Resolved:* whether the port's `map_tile_has_road()` (`improve &
MAP_IMPROVE_ROAD`) is the same concept as DOS's `layer2` FA-road bit —
**yes.** `col1_bridge.c`'s save loader sets both from the *same* Col1 mask
bit (`m & 0x08`) into two mirrored port arrays: "`Road in mask is 0x08; DOS
AI scoring also checks layer2 bit 0x40 for roads — mirror improve road into
that bit`" (`col1_bridge.c:611-624`). They're synchronized by construction,
so `map_tile_has_road()` is safe to use for whichever DOS check needs it.

*Still blocking, and bigger than thought:* re-reading `FUN_15eb_18ec`'s
stacking block precisely (`viceroy_unpacked.c:11944-11966`), DOS checks
river through **two separate signals**, not one:
```
if (FUN_137f_0142(tile) & 0x40 && field_job < 4):     term += u   ; "layer2-ish" river, food/crops only
if (terrain_byte & 0x40):                              term += u   ; a DIFFERENT river bit, ANY job,
  if (terrain_byte & 0x80 && term == u): term += u again           ; major-river doubles ONLY if this
                                                                     was the sole contributor so far
```
The port's `map_tile_has_river()`/`map_tile_has_major_river()` read the
**terrain byte** only (confirmed — `map.c:1271-1283`, matches the second
signal above). There is no port equivalent of the first signal
(`FUN_137f_0142() & 0x40`, gated to food/crop jobs specifically) — it may be
a genuinely separate river/irrigation concept the port doesn't track at all,
not just a naming mismatch like the road bit turned out to be. Also still
open: `u`'s size depends on the expert-match flag (same profession-context
entanglement as the resource/SoL work above). Both need resolving before a
correct stacking implementation — a half-fix risks silently dropping a whole
term, not just using the wrong unit size, so still not attempted.

Silver on mountains without a deposit / road can be forced to 0 or 1 (`18ec` ~11925–11938).

---

## Town commons (colony center tile)

Manual: settlement square **always produces some food and one other commodity**; specials apply **except Prime Timber**.

**Status:** formula below is **Col1-fixture calibrated** (not a NAMES row; full DOS commons composer peel pending). Keep as empirical until re-peeled.

**Food base** (before plow / river / specials):

- Forested: `@UNFORESTED` Farmer of cleared parent (`pedia & 7`) **+ 2**
- Cleared / hills: Farmer **+ 2** (with port Hills Farmer 2 → commons food 4)

**Secondary:** terrain-fixed job; amount = `NAMES[job] + 1`.

Then plow (+1 food on cleared), river (same magnitudes as port field table), Oasis/Wheat/Game **+2** food, matching secondary special **+2**.

| Tile (fixtures) | Food | Secondary |
|-----------------|-----:|-----------|
| Scrub Forest | 3 | 3 furs |
| Hills | 4 | 5 ore |
| Broadleaf Forest | 4 | 3 furs |
| Prairie + minor river | 5 | 5 cotton |
| Broadleaf + Game | 6 | 5 furs |

---

## Manual Terrain Chart vs `NAMES.TXT`

Printed chart often shows post-modifier lumber (e.g. Plains forested lumber **6** = NAMES Mixed **3** × DOS lumberjack ×2). Prefer **NAMES + `18ec`**, not the chart, for implementation.

| | Manual Plains forested | `NAMES` Mixed | After DOS lumber ×2 |
|--|------------------------|---------------|---------------------|
| Food | 3 | 2 | 2 |
| Lumber | 6 | 3 | **6** |
| Cotton | 1 | 1 | 1 |

---

## Port status (field yields)

| Rule | DOS | [`colony_yield.c`](../src/core/colony_yield.c) |
|------|-----|-----------------------------------------------|
| Base NAMES grids | Wired | Wired (Hills food override **2**) |
| Resource effect `17fa` | Additive / double, per (resource,job) pair | **2026-08-15 fix** — `colony_yield_resource_effect()`, byte-exact table (Game/etc. now correctly pair with multiple jobs) |
| Lumberjack ×2 | Yes | **2026-08-15 fix** — wired at the correct pipeline position (after resource, before plow/road/river) |
| Expert food/fish +2 | Yes (+ SoL mod re-add) | **2026-08-15 fix** — flat +2 wired (SoL mod re-add still needs SoL-threading) |
| Convert job whitelist | Yes | **2026-08-15 fix** — exact whitelist gate |
| Plow/road/river stack | Add | Max(road, river) — **divergent**; entangled with expert-flag unit sizing, not attempted |
| Fisherman distance modifier | Yes (`FUN_15eb_173e`) | **2026-08-15 fix** — real 3-case ladder confirmed from raw asm, ported |
| Fisherman needs Docks | Yes, zeroes yield outright | **2026-08-15 fix** — `colony_yield_for_worker` gained a `has_docks` param, threaded from every production/preview/badge caller (`turn.c`, `colony_preview.c`, `colony_screen.c` area overlay + jobs popup) |
| SoL mod: AI zero-out | Zeroed outright for AI (strong, cross-validated hypothesis — see manufacturing_worker_calc_1d4c.md) | **2026-08-15 fix** — `colony_prod_sol_bonus_field` (new function), wired into both field-yield call sites (`turn.c`, `colony_preview.c`); building contexts (craft/bells/crosses/hammers) keep the shared `colony_prod_sol_bonus`, unaffected |
| Town commons | Peel pending | Fixture formula |

---

## Explicitly excluded here

- Full SoL / Tory production math — [sons_of_liberty.md](sons_of_liberty.md)
- Building manufacturing — [building_production.md](building_production.md)
- Combat / movement columns from the Terrain Chart
