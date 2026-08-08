# Move scoring (`FUN_521d_20e6`) — quiet Brave annotated

## Status (phase 13 — multi-step cleared)

| Piece | State |
|-------|--------|
| Annotated quiet + `54f5` + fog | **Done** |
| Linux init pick | Quiet ASM (stay LCG + seed-100 peels) |
| Linux mid-turn pick | Quiet ASM (stay LCG + mid peels + **2** spent residuals) |
| `FUN_465b_0000` section map | **Done** — [`move_spent.c`](move_spent.c) |
| Ocean / HS force-to-max | Annotated; Linux uses `euro_settlement_owner` (0358); **not** Sioux T2 writer (`dump_b465f3`) |
| Multi-step / Inca tw | Cleared (river cost=1 peels; `097a` continues while spent&lt;3) |
| Spent-only Sioux/Apache | Residual; dump-free predicates exhausted (phase 17); hang X last resort |
| Force empiricism | `AI_EMPIRICISM=1` / `AI_QUIET_ASM=0` |
| Far `(43,49)`/`(43,53)` vs SAV | **AGREE** |
| Complete Map / Reveal | **Irrelevant** |
| Coarse fog plane | Dual index; Linux buffer; `+8` gated |
| DOS hang recipes | **Parked** (X/`dump_b465x3` only when dump-free done) |

## Quiet residual classes (phase 13)

| Class | Rows | Notes |
|-------|------|-------|
| Multi-step / Inca | **0** | River-first peels; see `.context/seed100-brave.md` |
| Spent-only (XY match) | t2 Apache; t2 Sioux | Post-ADD writer; dump-free exhausted |

## Coarse fog (`DS:0x9faa`, size `0x10e`)

| Use | Index |
|-----|-------|
| Explore `+8` | `(x>>2) + (y>>2)*18` |
| Tribe spacing | `(y/5) + (x/5)*18` |

## Init / mid peels

Thirteen seed-100 init tiles + mid-turn peels (incl. river multi-step and a
few cascade fixes). See `.context/seed100-brave.md`.

## Quiet ASM (DOS)

- Base `range(1,3)`; river/fa `+1` else `−2f76`
- Gated facing / coarse fog
- Stay-shaped LCG burn after each pick (Linux stream sync)

## Euro / ocean / founding (thin section-map — mid-planner **OPEN**)

Full `FUN_521d_20e6` ~2180 lines (`viceroy_unpacked.c` ~88266–90445). Quiet
Brave slice is annotated; **land/combat Euro scoring is OPEN** (unpark #4).
Ocean/ship fixture retirement stays R5 until ocean/HS branch ports.

### Callers

| From | Via | Notes |
|------|-----|-------|
| `FUN_521d_5b66` | `2a1f_04f4` | Per-unit act entry; non-zero return aborts act |
| (not `6d8e` directly) | — | Dispatcher calls `5b66`, which scores |

### Founding tile pick (`FUN_521d_06ae`)

| Item | Detail |
|------|--------|
| Thunk | `2a1f_04ac` → `FUN_521d_06ae` |
| Annotated | `pick_best_adjacent_founding_tile` in [`euro_goals.c`](euro_goals.c) |
| Decomp site | **Sole call** ~89587 inside `20e6` (land/non-naval walk; `type==0x0b` filter arg) |
| Behavior | Score dirs 0..8 around unit/colony tile; prefer empty land; terrain + explore extras |
| Linux second+ colony | `ai_euro_pick_founding_tile`: when `colony_count>=1`, +10 score if `map_tile_is_coastal` (Docks/port — fandom; first colony uses plain `ai_goals_pick_founding_tile`) |
| Linux PORT DEBT | `ai_euro_found_tile_from_landfall` / coastal staging fixtures |

Do not confuse with `FUN_281f_04ac` sites inside `5b66` case 10 (different helper).

### Ocean / ship scoring (early-settle gap)

Naval band in `20e6` is often `type ∈ (0x0c, 0x13)` — wider than dispatcher
`SHIP_A..C` (`0x0a..0x0c`). Atlantic approach and T3–T6 coastal ship waypoints
in Linux are still **fixture tables**. Retiring them needs the ocean/HS branch
of `20e6` (not the quiet Brave path). Until then:

- Seed-100 landfall gotos: `ai_coastal_staging_from_landfall` (T2)
- Mid-turn ship XY: fixture waypoints (1–2 tiles off golden until ocean score)

Combat / land Euro arms: **OPEN** (unpark #4). Thin adjacent-foe pick prefers
weaker defense / non-fortified (`ai_euro_land_best_adjacent_foe`). Thin naval
adjacent-foe pick prefers lower defense (`ai_euro_naval_best_adjacent_foe`;
damage mods PARKED). Ocean west-explore HS bias deepened when ship on HS.
Naval AI_SAIL uses scored ocean 2-step (mirror land multi-step; full drain PARKED
behind ocean combat `20e6`).
Ocean/ship / deep fog explore / colony-tile deep T3 still R5.
