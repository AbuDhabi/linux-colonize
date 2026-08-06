# Move scoring (`FUN_521d_20e6`) — quiet Brave annotated

## Status (phase 2)

| Piece | State |
|-------|--------|
| Annotated quiet Brave (`quiet_brave_scoring.c`) | **Done** — ASM `LAB_521d_4ea9` |
| Linux `ai_native_pick_dir` cutover | **Blocked** — coherent ASM cutover regresses `smoke_mapgen_seed100` / `smoke_ai_turns`; empirical formula restored |
| Full `20e6` (Euro / combat / ocean) | Parked |

## Why cutover failed (2026-08-06)

Replacing empirical base-200 / facing / home / −0x28 / roll(1,5) with ASM
`range(1,3)` + `−diff²×2` + `−2f76[terr]` (fog still omitted) broke init pulse
and TURN1→2 Brave XY. Likely causes:

1. **LCG burn shape** — ASM burns `range(1,3)` per dir; empiricism burns
   `range(1,5)` plus different stay handling.
2. **Missing `bVar20` fog/explore** — annotated but not yet wired with real
   map-seen / explore-mask accessors in the Linux port.
3. Historical note: facing-only or facing+−2f76 **on empirical base** also
   regresses — do not mix.

Next cutover attempt must land **base + terrain + facing + fog** together and
match DOS LCG call order end-to-end.

## Formula (Brave flags 0x10\|0x20)

```
score = range(1, 3)
if (unit_river && dest_river && cardinal) || (unit_fa && dest_fa):
  score += 1
else:
  score -= terr_cost[terr]          # raw DS:0x2f76 byte, not ×3
score += -facing_diff^2 * 2
if bVar20: fog/explore neighbor bonuses
# colony pull no-op when colony_count==0 / Brave combat strength 0
```

## Related

- [`quiet_brave_scoring.c`](quiet_brave_scoring.c)
- [`SYMBOL_MAP.md`](../SYMBOL_MAP.md)
- [`.context/seed100-brave.md`](../../.context/seed100-brave.md)
- [`docs/ai_transcription.md`](../../docs/ai_transcription.md)
