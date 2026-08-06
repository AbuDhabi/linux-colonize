# Move scoring (`FUN_521d_20e6`) — quiet Brave annotated

## Status (phase 3)

| Piece | State |
|-------|--------|
| Annotated quiet Brave + fog | **Done** — `quiet_brave_scoring.c` |
| Linux `ai_native_pick_dir` cutover | **Still blocked** — base+terrain+facing+fog regresses both smokes; empiricism restored |
| Full `20e6` (Euro / combat / ocean) | Parked |

## Fog semantics (Indian Braves)

From decomp `bVar20` block:

| Term | When | Indian NEW WORLD |
|------|------|------------------|
| `+8` far coarse-unseen land | `coarse[-0x6056]`==0, !ocean, inset | Yes (early ≈ all unseen) |
| `+4` ship west bias | `local_34` ship | No |
| `+2` explore mask | **`nation_id < 4` only** | **Skipped** |
| `−2` owner/presence | `FUN_281f_0682` ≥ 0 | Yes (approx) |
| `+2f79[terr]` | `local_6a != 0` | No (`5382` bit0 clears it) |

`FUN_281f_074a` reads explore plane at **DS:0x168** (not layer3).

## Cutover attempts

| Attempt | Result |
|---------|--------|
| Phase 2: base+terrain+facing, no fog | Regressed mapgen + TURN1→2; reverted |
| Phase 3: + Indian fog (+8/−2) | Still regressed (e.g. Apache init XY); reverted |

Likely remaining gaps before a green cutover:

1. **LAB_521d_54f5 entry gate** — facing/fog only when `(local_5c<0 && 06d2<0) \|\| owner==self`; not every scored dir.
2. **Military neighbor −10** loop before fog (combat-strength-0 Braves can still enter).
3. **Coarse fog / 0682** approximations vs live DOS planes.
4. **LCG** — empiricism burns `range(1,5)` (+ stay); ASM burns `range(1,3)` per scored dir only.

## LCG burn sequence (ASM quiet target)

```
for d in 0..7:
  if rejected (ocean/foreign/oob): continue  # no burn
  score_base = range(1, 3)   # one burn
  # terrain / facing / fog — no further RNG in Indian quiet path
# stay not in this loop
```

## Related

- [`quiet_brave_scoring.c`](quiet_brave_scoring.c)
- [`.context/seed100-brave.md`](../../.context/seed100-brave.md)
- [`docs/ai_transcription.md`](../../docs/ai_transcription.md)
