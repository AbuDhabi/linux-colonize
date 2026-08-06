# Move scoring (`FUN_521d_20e6`) — quiet Brave annotated

## Status (phase 5)

| Piece | State |
|-------|--------|
| Annotated quiet + `54f5` + fog + `0682` bit0 | **Done** |
| Init-pulse LCG audit | **Done** — stay surplus +34; see `.context/seed100-brave.md` |
| Linux cutover | **Blocked** — LCG-aligned gated ASM still misses Apache `(46,52)` |

## Phase 5 finding

Aligning pick_dir burns to ASM (drop stay; `range(1,3)`; keep `post_first`) does **not** green goldens. Next: scoring-term gap (home/−0x28/+5 vs quiet path), not another fog/gate/LCG-stay pass.

## `LAB_521d_54f5` gate

```
(unit_on_dest < 0 && tribe_or_presence(dest) < 0) || owner(dest) == self
```

## LCG (ASM quiet target)

```
for d in 0..7:
  if rejected: continue
  score_base = range(1, 3)
# no stay in loop
```

## Related

- [`quiet_brave_scoring.c`](quiet_brave_scoring.c)
- [`.context/seed100-brave.md`](../../.context/seed100-brave.md)
