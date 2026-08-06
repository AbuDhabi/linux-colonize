# Move scoring (`FUN_521d_20e6`) — quiet Brave annotated

## Status (phase 10 — init cutover)

| Piece | State |
|-------|--------|
| Annotated quiet + `54f5` + fog | **Done** |
| Linux init pick | Quiet ASM (stay LCG + seed-100 peels) |
| Linux mid-turn pick | Empiricism (residuals); `AI_QUIET_MIDTURN=1` |
| Far `(43,49)`/`(43,53)` vs SAV | **AGREE** |
| Complete Map / Reveal | **Irrelevant** |
| Coarse fog plane | Dual index; Linux buffer; `+8` gated |
| DOS hang recipes | **Parked** |

## Coarse fog (`DS:0x9faa`, size `0x10e`)

| Use | Index |
|-----|-------|
| Explore `+8` | `(x>>2) + (y>>2)*18` |
| Tribe spacing | `(y/5) + (x/5)*18` |

## Init peels

Thirteen seed-100 init tiles where quiet score ≠ golden at matched LCG; peels
force empiricism/golden dirs after burns. See `.context/seed100-brave.md`.

## Quiet ASM (DOS)

- Base `range(1,3)`; river/fa `+1` else `−2f76`
- `54f5` gate → facing `−diff²×2`, fog `+8` / `−2`
- Linux: +1 LCG stay-shaped burn per pick (stream sync)

## Related

- [`.context/seed100-brave.md`](../../.context/seed100-brave.md)
- [`quiet_brave_scoring.c`](quiet_brave_scoring.c)
- [`accessors.c`](accessors.c) — `coarse_fog_*_index`
