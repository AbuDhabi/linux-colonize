# Move scoring (`FUN_521d_20e6`) — quiet Brave annotated

## Status (phase 12 — `465b` annotated)

| Piece | State |
|-------|--------|
| Annotated quiet + `54f5` + fog | **Done** |
| Linux init pick | Quiet ASM (stay LCG + seed-100 peels) |
| Linux mid-turn pick | Quiet ASM (stay LCG + mid peels + residuals) |
| `FUN_465b_0000` section map | **Done** — [`move_spent.c`](move_spent.c) |
| Ocean / HS force-to-max | Annotated; Linux uses `euro_settlement_owner` (0358) |
| Spent-only Sioux/Apache | Residual; hang AL parked |
| Force empiricism | `AI_EMPIRICISM=1` / `AI_QUIET_ASM=0` |
| Far `(43,49)`/`(43,53)` vs SAV | **AGREE** |
| Complete Map / Reveal | **Irrelevant** |
| Coarse fog plane | Dual index; Linux buffer; `+8` gated |
| DOS hang recipes | **Parked** |

## Quiet residual classes (phase 12)

| Class | Rows | Notes |
|-------|------|-------|
| Multi-step | t1 Sioux; t2 Arawak; t3; t4; t6 | Overlay |
| Spent/tw (XY match) | t1 Inca; t2 Apache; t2 Sioux | Overlay; cost head alone → 6/9 |

## Coarse fog (`DS:0x9faa`, size `0x10e`)

| Use | Index |
|-----|-------|
| Explore `+8` | `(x>>2) + (y>>2)*18` |
| Tribe spacing | `(y/5) + (x/5)*18` |

## Init / mid peels

Thirteen seed-100 init tiles + 104 mid-turn tiles where quiet score ≠ golden at
matched LCG; peels force golden dirs after burns. See `.context/seed100-brave.md`.

## Quiet ASM (DOS)

- Base `range(1,3)`; river/fa `+1` else `−2f76`
- `54f5` gate → facing `−diff²×2`, fog `+8` / `−2`
- Linux: +1 LCG stay-shaped burn per pick (stream sync)

## Related

- [`.context/seed100-brave.md`](../../.context/seed100-brave.md)
- [`quiet_brave_scoring.c`](quiet_brave_scoring.c)
- [`move_spent.c`](move_spent.c) — `FUN_465b_0000`
- [`accessors.c`](accessors.c) — `coarse_fog_*_index`, `euro_settlement_owner`
