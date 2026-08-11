# Terrain field yields (colony area / town commons)

Reference for what a map square can produce when worked as a field job (or, for the colony center, as the automatic **town commons** harvest). Sons of Liberty / Tory modifiers are summarized under [Field composition order](#field-composition-order-fun_15eb_18ec); full sentiment catalog: [sons_of_liberty.md](sons_of_liberty.md).

## Sources

| Source | Role |
|--------|------|
| [`COLONIZE/NAMES.TXT`](../COLONIZE/NAMES.TXT) `@UNFORESTED` / `@FORESTED` / `@OTHER` / `@RESOURCE` / `@JOB` | **Authoritative** base yield grids and resource **catalog values** |
| `FUN_15eb_17fa` / `FUN_15eb_18ec` (`viceroy_unpacked.c` ~11717–11991) | **Authoritative** special-resource effect, expert/convert, lumber ×2, plow/road/river stacking, SoL ± on fields |
| [`COLONIZE/Colonization.pdf`](../COLONIZE/Colonization.pdf) | Qualitative rules (commons dual-produce, Prime Timber exception, plow/road/river intent). Printed Terrain Chart often **≠** `NAMES` — prefer `NAMES` + decomp |
| MAPEDIT resource class table (`mapedit_resource_type_by_terrain` in [`map.c`](../src/core/map.c)) | Which special resource **type** a terrain class may roll |
| Col1 fixtures / [`test_colony_yield.c`](../tests/smoke/test_colony_yield.c) | Town-commons dual-produce — **empirically calibrated**, peel pending |

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

**Lumberjack note:** DOS always doubles lumber after other field mods (`local_14 == 5` → `<<1` in `FUN_15eb_18ec`). So Mixed forest lumber **3** in NAMES becomes **6** in play — matching the printed Terrain Chart. **Port:** [`colony_yield.c`](../src/core/colony_yield.c) does **not** apply this ×2 yet (**divergent**).

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

When the worker’s **skill matches** the job, an additive resource bonus is itself **×2** before being added (`18ec` ~11909–11912). Double (`−1`) path does `yield <<= 1`.

**Port:** still uses absolute `@RESOURCE` / base+2 (**divergent**).

Examples (free colonist, no other mods):

| Tile | Port (stale) | DOS |
|------|-------------:|----:|
| Plains + Prime Cotton (cotton) | 6 | 4 (double of 2) |
| Hills + Minerals (ore) | 4 | 4+3 = 7 |
| Ocean + Fishery (fish) | 5 | 3+3 = 6 |
| Mountain + Silver Deposit | 12 | 1+2 = 3 (then other mods) |

---

## Field composition order (`FUN_15eb_18ec`)

Simplified pipeline for one surround work-plot (authoritative order):

1. Base from terrain×job table at DS `0x2f7b` (NAMES-loaded).
2. Early terrain/FA tweaks (incl. fur-specific road/river nibbles).
3. Clamp negative → 0.
4. **SoL / Tory mod** if `mod > 0`: `yield += mod` (see [sons_of_liberty.md](sons_of_liberty.md)).
5. **Expert:** food/fish → `yield += 2` (and re-add positive SoL); other jobs → `yield <<= 1`.
6. **Special resource** via `17fa` (double or add; expert doubles additive).
7. **Lumberjack:** `yield <<= 1`.
8. **Plow / road / river** stack (below).
9. Fish without Docks → 0; Hudson FF may double furs.
10. **Convert** +1 on allowed jobs.
11. If SoL/Tory `mod < 0`: `yield += mod` (floor at 0).

### Expert / convert

| Worker | Rule | Port |
|--------|------|------|
| Matching expert, food or fish | **+2** (not ×2) | **Divergent** (uses ×2 for all) |
| Matching expert, other field jobs | **×2** | Wired |
| Mismatched skill | Free-colonist yield | Wired |
| Indian convert | **+1** if job ∈ {0,1,2,3,4} or job &gt; 7 (fisherman); **not** lumber/ore/silver | **Divergent** (always +1) |

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
| Resource effect `17fa` | Additive / double | Absolute `@RESOURCE` / +2 — **divergent** |
| Lumberjack ×2 | Yes | **Missing** |
| Expert food/fish +2 | Yes | Uses ×2 — **divergent** |
| Convert job whitelist | Yes | Always +1 — **divergent** |
| Plow/road/river stack | Add | Max(road, river) — **divergent** |
| Town commons | Peel pending | Fixture formula |

---

## Explicitly excluded here

- Full SoL / Tory production math — [sons_of_liberty.md](sons_of_liberty.md)
- Building manufacturing — [building_production.md](building_production.md)
- Combat / movement columns from the Terrain Chart
