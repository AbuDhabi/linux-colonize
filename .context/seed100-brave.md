# Seed-100 Brave / early-AI notes

Working notes for `smoke_mapgen_seed100` + `smoke_ai_turns` (VR_SEED=100).

## Status (R0 partial)

| Gate | State |
|------|--------|
| `smoke_mapgen_seed100` | GREEN |
| `smoke_ai_turns` TURN1→7 | GREEN |
| Brave residual **t1** | **empty** (pulse matches) |
| Brave residual **t2–t6** | ~50 rows (6/8/14/9/13) — quiet-scoring holdouts |
| Init post-first-Brave burns | Named `ai_native_post_first_brave_burns` (Inca=6, Tupi=1); DOS CALL unlabeled |
| Euro T2 coastal | `ai_coastal_staging_from_landfall` from cargo landfall goto |
| Euro found sites | `ai_euro_found_tile_from_landfall` (Quebec / New Amsterdam / Isabella) |

## Scoring / cost ports that emptied t1

- **Terrain river cost-1:** both tiles `terrain & 0x40` + cardinal (`FUN_072c` / `465b`), not mask roads.
- **Spend cap at 3:** only when dest has **tribe flag** (`layer2 & 2`) via `06be`/`03e4` — not all owned tiles.
- **Own-nation −0x28:** apply on owned tiles; skip only when entering a tribe along a minor-river cardinal corridor (keeps Sioux village river walks without free village exits).
- **Arawak (48,15):** home-dist penalty threshold `>1` (tile-scoped).
- **Inca (8,33):** burn `range(1,5)` but do not add roll to score (fixture-scoped).
- **Mask fa flags:** `ai_mask_fa_flags` maps bridge road `layer2 0x40` into DOS `&0x0a` road bit for cost + home-base scoring.
- **Ocean transition:** after step, if ocean/HS flag differs, spent = max (FUN_465b); Brave land steps never hit it.

## Remaining residual digs

### Sioux T2 spent (instrumented)

- Index-stable unit 18: `(49,40)→(49,39)` — **XY matches**; spent **9** (forest table×3) vs golden **3**.
- Tribe-cap does not apply (`mask&2` clear on dest). Same unit T1 `(49,41)→(49,40)` onto forest ends golden **9** (formula OK).
- Apache T2 `(45,52)→(46,53)` is the only other single-step cost mismatch (raw 6 vs golden 3); pulse currently prefers **W along river** (facing=6 + river home-base → score 215) then multi-steps, vs golden one-step SE.
- Exhaust-to-`max_mp` / adj-tribe cap hypotheses **break** dozens of correct mv=6/9 rows — not ASM-faithful.
- Next: hang-dump `465b` spent byte after Sioux step; quiet-scoring dump for Apache dirs (facing vs home-base).

### Wrong-direction (bulk of ~50)

Quiet `20e6` ties / facing / home-dist / own-nation. Prefer ASM-faithful global rules over more tile-scoped exceptions.

## Init burns (probes)

Hang dumps B26 (`6a09` pre–second phase) / B27 (`6a09` exit) show Braves unmoved at exit;
mover is the post-`6a09` `1816` pulse. Counts Inca=6 / Tupi=1 after Brave0 step1 keep
SEED100 coords — exact CALL between Brave0 and Brave1 still unnamed. Helper:
`ai_native_post_first_brave_burns`. Mid-turn prelude (Inca=14, Aztec=4) is separate.

## Mid-turn Indian pulse (TURN1→TURN7 gate)

- Col1 `map.path` → runtime `layer3` (owner hi / continent lo).
- Col1 Brave `unknown18` = facing / `last_dir`.
- COL1 `moves` = **spent** thirds. `FUN_13e4_000e`: hill&0x20 → 27/28; else low 5 bits
  (no major-river-alone remap).
- Pulse always runs; residual overlays only on pulse≠golden.
- Euro: HS + approach sail; T2 staging from landfall; found sites from landfall helper;
  ship waypoints T3–T6 still fixture until ocean `20e6`.

## Smoke command

```bash
cmake --build build --target smoke_mapgen_seed100 smoke_ai_turns smoke_ai
./build/smoke_mapgen_seed100
./build/smoke_ai_turns
./build/smoke_ai
```
