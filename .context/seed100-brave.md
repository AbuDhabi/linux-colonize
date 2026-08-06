# Seed-100 Brave / early-AI notes

Working notes for `smoke_mapgen_seed100` + `smoke_ai_turns` (VR_SEED=100).

## Status (phase 11 — quiet-ASM mid-turn cutover)

| Gate | State |
|------|--------|
| Init pick (default) | Quiet ASM + stay LCG + 13 peels — **green** |
| Mid-turn pick (default) | Quiet ASM + stay LCG + 104 peels + 8 residuals — **green** |
| Force empiricism | `AI_EMPIRICISM=1` or `AI_QUIET_ASM=0` (keeps emp residual set) |
| `smoke_mapgen_seed100` / `smoke_ai_turns` | GREEN |
| Far ocean/land vs SAV | **AGREE** |
| Complete Map / Reveal | Irrelevant (viewpoint only) |
| DOS hang campaign | **Parked** |

## Quiet mid-turn inventory (phase 11)

`AI_QUIET_MIDTURN=1` / default quiet, residuals off: classify vs TURN goldens.

| Class | Count (t1…t6) | Notes |
|-------|----------------|-------|
| Dir-only (scoring) | 104 along golden LCG | Mid peels `(turn,nation,xy)→dir` after scoring burns |
| Multi-step (non-adjacent end) | 5 | Residual overlay (not peelable) |
| Spent/tw only (XY match) | 3 | Residual overlay; `465b` parked |
| Cascade from earlier wrong step | (absorbed) | Partial peel sets cascade; full-path miss set is stable |

Quiet residual tables (after peels): **8 rows** — t1:2, t2:3, t3:1, t4:1, t5:0, t6:1.

Empiricism still needs its larger XY overlay set (selected when quiet ASM off).

```bash
./build/smoke_ai_turns
AI_EMPIRICISM=1 ./build/smoke_ai_turns
./build/smoke_mapgen_seed100
```

## Quiet ASM init inventory (phase 10)

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
for several tiles (hence peels until quiet terms catch up).
