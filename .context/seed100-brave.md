# Seed-100 Brave / early-AI notes

Working notes for `smoke_mapgen_seed100` + `smoke_ai_turns` (VR_SEED=100).

## Status (phase 7)

| Gate | State |
|------|--------|
| Default (empiricism) | GREEN |
| `AI_QUIET_ASM=1` | RED — dir-only at `(47,53)` |
| Residual t2–t6 | 50 rows |

## Phase 7 — Per-dir score dump `(47,53)`

```bash
AI_QUIET_ASM=1 AI_LCG_AUDIT=1 AI_ASM_STAY_SYNC=1 ./build/smoke_mapgen_seed100
AI_LCG_AUDIT=1 ./build/smoke_mapgen_seed100
```

Matched RNG at n=7 (`nexts=27046` both). `last_dir=0`.

### ASM quiet terms (accepted dirs)

| d | dest | base | terr | face | fog+8 | fog−2 | total | far | far_ocean |
|---|------|------|------|------|-------|-------|-------|-----|-----------|
| 3 | (48,54) | 1 | −2 | −18 | 0 | 0 | −19 | (51,57) | yes |
| 4 | (47,54) | 1 | −2 | −32 | +8 | −2 | −27 | (47,57) | no |
| 5 | (46,54) | 1 | −3 | −18 | 0 | 0 | −20 | (43,57) | yes |
| **6** | **(46,53)** | **2** | **−2** | **−8** | **+8** | **0** | **0** | **(43,53)** | **no** |
| **7** | **(46,52)** | **3** | **−2** | **−2** | **0** | **0** | **−1** | **(43,49)** | **yes** |

ASM **best=6**. Empiricism **best=7** (totals 208 vs 212; no fog).

### Flipping term

**Fog `+8` on dir 6** (far land `(43,53)`). Dir 7 far `(43,49)` is **ocean** → no `+8`.  
Facing and base favor NW (d7); fog alone flips W over NW (0 > −1).

Without fog on d6: d6=−8, d7=−1 → would pick golden NW.

### Verdict

No quiet **formula bug** proven: ocean reject on far `(43,49)` and `+8` on land far match annotated DOS fog. Empiricism lacks fog so picks NW.

**Next RE (DOS hang):** at init Brave `(47,53)`, does live `20e6` apply `+8` for dir 6 and choose W, or skip fog / use another path that yields NW `(46,52)`? Until then keep empiricism default; `AI_QUIET_ASM` + `AI_ASM_STAY_SYNC` audit-only.

## Empiricism vs DOS quiet

Quiet ASM: `range(1,3)`, river/fa `+1` else `−2f76`, gated facing/fog.  
Not DOS quiet: base 200, +4/−6/+3, home, −0x28, +5, stay.

## Spent (parallel)

`VR_B465R` → AL. See `tools/brave_dump/midturn_465b.md`.
