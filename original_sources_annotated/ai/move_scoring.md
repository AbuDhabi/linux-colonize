# Move scoring (`FUN_521d_20e6`) — quiet Brave annotated

## Status (phase 4)

| Piece | State |
|-------|--------|
| Annotated quiet + `LAB_521d_54f5` gate + −10 + fog + `0682` bit0 | **Done** |
| Linux `ai_native_pick_dir` cutover | **Still blocked** — phase 4 gated cutover reverted (same Apache `(46,52)` miss as phase 3) |
| Full `20e6` (Euro / combat / ocean) | Parked |

## `LAB_521d_54f5` gate (annotated)

Facing, military −10, and `bVar20` fog run **only** when:

```
(unit_on_dest < 0 && tribe_or_presence(dest) < 0) || owner(dest) == self
```

| Callee | Annotated |
|--------|-----------|
| `FUN_281f_07e0` | `unit_index_on_tile` |
| `FUN_281f_06d2` | `tile_tribe_or_presence` |
| `FUN_281f_06dc` | `owner_nibble` |
| `FUN_281f_0682` | `tile_owner_or_presence` (**layer2 bit0**) |

## Phase 4 cutover finding

Gated ASM (base `range(1,3)` + terrain + facing/−10/fog with bit0 `0682` + early all-unseen coarse fog) still fails `smoke_mapgen_seed100` the same way as ungated fog: **missing Apache unit at expected `(46,52)`**. Gate/fog were necessary for fidelity but **not sufficient** for green goldens.

Likely remaining gap: **LCG shape** (empiricism stay + `range(1,5)` per dir vs ASM `range(1,3)` only on accepted dirs) desyncs the multi-Brave init pulse before any single-unit score can match. Home-dist / −0x28 / +5 remain empiricism-only and may still be load-bearing if quiet ASM is incomplete for NEW WORLD.

## LCG burn sequence (ASM quiet target)

```
for d in 0..7:
  if rejected: continue  # no burn
  score_base = range(1, 3)
# stay not in this loop
```

## Related

- [`quiet_brave_scoring.c`](quiet_brave_scoring.c)
- [`.context/seed100-brave.md`](../../.context/seed100-brave.md)
