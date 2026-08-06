# Move scoring (`FUN_521d_20e6`) — quiet Brave annotated

## Status (phase 8)

| Piece | State |
|-------|--------|
| Annotated quiet + `54f5` + fog | **Done** |
| Linux default | Empiricism (green) |
| Far `(43,49)`/`(43,53)` vs SAV | **AGREE** (`probe_far_ocean_4753`) |
| `AI_QUIET_ASM` cutover | Blocked — fog `+8` prefers W; map-correct |
| Next DOS hang | [`init_20e6_4753.md`](../../tools/brave_dump/init_20e6_4753.md) |

## Phase 8 — Far ocean

Linux seed-100 terrain matches `SEED100.SAV` at unit/dest/far cells. No map fix;
do not disable fog to match empiricism.

## Phase 7 finding (`(47,53)`, matched RNG)

| Dir | ASM total | Driver |
|-----|-----------|--------|
| 6 W → `(46,53)` | **0** (best) | fog `+8` (far land `(43,53)`) |
| 7 NW → `(46,52)` | −1 | far `(43,49)` **ocean** → no `+8` |

Facing/base favor d7; **fog `+8` is the flip**. Hang sites: `521d:5730` (`+8`),
`521d:58a5` (write dir to `unit+0x314f`).

## Quiet ASM (DOS)

- Base `range(1,3)`; river/fa `+1` else `−2f76`
- `54f5` gate → facing `−diff²×2`, fog `+8` / `−2`
- Empiricism home/−0x28/+5/base-200 are **not** DOS quiet

## Related

- [`.context/seed100-brave.md`](../../.context/seed100-brave.md)
- [`quiet_brave_scoring.c`](quiet_brave_scoring.c)
- [`tools/brave_dump/init_20e6_4753.md`](../../tools/brave_dump/init_20e6_4753.md)
