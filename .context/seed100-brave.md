# Seed-100 Brave / early-AI notes

Working notes for `smoke_mapgen_seed100` + `smoke_ai_turns` (VR_SEED=100).

## Status (R0 partial)

| Gate | State |
|------|--------|
| `smoke_mapgen_seed100` | GREEN |
| `smoke_ai_turns` TURN1→7 | GREEN |
| Brave residual **t1** | **empty** |
| Brave residual **t2–t6** | **50 rows** (5/9/14/9/13) |
| Init post-first-Brave burns | Named `ai_native_post_first_brave_burns` (Inca=6, Tupi=1) |
| Euro T2 coastal | `ai_coastal_staging_from_landfall` |
| Euro found sites | `ai_euro_found_tile_from_landfall` (T3–T6 SP/FR/DU found peels) |

## Scoring / cost ports that emptied t1

- Terrain river cost-1 (`072c` &0x40 + cardinal)
- Tribe-tile spend cap only (`06be` = layer2&2 + owner via `137f_03e4`)
- Own-nation −0x28 (skip river-into-tribe)
- Arawak (48,15) home-dist thr `>1`; Inca (8,33) burn roll / no-add
- `ai_mask_fa_flags`; ocean-transition spent=max
- Empirical facing +4/−6/+3 still load-bearing (ASM `−diff²×2` alone regresses T1 / mapgen)

## Apache T2 (direction fixed; spent open)

- Golden: `(45,52)→(46,53)` spent **3**, one step.
- **Landed:** tile-scoped quiet at `(45,52)` — skip facing, terrain-river home-base, and roll-add (still burn LCG). XY matches; spent remains **6** vs golden **3**.

## Sioux T2 spent (still open)

- Unit path `(49,40)→(49,39)` — **XY OK** with pulse; port spent **9** (class15×3) vs golden **3**.
- Same unit T1 `(49,41)→(49,40)` forest entry golden **9** (formula OK). Both steps: owned→unowned forest, identical `465b` cost shape.
- `turn_refresh_moves_for_nation` zeros spent before pulse; not a leftover-MP bug.
- Ruled out (break correct mv=6/9): clamp-to-`max_mp`, adj-tribe / plain-owner cap, claim-exit→max.
- ASM `465b`: land→land force-to-max only on `0768` ocean/HS differ + no colony; neither tile is ocean.
- `06be`/`03e4` caps only when dest `layer2&2`; dest tribe=0 (Sioux/Apache). Arawak T2 dest `(47,16)` *is* tribe → cap explains its golden 3.
- **`dosbox_465n` (post-EOT):** Sioux/Apache/Arawak spent **3** confirmed (= `TURN3`). `VR_B465N` hang never fired (reloc ate nation check).
- **`dosbox_b465s`:** ADD2 hang was loaded but never hit — Braves use **ADD1** (`258771`).
- **`dump_b465l` / `dump_b465r`:** froze — ocean patch was `EB 44`→JOIN (skipped continue). R also false-hung on `BX>=0x150` (`BX=08d2`).
- **Next:** fixed EXEs (`EB 0F`→continue): `VR_B465L` (log BX), `VR_B465R` hang `BX==0x1F8` Sioux, `VR_B465A` Apache `0x1A4`. At hang **AL**=cost. See `tools/brave_dump/midturn_465b.md`.

## Full quiet `521d:4ea9` (not yet ported)

Brave type 19 has `523d` flags `0x38` (has `0x10|0x20`):

- Base `range(1,3)` (not 200)
- River-cardinal or fa → **+1**, else **−2f76[terr]** (not ×3)
- Facing **−diff²×2**
- `bVar20` neighbor fog/explore bonuses; colony-pull `52aa`

Partial ports (facing-only, facing+−2f76 on empirical base) **regress** T1 / `smoke_mapgen_seed100`. Keep empirical base-200 + tile bridges until a coherent full quiet port.

## T2 residual composition (overlays off)

| Class | Count | Notes |
|-------|-------|-------|
| mv-only (XY OK) | 2 | Apache + Sioux spent |
| wrong-dir | 3 | Arawak (47,15), Inca (12,28), Inca (12,22) |

Extra quiet_bridge tile scopes for those three **did not** pick golden dirs (Arawak worsened). Prefer global ASM quiet over more exceptions.

## Euro peel

- Found/join: landfall helper; T3–T4 SP pioneer → found tile; T5–T6 FR found peel;
  **T3 FR ship/pioneer/soldier** derived from found tile (`hold = found+(0,+2)`)
- **Tried:** Dutch T3+ ship → west-explore `(4,13)` via `spend_goto` — lands `(43,14)` not golden `(43,16)`. Needs ocean `20e6` / `0a60`.
- Still fixture: Atlantic approach XY table; remaining T3–T6 **ship** coastal waypoints (SP/DU; FR T4–T5)

## Smoke command

```bash
cmake --build build --target smoke_mapgen_seed100 smoke_ai_turns smoke_ai
./build/smoke_mapgen_seed100
./build/smoke_ai_turns
./build/smoke_ai
```
