# Seed-100 Brave / early-AI notes

Working notes for `smoke_mapgen_seed100` + `smoke_ai_turns` (VR_SEED=100).

## Status (phase 6)

| Gate | State |
|------|--------|
| Default (empiricism) | GREEN |
| `AI_QUIET_ASM=1` | RED — Apache init miss |
| Residual t2–t6 | 50 rows |

## Empiricism vs DOS quiet

Quiet ASM (`LAB_521d_4fb4`): `range(1,3)`, river/fa `+1` else `−2f76`, gated `−diff²×2`/fog.  
**Not** in DOS quiet: base 200, facing +4/−6/+3, home-dist, −0x28, +5, stay. Those are Linux inventions (`FUN_124c_0040` unused in `20e6`).

## Phase 6 A/B isolation

```bash
AI_LCG_AUDIT=1 ./build/smoke_mapgen_seed100
AI_QUIET_ASM=1 AI_LCG_AUDIT=1 ./build/smoke_mapgen_seed100
```

### Pulse-enter nexts (init)

| nation | emp nexts | asm nexts | delta |
|--------|-----------|-----------|-------|
| 4 | 26951 | 26951 | 0 |
| 5 | 27007 | 27001 | −6 |
| 6 | 27015 | 27008 | −7 |
| **7** | **27046** | **27032** | **−14** |

Delta at n=7 = stay surplus from n=4..6 (6+1+7). Same `rng_state` print is misleading (seeded word); **nexts** is the real stream cursor.

### Critical Brave (golden unit at `(46,52)`)

| mode | start | dir | end |
|------|-------|-----|-----|
| emp | `(47,53)` | **7** (NW) | `(46,52)` |
| asm | `(47,53)` | **6** (W) | `(46,53)` |

### Classification

1. **RNG-before:** ASM enters n=7 with 14 fewer nexts (stay).
2. After forcing +1 next/pick (stay-shaped sync): n=7 nexts **match** (27046) but ASM still picks **dir=6**. Sync reverted (not DOS).

**Verdict: dir-only at `(47,53)`** once RNG is equalized — ASM quiet score prefers W over empiricism/golden NW. Next RE: per-dir score dump at `(47,53)` (terrain/facing/fog terms) vs live DOS, not more LCG stay patches.

## Quiet cutover log

| Phase | Result |
|-------|--------|
| 2–5 | cutover red; LCG stay insufficient |
| 6 | A/B: miss is n=7 Brave `(47,53)` dir 6 vs 7 |

## Spent (parallel)

`VR_B465R` → AL. See `tools/brave_dump/midturn_465b.md`.
