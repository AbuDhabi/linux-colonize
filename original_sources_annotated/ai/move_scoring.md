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
