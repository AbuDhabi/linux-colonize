# Move scoring (`FUN_521d_20e6`) — quiet Brave annotated

## Status (phase 9 — coarse fog)

| Piece | State |
|-------|--------|
| Annotated quiet + `54f5` + fog | **Done** |
| Linux default | Empiricism (green) |
| Far `(43,49)`/`(43,53)` vs SAV | **AGREE** (`probe_far_ocean_4753`) |
| Complete Map / Reveal | **Irrelevant** to AI — viewpoint only (`unknown42[2]`); `SEED100` ≡ `SEED100_UNREVEALED` on `seen[]` + Apache XY |
| Coarse fog plane | Dual index recovered; Linux buffer + tribe `/5` writes + explore `>>2` +8 gate |
| `AI_QUIET_ASM` cutover | Still gated — validate vs SEED100; hang **not** required |
| DOS hang recipes | **Parked** (overlay/reloc); prefer goldens + coarse-fog port |

## Coarse fog (`DS:0x9faa`, size `0x10e`)

Same 270-byte plane, two index formulas (ASM-confirmed):

| Use | Index | Writer / reader |
|-----|-------|-----------------|
| Explore `+8` | `(x>>2) + (y>>2)*18` | Read in quiet `20e6`; OR bits from euro `0a60` |
| Tribe spacing | `(y/5) + (x/5)*18` | `FUN_6a09` store `1` after capital/satellite |

`FUN_6a09` / `FUN_521d_0a60` memset the plane (`FUN_1d1d_0dae`, cb=`0x10e`) at entry. Tribe `/5` marks do **not** clear explore `>>2` cells — do not assume village place alone kills `+8`.

## Phase 7 finding (`(47,53)`, matched RNG)

| Dir | ASM total | Driver |
|-----|-----------|--------|
| 6 W → `(46,53)` | **0** (best) | fog `+8` (far land `(43,53)`) under all-unseen assume |
| 7 NW → `(46,52)` | −1 | far `(43,49)` **ocean** → no `+8` |

Facing/base favor d7; **fog `+8` is the flip** when explore cell is zero. Golden / unrevealed twin both end **NW** — so live DOS does not match “always unseen + quiet”.

## Quiet ASM (DOS)

- Base `range(1,3)`; river/fa `+1` else `−2f76`
- `54f5` gate → facing `−diff²×2`, fog `+8` / `−2`
- Empiricism home/−0x28/+5/base-200 are **not** DOS quiet

## Related

- [`.context/seed100-brave.md`](../../.context/seed100-brave.md)
- [`quiet_brave_scoring.c`](quiet_brave_scoring.c)
- [`accessors.c`](accessors.c) — `coarse_fog_*_index`
- Optional hang notes (parked): [`tools/brave_dump/init_20e6_4753.md`](../../tools/brave_dump/init_20e6_4753.md)
