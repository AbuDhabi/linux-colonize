# Move scoring (`FUN_521d_20e6`) — quiet Brave annotated

## Status (phase 7)

| Piece | State |
|-------|--------|
| Annotated quiet + `54f5` + fog | **Done** |
| Linux default | Empiricism (green) |
| `AI_QUIET_ASM` cutover | Blocked — `(47,53)` fog `+8` prefers W |

## Phase 7 finding (`(47,53)`, matched RNG)

| Dir | ASM total | Driver |
|-----|-----------|--------|
| 6 W → `(46,53)` | **0** (best) | fog `+8` (far land `(43,53)`) |
| 7 NW → `(46,52)` | −1 | far `(43,49)` **ocean** → no `+8` |

Facing/base favor d7; **fog `+8` is the flip**. Not an empiricism-home issue. No quiet bug proven without DOS hang on whether fog fires.

## Quiet ASM (DOS)

- Base `range(1,3)`; river/fa `+1` else `−2f76`
- `54f5` gate → facing `−diff²×2`, fog `+8` / `−2`
- Empiricism home/−0x28/+5/base-200 are **not** DOS quiet

## Related

- [`.context/seed100-brave.md`](../../.context/seed100-brave.md)
- [`quiet_brave_scoring.c`](quiet_brave_scoring.c)
