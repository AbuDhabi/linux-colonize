# Move scoring (`FUN_521d_20e6`) — quiet Brave annotated

## Status (phase 6)

| Piece | State |
|-------|--------|
| Annotated quiet + `54f5` + fog + `0682` bit0 | **Done** |
| Empiricism home/−0x28/+5/base-200 | **Not DOS quiet** — Linux inventions |
| Linux default | Empirical (smokes green) |
| `AI_QUIET_ASM=1` | A/B path for init isolation |

## Phase 6 isolation

A/B (`AI_QUIET_ASM` + `AI_LCG_AUDIT`): golden `(46,52)` is n=7 Brave from `(47,53)`.
Emp dir=7; ASM dir=6. Stay surplus (−14 nexts at n=7) is real but syncing it
still leaves dir=6 → **dir-only** score gap at that tile. Do not add empiricism
home/−0x28/+5 into annotated quiet.

## Quiet ASM (DOS `LAB_521d_4fb4`, flags `0x10|0x20` at `0x523d`)

- Base `FUN_281f_04d4(1,3)`
- River/fa cardinal → `+1`, else `−DS:0x2f76[terr]` (not ×3)
- `LAB_521d_54f5` gate → facing `−diff²×2`, military −10, `bVar20` fog
- **No** home-dist (`FUN_124c_0040` unused in `20e6`), **no** −0x28, **no** +5, **no** base 200

## Empiricism (Linux only — keeps goldens)

Base 200, facing +4/−6/+3, home-dist, −0x28, +5, `range(1,5)` + stay. Do **not** annotate these into quiet ASM.

## Related

- [`quiet_brave_scoring.c`](quiet_brave_scoring.c)
- [`.context/seed100-brave.md`](../../.context/seed100-brave.md)
