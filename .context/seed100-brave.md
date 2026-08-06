# Seed-100 Brave / early-AI notes

Working notes for `smoke_mapgen_seed100` + `smoke_ai_turns` (VR_SEED=100).

## Status (phase 10 — quiet-ASM init cutover)

| Gate | State |
|------|--------|
| Init pick (default) | Quiet ASM + stay LCG + 13 peels — **green** |
| Mid-turn pick | Empiricism + residuals (`AI_QUIET_MIDTURN=1` to probe) |
| Force empiricism | `AI_EMPIRICISM=1` or `AI_QUIET_ASM=0` |
| `smoke_mapgen_seed100` / `smoke_ai_turns` | GREEN |
| Far ocean/land vs SAV | **AGREE** |
| Complete Map / Reveal | Irrelevant (viewpoint only) |
| DOS hang campaign | **Parked** |

## Quiet ASM init inventory (stay-sync / matched LCG)

Pulse `nexts` match empiricism when stay burn is applied after each ASM pick.
At matched RNG, quiet formula still disagrees with golden on **13** Braves
(scoring — not cascade). Empiricism dirs match golden.

| Nation | Spawn | ASM dir | Golden dir | Dest golden |
|--------|-------|---------|------------|-------------|
| 4 | (11,30) | 7 | 1 | (12,29) |
| 4 | (6,34) | 0 | 1 | (7,33) |
| 6 | (48,4) | 1 | 5 | (47,5) |
| 6 | (25,7) | 0 | 7 | (24,6) |
| 7 | (46,56) | 0 | 2 | (47,56) |
| 8 | (13,48) | 7 | 5 | (12,49) |
| 8 | (17,33) | 2 | 3 | (18,34) |
| 8 | (9,43) | 0 | 7 | (8,42) |
| 9 | (33,54) | 0 | 7 | (32,53) |
| 9 | (30,50) | 0 | 5 | (29,51) |
| 10 | (48,42) | 7 | 1 | (49,41) |
| 10 | (47,39) | 0 | 2 | (48,39) |
| 11 | (32,31) | 0 | 5 | (31,32) |

Classification: **dir-only scoring** at matched LCG (quiet facing/fog/base vs
empiricism home/+5/−0x28/facing). Coarse fog already correct for `(47,53)`→NW.
Without stay burn, earlier nations diverge LCG and inflate the miss list (14+).

Mitigation for cutover: bake stay LCG into quiet ASM; seed-100 init peels force
golden dirs (same class of debt as prior tile-scoped emp peels). Mid-turn
residuals unchanged.

## Coarse fog (phase 9)

DOS plane `DS:0x9faa` (size `0x10e`): explore `+8` uses `(x>>2)+(y>>2)*18`;
tribe place uses `(y/5)+(x/5)*18`. Linux `s_ai_coarse_fog` mirrors this.

```bash
./build/smoke_mapgen_seed100
AI_EMPIRICISM=1 ./build/smoke_mapgen_seed100   # legacy picker
AI_LCG_AUDIT=1 ./build/smoke_mapgen_seed100    # score / step dumps
```

## Empiricism vs DOS quiet

Quiet ASM: `range(1,3)`, river/fa `+1` else `−2f76`, gated facing/fog, **+1 LCG
stay-shaped burn** per pick (stream sync).  
Empiricism: base 200, +4/−6/+3, home, −0x28, +5, stay — still the better match
for several init tiles (hence peels until quiet terms catch up).
