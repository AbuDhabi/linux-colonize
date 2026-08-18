# Terrain field yields (colony area / town commons)

Reference for what a map square can produce when worked as a field job (or, for the colony center, as the automatic **town commons** harvest). Sons of Liberty / Tory modifiers are summarized under [Field composition order](#field-composition-order-fun_15eb_18ec); full sentiment catalog: [sons_of_liberty.md](sons_of_liberty.md).

## Sources

| Source | Role |
|--------|------|
| [`COLONIZE/NAMES.TXT`](../COLONIZE/NAMES.TXT) `@UNFORESTED` / `@FORESTED` / `@OTHER` / `@RESOURCE` / `@JOB` | **Authoritative** base yield grids and resource **catalog values** |
| `FUN_15eb_17fa` / `FUN_15eb_18ec` (`viceroy_unpacked.c` ~11717–11991) | **Authoritative** special-resource effect, expert/convert, lumber ×2, plow/road/river stacking, SoL ± on fields |
| [`COLONIZE/Colonization.pdf`](../COLONIZE/Colonization.pdf) | Qualitative rules (commons dual-produce, Prime Timber exception, plow/road/river intent). Printed Terrain Chart often **≠** `NAMES` — prefer `NAMES` + decomp |
| MAPEDIT resource class table (`mapedit_resource_type_by_terrain` in [`map.c`](../src/core/map.c)) | Which special resource **type** a terrain class may roll |
| Col1 fixtures / [`test_colony_yield.c`](../tests/unit/test_colony_yield.c), `FUN_15eb_1f72` (`viceroy_unpacked.c` ~12474) | Town-commons dual-produce — composer logic read directly, but its base-yield data table (`0x2f7b`) isn't in the decompile; port formula is a **golden-verified approximation**, see [Town commons](#town-commons-colony-center-tile) |

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
| Hills | — | **2** | 0 | 0 | 0 | 0 | 0 | 4 | 0 | 0 |

**Hills food: player-confirmed 2026-08-15 (Viceroy) — 2, not NAMES.TXT's 1.**
A non-specialist Farmer on a worked (non-colony-center) Hills tile produces
**2** food. `NAMES.TXT` `@OTHER` lists Farmer **1** for Hills, but real
gameplay matches the port's existing override (`colony_yield.c` comment:
"Terrain Chart / FreeCol / live Col1"), not the raw NAMES row. Whatever
mechanism bumps this in DOS (a hard-coded case, a NAMES-independent Farmer
path, or something the `FUN_15eb_17fa`/`18ec` peel hasn't traced) is still
not identified at the byte level — but the *output* is no longer in doubt,
so this is resolved player-side even though the "why NAMES says 1" question
stays open as low-priority RE backlog. No code change needed (port already
had it right); this just confirms the port's override over the raw data
table instead of flagging it as an unverified divergence.

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
   **2026-08-15 fix — steps 5+6 now wired in this order, port-side:**
   player-confirmed on Viceroy: Expert Ore Miner, Hills+road+sentiment(+1) =
   12 ore; Free Colonist, same tile = 6. The port used to compute
   base+road/river first, double the whole thing for a matching expert, then
   add the SoL/Tory mod *flat, externally, after* `colony_yield_for_worker`
   returned (in `turn.c`/`colony_preview.c`) — giving free=6 (right, by
   coincidence) but expert=(4+1)×2+1=11, not 12, because the mod never got
   swept up by the expert doubling the way DOS's step-5-before-step-6 order
   demands. Fixed: `colony_yield_for_worker` (moved to `colony_yield.c`,
   see below) now takes `sol_bonus` as a signed parameter and folds it in at
   the correct pipeline position itself; `turn.c`/`colony_preview.c` no
   longer add it externally. Regression: `test_colony_yield.c` (Hills+road,
   sol_bonus=1, free=6/expert=12, matched the hand-derivation on first run).
7. **Special resource** via `17fa` (double or add; expert doubles additive).
   **2026-08-15:** the expert-doubles-additive half is wired (same refactor
   as steps 5+6). Player later confirmed the same shape directly: on a
   fur-boosting-resource tile, expert:free was also exactly ×2 (consistent
   with, though not solely attributable to, this rule — see the Fur Trapper
   writeup below, where the *non*-resource tile's gap was the real finding).
   Directly unit-tested regardless (`test_colony_yield.c`, Game(9)+Farmer:
   free=3, expert=7 — `1+2` vs `1+2+2×2`), since the player data alone
   couldn't isolate this term from Henry Hudson's own ×2 on that tile.
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
| Matching expert, food or fish | **+2** (not ×2), plus the SoL mod re-add above | **Contradicted, not wired** — a 2026-08-15 pass believed this landed, but `colony_yield_pipeline` ships plain ×2 for every field expert today (its own comment there claims the +2 variant "regressed golden_colony_prod01"). Asm-confirmed real 2026-08-18 (`FUN_15eb_18ec` ~11890-11899, `local_16`/`local_18` branch — see "Field Farmer/Fisherman expert formula" below): +2 is right, and the "SoL mod re-add" is real too, not a stale claim. Not reimplemented yet — the re-add term (`local_1c`) isn't simply this port's existing `sol_bonus`, see below. |
| Matching expert, other field jobs | **×2** | Wired |
| Mismatched skill | Free-colonist yield | Wired |
| Indian convert | **+1** if job ∈ {0,1,2,3,4} or job &gt; 7 (fisherman); **not** lumber/ore/silver | **2026-08-15 fix, re-verified against raw asm on user question** — `colony_yield_for_worker` gates on the exact whitelist. First pass read this from the `.c` decompile only (`FUN_15eb_18ec` ~11974-11979); re-checked byte-for-byte against `viceroy_unpacked.asm` (`~15eb:1cd6-1d06`) after a user asked whether converts really lose the bonus on lumber/ore/silver specifically — confirmed exact, not decompiler noise: `CMP local_14,0/2/3/1/4` (`JZ` to the `INC`) then `CMP local_14,8; JL` (skip unless ≥8, i.e. Fisherman only from that point up.) Lumber(5)/Ore(6)/Silver(7) are the only 3 field jobs with no matching branch. Real DOS behavior, not a port bug, whatever the manual/community memory says — the *manufacturing* (building) side of converts (1/3 the free-colonist rate, confirmed separately in `manufacturing_worker_calc_1d4c.md`) is unaffected by this and already matches the "converts work buildings poorly" expectation exactly. |

### Field Farmer/Fisherman expert formula

Read `FUN_15eb_18ec` (`viceroy_unpacked.c` ~11771, the real per-tile field-yield composer) directly, chasing New Amsterdam/New Holland's horse-breeding drift in `golden_colony_prod02` (both traced to their expert Fishermen). Confirmed three real mechanics; two are now wired.

**Wired 2026-08-18: non-expert Farmer gets an unconditional `+1`** (`if (local_14 == 0) local_12 = local_c;`, ~11950) that does **not** stack with plow — a plowed and an unplowed non-expert Farmer tile land on the same total (`colony_yield_pipeline`). Two attempts at the stacking direction (bolted on top of the existing "plow OR river" term, then rebuilt as a separate third term alongside plow) both fit the three plow-less real tiles that first surfaced this (`golden_colony_prod02`'s Curacao/Recife mismatched-skill Farmers, Recife's Convert Farmer) but overshot by `+1` on 8-9 *other* previously-exact colonies — until checking which of those were real, un-synthesized data: `golden_colony_prod02`'s New Amsterdam (Desert, plowed) and Fort Orange (Grassland, plowed, Convert) are both real, plowed, non-expert Farmers that need exactly the *same* `+1` as the plow-less tiles, not `+2` — settling "replaces plow" over "stacks with it". The only holdout was `test_units.c`'s runtime plow-tick invariant (plowing should raise Farmer yield by `+1`), not corroborated by any real capture and contradicted by these two — updated to expect no change from plowing a non-expert Farmer tile alone. `golden_colony_prod01`'s synthetic Fort Orange/Fort Nassau/St. Louis/Port au Prince fixtures (whose Farmer tiles relied on plow for the yield this replaced) were re-derived to match.

**Wired 2026-08-18: the Fisherman coastal distance mod applies to experts too** (~11814-11838) — added post-`×2`, flat, same bucket shape (`+1` sheltered / `-1` moderately open / `-2` fully open) `colony_yield_fisherman_distance_mod` already has for non-experts, which previously skipped experts entirely. A first attempt at this alone broke four `golden_colony_prod01` fixtures; the Farmer fix above showed that class of "regression" is often a re-derivable synthetic fixture rather than real counter-evidence, so it was re-attempted rather than left reverted — and `golden_colony_prod02` (the only real, un-synthesized data for this) is now clean: New Amsterdam and New Holland (whose expert Fishermen this term was built from) both match exactly, and no other real colony regressed. `golden_colony_prod01`'s New Amsterdam/Guadeloupe/Fort Nassau/St. Louis are still off by a small amount (all real, un-synthesized coastal tiles whose actual ocean-neighbor count sits at 5, one short of the `+1`→`-1` bucket boundary, with every remaining neighbor either already ocean or another colonist's worked tile — no free lever to shift the bucket without disturbing a different cargo's tile) — left unpatched pending either a real data point pinning what these specific tiles should yield, or the missing piece below.

**Still open — Farmer/Fisherman experts get a flat `+2` on skill match, not `×2`** (`local_16`/`local_18` branch, ~11890-11899) — matching the Expert/convert table's "Contradicted, not wired" row above. That branch *also* re-adds the colony's SoL/Tory term (`local_1c`) a second time on top of the once-only addition every job already gets (~11887) — i.e. Farmer/Fisherman experts get their sentiment bonus counted twice. `local_1c` is **not** this port's `sol_bonus`/`colony_prod_sol_bonus_field` — it's built from the colony's SoL latch bits (`+1` each) *minus* a tory-defection/difficulty term. One piece of that term *is* now decoded: its divisor (`local_10` ~11871) is `10 - difficulty`, confirmed by fixing the identical divisor in the separate `colony_prod_sol_bonus` (`colony_production.c`, used by the building/manufacturing composer `FUN_15eb_1d4c`) — that function had `10 - difficulty*2`, passing its own regression test only by coincidence (a clamp hid the wrong number at Viceroy); player-confirmed via Recife (0% sentiment modifier at 20% SoL, Viceroy, no latch). Still undecoded: `local_1c`'s tory *numerator* (~11866-11868, byte offset `+0x1f` times an unidentified `FUN_15eb_0274()` result) — a separate variable from `colony_prod_sol_bonus`'s, not yet verified against a real data point. This is very likely the missing piece behind the `golden_colony_prod01` coastal-tile residuals above too: a colony-wide SoL/Tory re-add (not a per-tile terrain fix) is exactly the kind of thing that could nudge these specific totals without needing a neighbor-count workaround.

**To finish:** verify `local_1c`'s tory numerator against a real data point, then implement it together with the flat `+2` (both apply to the *same* expert-skill-match branch, and the SoL re-add was already found to matter for Fisherman even before the `+2` — see the New Amsterdam Fishery-resource case above). Re-verify against both goldens in full before keeping it; a partial or wrongly-scaled version has reproduced regressions here twice already.

### Plow / road / river stacking

Unit size `u = 2` if (matching expert and not food/fish) **or** lumberjack; else `u = 1`. Bonuses **add** (they stack):

| Condition | Jobs | Add |
|-----------|------|----:|
| Farmer path (job 0) | Farmer | +`u` (plow-shaped; see decomp ~11950) |
| `layer2 & 0x0a` (FA / plow-road mask) | job &gt; 3 (fur, lumber, ore, silver) | +`u` |
| `layer2 & 0x40` (river) | job &lt; 4 (food + cash crops) | +`u` |
| Terrain river bit `0x40` | (adds again) | +`u` |
| Major river (`0x40|0x80`) when only one unit so far | | +`u` again |

**Port:** plow +1 on crops; road +1 on fur/lumber/ore/silver (×2 unit for a
matching non-food/fish expert or any Lumberjack — **2026-08-15 fix**, see
below); river magnitudes FreeCol-shaped; **road and river do not stack**
(max of one) — this specific piece stays **divergent** from DOS's literal
multi-signal additive stack (below), but the *unit size itself* (u=1 vs u=2)
is now DOS-confirmed and wired, which was the higher-value half of this
item.

**2026-08-15 fix — expert/lumberjack road-river doubling, player-confirmed
(Viceroy):** Expert Ore Miner, Hills+road+sentiment(+1) = 12; Free
Colonist, same tile = 6. This only reproduces if the road/river bonus
itself doubles for the expert (`u=2`), not just the flat expert ×2 already
wired (that alone predicts 11, not 12 — see the "Expert" step-6 fix above
for the full arithmetic). Fixed: `colony_yield_road_or_river_bonus` takes a
`big_unit` flag, true for a matching non-food/fish expert or any
Lumberjack (matching or not — the latter per the decomp's own "or
lumberjack" clause, not itself independently player-tested, but it's the
same asm-read rule already cited in this section's `u` definition).
Regression: `test_colony_yield.c` (Ore Miner case above) plus an existing
`test_units.c` road/lumberjack test whose hardcoded `+1` expectation
predated this fix and needed updating to `+2` to match (a real, expected
behavior change, not a new bug — that test used `colony_yield_for_tile`,
profession-less, so it exercises the *unconditional* Lumberjack half of the
rule specifically).

**2026-08-15: revisited, and a claim from earlier this same day was wrong —
corrected below rather than left to mislead.**

The job>3 check (`FUN_15eb_18ec`'s `FUN_137f_0142(tile) & 0x0a`) really is
road: that exact mask (`0x0a`) is independently cited in `ai_transcription.md`
for a *different* DOS subsystem (AI movement costing) as the road test the
port's `MAP_LAYER2_FA_ROAD` bit exists to mirror, and `col1_bridge.c`'s save
loader confirms `MAP_IMPROVE_ROAD` and `MAP_LAYER2_FA_ROAD` are set from the
same Col1 bit by construction. So `map_tile_has_road()` is safe for *that*
check.

**But the earlier note above claiming this also resolved the job<4 river
check was wrong** — a same-day mistake, not a new problem. That check tests
a *different* mask (`FUN_137f_0142(tile) & 0x40`, standalone, not `0x0a`) on
the same runtime array. I'd conflated "the port's own `MAP_LAYER2_FA_ROAD`
constant happens to equal `0x40`" with "DOS's own bit `0x40` in this array
means road" — those are unrelated facts. The port chose `0x40` as its *own*
arbitrary bit value when defining `MAP_LAYER2_FA_ROAD`; nothing ties that
choice to what DOS's `FUN_137f_0142` returns for bit `0x40`. Read further
usages of `FUN_137f_0142` this pass (`viceroy_unpacked.c:6783-6935`,
`FUN_137f_0314`/`0358`/`0392`/`03e4`/`044a`) — bits `0x01`/`0x02` there read
as settlement/unit occupancy (matches `col1_bridge.c`'s own "Col1 mask low
bits carry village/capital occupancy" note), bit `0x04` matches the port's
own `MAP_LAYER2_SUPPRESS`, but nothing pins down what bit `0x40` *alone*
means in this specific array — still open.

So DOS still checks river through **two separate signals**, confirmed
unresolved (not "one road blocker down, one river blocker to go" as the
earlier version of this note claimed):
```
if (FUN_137f_0142(tile) & 0x40 && field_job < 4):     term += u   ; unknown runtime-array bit, food/crops only
if (terrain_byte & 0x40):                              term += u   ; the STATIC map-data river bit, ANY job,
  if (terrain_byte & 0x80 && term == u): term += u again           ; major-river doubles ONLY if this
                                                                     was the sole contributor so far
```
The port's `map_tile_has_river()`/`map_tile_has_major_river()` read the
**terrain byte** only (confirmed — `map.c:1271-1283`, matches the second
signal). There is no port equivalent of the first signal at all — genuinely
unresolved, not just unmapped. `u`'s size depending on the expert-match flag
is **now resolved and wired** (2026-08-15, player-confirmed — see below);
what's left open is specifically the *multi-signal additive stack itself*
(the unidentified runtime-array bit, and whether road+river should add
rather than take the max) — a half-fix there risks silently dropping a
whole term, so still not attempted.

Silver on mountains without a deposit / road can be forced to 0 or 1 (`18ec` ~11925–11938).

**2026-08-15: player-supplied gameplay data (Viceroy difficulty) — crop-job
magnitudes confirmed as-is, Fisherman bug found and fixed.** Five live
observations (free colonist, no sentiment bonus unless noted):
- Lake + major river, Fisherman: 6 food.
- Scrub Forest + major river, Farmer: 3 food.
- Conifer Forest + major river, Farmer: 3 food.
- Plains + minor river, Farmer, +1 sentiment: 7 food.
- Fully-enclosed-by-ocean tile, Fisherman: 2 food; a coastal tile usually 4,
  "sometimes 6" (previously unexplained to the player).

Checked against the port's actual formulas (not the unresolved
`FUN_137f_0142` bit theory above, which stays open):
- **Crop jobs (job&lt;4), no code change needed.** Scrub/Conifer are
  unplowable (forest), so their major-river delta (+2 on a base-1 tile) is
  river alone — matches the port's existing `colony_yield_river_bonus`
  crop bucket (base 1, major ×2) exactly. Plains+minor river and +1 sentiment
  only reconciles to the observed 7 if the tile is also plowed
  (4 base + 1 plow + 1 minor-river + 1 sentiment = 7); unplowed doesn't fit
  (would give 6). Both readings are consistent with the port's current model
  as-is — no evidence of a bug for crop jobs from this data. Whatever the
  unresolved runtime-array bit (`FUN_137f_0142 & 0x40`, job&lt;4-gated) truly
  is, it doesn't change the final crop-job output the port already produces.
- **Fisherman — real bug, fixed.** `colony_yield_river_bonus`'s switch had
  no `COLONIZE_JOB_FISHERMAN` case, silently falling to `default: return 0`
  — Fisherman got *no* river bonus at all, even though DOS's static
  terrain-river-bit check (`terrain_byte & 0x40`, the second signal above)
  applies to *any* job, fish included; only the *first*, still-unidentified
  runtime-array signal is job&lt;4-gated (and job 8 isn't &lt;4, so Fisherman
  was never eligible for that one anyway — consistent, not a new mystery).
  Lake+major-river=6 confirms it: Ocean base fish 3, +1 coastal distance mod
  (`colony_yield_fisherman_distance_mod`, few ocean neighbors around a small
  lake) = 4, +2 major river (base 1 × 2, same magnitude bucket as
  Farmer/Ore/Silver) = 6. This *also* resolves the player's separate
  "coastal usually 4, sometimes 6" observation, previously opaque: the
  "sometimes 6" tiles are simply coastal *and* major-river (4 + 2), nothing
  to do with the distance-mod cascade itself. Fixed: added a
  `COLONIZE_JOB_FISHERMAN` case (base 1, major ×2) to
  `colony_yield_river_bonus`. Since `colony_yield_for_tile` already calls
  the shared road/river helper unconditionally for every job, no other
  pipeline change was needed. Regression: `test_colony_yield.c` (Ocean +
  major river tile, `colony_yield_for_tile(..., COLONIZE_JOB_FISHERMAN)` ==
  6, matched the hand-derived value on first run).

The `FUN_137f_0142 & 0x40` runtime-array bit's actual identity is still
unresolved — this pass confirmed it doesn't change any *observable* output
the port produces for the cases tested, not what the bit itself means.

**2026-08-15: more player data (Viceroy) — expert road/river unit size
confirmed and fixed; a second, unexplained anomaly found and left open.**
Four more live observations (road present, sentiment as noted):
- Expert Ore Miner, Hills+road, +1 sentiment: 12 ore.
- Free Colonist, Hills+road, +1 sentiment: 6 ore.
- Expert Fur Trapper, Mixed Forest+road, +2 sentiment: 28 furs.
- Free Colonist, Mixed Forest+road, +2 sentiment: 14 furs.

**Ore: clean, exact, fully explained — fixed.** Hills Ore base is 4. Free:
`4 + sol(1) + road(u=1) = 6`, matches. Expert: `4 + sol(1) = 5`, `<<=1`
(expert doubling) `= 10`, `+ road(u=2 for a matching expert) = 12`, matches
*exactly* — and only with this order (SoL folds in before the expert
double, road/river unit doubles for the expert too). This is what's fixed
above (`colony_yield_pipeline`), fully validated by this data point, and
already ported.

**Fur: real gap, resource hypothesis ruled out by the player, new
hypothesis found — still not implemented, needs one more confirmation.**
Mixed Forest Furs base is 3 (`NAMES.TXT` `@FORESTED`, double-checked against
the raw file directly). Player confirmed **no special resource** was on
that tile, ruling out the `R=8` additive-resource guess floated earlier.
Player also confirmed: on a *different* tile that does carry a fur-boosting
special resource, the expert:free ratio was *also* exactly twofold —
consistent with (and a real independent player-side confirmation of) the
already-wired "expert doubles the additive resource bonus" rule (step 7
above), but doesn't bear on this specific non-resource tile's gap.

New candidate, found by re-deriving what *would* make both numbers exact
(not approximate) instead of guessing a free parameter:
1. **Road's per-job magnitude may need the same fur/lumber-vs-everything-else
   split river already has.** The port's `colony_yield_road_bonus` is a flat
   `+1` for *any* of fur/lumber/ore/silver; `colony_yield_river_bonus`
   already uses `+2` for fur/lumber specifically (`+1` for ore/silver/crop) —
   an asymmetry between road and river that was never DOS-confirmed for
   *either* function (the river split itself is commented "FreeCol
   classic/Col1", i.e. not decomp-derived) and may simply be a port
   invention. If Fur Trapper's road magnitude is *also* 2 (matching river's
   own fur/lumber bucket, not the flat 1 every other road job gets):
   `free = base(3) + sol(2) + road(u=1, base=2) = 7` — still short of 14 by
   exactly ×2.
2. **Henry Hudson** (fur trapper output +100%) would supply exactly that
   missing ×2, applied uniformly to both free and expert (it's a flat
   post-pipeline multiply in `turn.c`/`colony_preview.c`, unaffected by
   skill level) — and combining both pieces lands on the *exact* observed
   numbers, not approximately:
   ```
   free:   (base 3 + sol 2 + road[u=1,base=2]) × Hudson(2) = 7 × 2 = 14  ✓
   expert: ((base 3 + sol 2) <<=1 + road[u=2,base=2]) × Hudson(2)
         = (10 + 4) × 2 = 28  ✓
   ```
   Both equations solve *exactly*, not just closely — a much stronger fit
   than the ruled-out resource guess, and using only mechanisms already
   confirmed to exist (Hudson) or already partially wired with an untested
   asymmetry (road's per-job magnitude).

**Confirmed and fixed 2026-08-15.** Player confirmed Henry Hudson was
owned by that nation — both equations above solve exactly, not
coincidentally. Fixed: `colony_yield_road_bonus` now uses the same
per-job magnitude bucket `colony_yield_river_bonus` already had (furs/
lumber `+2`, ore/silver `+1`), replacing the old flat `+1` for every road
job. Regression: `test_colony_yield.c` (Fur Trapper, Mixed Forest+road+
sentiment(+2), no Hudson in this direct-pipeline test — Hudson is an
external post-hoc multiply in `turn.c`/`colony_preview.c`, already covered
by existing "Henry Hudson" tests — free=7/expert=14, matching the
pre-Hudson half of the derivation exactly); one pre-existing `test_units.c`
road/lumberjack test needed its hardcoded expectation updated again
(`clear+2` → `clear+4`: Lumberjack is in the fur/lumber magnitude bucket
too, so its road bonus is now `2(base) × 2(unit size) = 4`).

---

## Town commons (colony center tile)

Manual: settlement square **always produces some food and one other commodity**; specials apply **except Prime Timber**. The colony center is *auto-worked* — no colonist assigned, no expert/convert doubling, no docks gate.

**Status:** the real DOS composer (`FUN_15eb_1f72`, `viceroy_unpacked.c` ~12474) has been read directly. Its secondary-commodity logic (river + SoL latch bits, no plow, no flat road) is now wired byte-for-byte; only its per-terrain *base* yield still reuses this file's field-worker tables as a stand-in, since the composer's own base table lives in a separate binary data address (`0x2f7b`, indexed by the same pedia numbering) not present in the decompiled pseudo-C. Confirmed correct against `golden_colony_prod01`/`02` (21 real Dutch colonies, one captured DOS turn each) — including two colonies (Curacao, Paramaribo) where town commons is that colony's *only* source of its secondary cargo, so those two checks pin the formula with zero free parameters. One data-table bug this pass also turned up and fixed: `k_forested`'s Rain row had Food/Sugar = 2/2; `NAMES.TXT` says 1/1, and Paramaribo's real capture (isolating the Rum Distiller's exact consumption) independently confirmed 1/1 — the table constant was wrong, not the formula.

### Food

```
food = 2                                   (flat; 0 for pedia 25/26/27 — Ocean/Sea Lane/Mountains, uninhabitable)
     + 2   if plowed and pedia 0-7 (cleared land)
     + 1/2 if river (minor/major)
     + 2   if resource is Oasis(1) / Wheat(2) / Game(9)   (Prime Timber excluded)
     + sol_bonus                            (signed live SoL/Tory value, turn.c colony_prod_sol_bonus_field)
floor 0
```

Confirmed 2026-08-17: a per-terrain "cleared-parent Farmer + 2" food base was tried and regressed `golden_colony_prod01` (nearly every colony's food 1-4 too high); flat +2 is the one that matches. `NAMES.TXT`'s food chart does **not** apply to the town square.

**2026-08-18, still open:** `FUN_15eb_1f72` (~12506-12518, the same composer secondary above reads) shows the real base isn't flat +2 but a 4-way class split by pedia — class 2 (most forest + Hills/Mountains) equals flat +2, which is why this passed golden for the large majority of colonies (mostly forest town centers); class 3 (most cleared land, e.g. Savannah) is +1 higher; class 1 (Desert/Scrub) is -1 lower. Player-confirmed via `golden_colony_prod02`'s Recife (Savannah, class 3 → real food 3, not 2). Tried and reverted the same day, combined with the Farmer `+1` finding below — see "[Field Farmer/Fisherman expert formula](#field-farmerfisherman-expert-formula--open-2026-08-18)" for why (short version: correct for the one colony that exposed it, net regression everywhere else forest-centered).

### Secondary commodity

The job is a **fixed per-terrain choice** (below), not DOS's real per-tile max-over-all-jobs search (`FUN_15eb_1f72` loops jobs 1-7 skipping Lumberjack, scoring each via the 0x2f7b table + resource effect, and keeps the max) — the fixed choice matches that search's outcome for every terrain this project has real data for, since ties/near-ties haven't come up yet.

```
secondary = table[pedia][job]                  (same per-pedia table as field yields, see sections above)
          + 1/2 if river (minor/major)
          + 1   if COLONIZE_COLONY_FLAG_SOL_50 is set
          + 1   if COLONIZE_COLONY_FLAG_SOL_100 is set
          + 2   if resource matches job (flat types)
          × 2   if resource is a DOUBLE match (Prime Cotton/Tobacco/Sugar on their planter)
floor 0
```

**No plow term, no flat road** — asm-confirmed absent from `FUN_15eb_1f72` (2026-08-18). Earlier passes assumed "every colony founds with a road" (`+1` flat) and a plow bonus matching food's; both were unverified guesses this port carried since before the composer was read, and both are gone now. The SoL term is the two *latch* bits specifically (hysteresis flags, see [sons_of_liberty.md](sons_of_liberty.md)), not the general signed live-percentage/Tory-penalty `sol_bonus` value food uses.

Prime Timber (resource 10/11) never applies to the commons (matches the manual's stated exception).

| Pedia | Terrain | Secondary job | Table col. (base) |
|------:|---------|----------------|--------------------|
| 0 | Tundra | Ore Miner | `@UNFORESTED` Ore |
| 1 | Desert | Ore Miner | `@UNFORESTED` Ore |
| 2 | Plains | Cotton Planter | `@UNFORESTED` Cotton |
| 3 | Prairie | Cotton Planter | `@UNFORESTED` Cotton |
| 4 | Grassland | Tobacco Planter | `@UNFORESTED` Tobacco |
| 5 | Savannah | Sugar Planter | `@UNFORESTED` Sugar |
| 6 | Marsh | Tobacco Planter | `@UNFORESTED` Tobacco |
| 7 | Swamp | Sugar Planter | `@UNFORESTED` Sugar |
| 8-23 | Forest (`pedia & 7`) | Sugar Planter if Rain (`&7==7`), else Fur Trapper | `@FORESTED` Sugar / Furs |
| 24 | Arctic | — none — | — |
| 25 | Ocean | — none — | — |
| 26 | Sea Lane | — none — | — |
| 27 | Mountains | Silver Miner | `@OTHER` Silver (uninhabitable, moot) |
| 28 | Hills | Ore Miner | `@OTHER` Ore |

### Worked examples (unit_colony_yield fixtures, `colony_flags`/`sol_bonus` = 0)

| Tile | Base | Modifiers | Secondary |
|------|-----:|-----------|-----------:|
| Scrub Forest | 2 (Fur) | — | 2 furs |
| Hills | 4 (Ore) | +2 (coincidental Prime Ore hash hit) | 6 ore |
| Broadleaf Forest | 2 (Fur) | — | 2 furs |
| Prairie + minor river | 3 (Cotton) | +1 river | 4 cotton |
| Broadleaf + Game | 2 (Fur) | +2 Game | 4 furs |
| Hills, SoL latch (SOL_50 only) | 4 (Ore) | +2 resource, +1 latch | 7 ore |
| Hills, SoL latch (both bits) | 4 (Ore) | +2 resource, +2 latch | 8 ore |

### Real-DOS confirmation (Savannah vs Swamp sugar)

Player-observed (live DOS play): **New Holland** (Savannah, no plow) town center makes **5 sugar**; **Guadeloupe** (Swamp, plowed) makes **4 sugar**. Both colonies are near 100% Sons of Liberty. Colonizapedia's field chart lists Savannah and Swamp sugar as equal, so naively they "should" match — they don't:

- No plow check — Guadeloupe's plow contributes nothing to secondary in real DOS.
- Both SoL latch bits contribute: Savannah `3 + 2(latch) = 5` ✓, Swamp `2 + 2(latch) = 4` ✓.

This is now exactly how the port computes both (`golden_colony_prod01`'s synthetic Quebec/Guadeloupe/New Holland/Bahia/St. Louis fixtures were re-derived to this formula 2026-08-18, since that save's whole map is hand-reconstructed and the terrain choice for each is a free parameter — see `test_colony_prod01.c`). Two *real, unpatched* saves independently confirm the same formula with zero free parameters: **Curacao** (`golden_colony_prod02`, Broadleaf Forest, no river/road, full latch — town commons is its only furs source: `2 + 2 = 4`, matches the captured turn exactly) and **Paramaribo** (`golden_colony_prod01`, Rain Forest, full latch — town commons is its only sugar source net of its Rum Distiller's own confirmed consumption: `1 + 2 = 3`, matches once the Rain-row table bug above was fixed).

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
| Expert food/fish +2 | Yes (+ SoL mod re-add) | **2026-08-15 fix (player-confirmed, Viceroy)** — flat +2 and the SoL mod re-add both wired, plus `sol_bonus` now folds in *before* expert doubling colony-wide (`colony_yield_pipeline`), not as a flat post-hoc add |
| Convert job whitelist | Yes | **2026-08-15 fix** — exact whitelist gate |
| Plow/road/river stack | Add (multi-signal) | Max(road, river) — still **divergent** on the multi-signal additive stack itself (unresolved runtime-array bit, see below), but the **unit size** (u=1 vs u=2 for expert/Lumberjack) is now **2026-08-15 fix, player-confirmed (Viceroy)** — wired |
| Fisherman distance modifier | Yes (`FUN_15eb_173e`) | **2026-08-15 fix** — real 3-case ladder confirmed from raw asm, ported |
| Fisherman needs Docks | Yes, zeroes yield outright | **2026-08-15 fix** — `colony_yield_for_worker` gained a `has_docks` param, threaded from every production/preview/badge caller (`turn.c`, `colony_preview.c`, `colony_screen.c` area overlay + jobs popup) |
| SoL mod: AI zero-out | Zeroed outright for AI (strong, cross-validated hypothesis — see manufacturing_worker_calc_1d4c.md) | **2026-08-15 fix** — `colony_prod_sol_bonus_field` (new function), wired into both field-yield call sites (`turn.c`, `colony_preview.c`); building contexts (craft/bells/crosses/hammers) keep the shared `colony_prod_sol_bonus`, unaffected |
| Town commons | Peel pending | Fixture formula |

---

## Explicitly excluded here

- Full SoL / Tory production math — [sons_of_liberty.md](sons_of_liberty.md)
- Building manufacturing — [building_production.md](building_production.md)
- Combat / movement columns from the Terrain Chart
