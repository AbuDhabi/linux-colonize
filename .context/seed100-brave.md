# Seed-100 Brave / early-AI notes

Working notes for `smoke_mapgen_seed100` + `smoke_ai_turns` (VR_SEED=100).

## Status (R0 partial + phase 4 annotate)

| Gate | State |
|------|--------|
| `smoke_mapgen_seed100` | GREEN |
| `smoke_ai_turns` TURN1→7 | GREEN |
| Brave residual **t1** | **empty** |
| Brave residual **t2–t6** | **50 rows** (5/9/14/9/13) — unchanged (cutover not landed) |
| Quiet ASM + `54f5` gate + fog | **Annotated** — `quiet_brave_scoring.c` |
| Quiet Linux cutover | **Blocked** — phases 2–4 reverted |

## Quiet cutover log

| Phase | Change | Result |
|-------|--------|--------|
| 2 | base+terrain+facing, no fog | Both smokes fail |
| 3 | + Indian fog ungated | Same; Apache init XY miss |
| 4 | + `54f5` gate + −10 + `0682` bit0 | Same Apache `(46,52)` miss; reverted |

**Concrete finding:** gate/fog/`0682` alone do not unblock. Next RE: compare per-Brave LCG burn counts on init pulse (empiricism stay+`range(1,5)` vs ASM `range(1,3)`), and whether quiet ASM omits still-load-bearing home/−0x28/+5 terms.

### Intended ASM LCG (when cutover lands)

Per scored dir 0..7: one `range(1,3)`. Rejected dirs burn nothing. Stay outside loop.

## Apache / Sioux spent (parallel — not scoring)

| Unit | XY | Spent Linux vs golden |
|------|-----|------------------------|
| Apache T2 `(45,52)→(46,53)` | OK (tile bridge) | 6 vs **3** |
| Sioux T2 `(49,40)→(49,39)` | OK | 9 vs **3** |

Hang-dump next (see `tools/brave_dump/midturn_465b.md`):

1. `VR_B465L.EXE` logger → `dump_b465l2` (last BX at ADD1)
2. `VR_B465R.EXE` hang `BX==0x1F8` (Sioux) → **AL = step cost**
3. `VR_B465A.EXE` hang `BX==0x1A4` (Apache) → **AL = step cost**

`tools/probe_sioux_spent.c` dumps tile/cost for those steps from TURN2.SAV (terrain class ×3 = 9 unless river/fa/tribe-cap). Dump must show why DOS forces 3.

## T2 residual composition (overlays off)

| Class | Count | Notes |
|-------|-------|-------|
| mv-only (XY OK) | 2 | Apache + Sioux spent |
| wrong-dir | 3 | Arawak (47,15), Inca (12,28), Inca (12,22) |

## Smoke command

```bash
cmake --build build --target smoke_mapgen_seed100 smoke_ai_turns smoke_ai
./build/smoke_mapgen_seed100
./build/smoke_ai_turns
./build/smoke_ai
```
