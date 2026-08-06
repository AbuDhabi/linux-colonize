# Seed-100 Brave / early-AI notes

Working notes for `smoke_mapgen_seed100` + `smoke_ai_turns` (VR_SEED=100).

## Status (phase 13 — multi-step / act loop)

| Gate | State |
|------|--------|
| Init pick (default) | Quiet ASM + stay LCG + 13 peels — **green** |
| Mid-turn pick (default) | Quiet ASM + stay LCG + mid peels + **2** spent residuals — **green** |
| Multi-step / Inca tw | Cleared via river cost=1 peels (097a continues while spent&lt;3) |
| Spent-only Sioux/Apache | Still residual; hang AL parked (`midturn_465b.md`) |
| Force empiricism | `AI_EMPIRICISM=1` or `AI_QUIET_ASM=0` (keeps emp residual set) |
| `smoke_mapgen_seed100` / `smoke_ai_turns` | GREEN |
| DOS hang campaign | **Parked** |

## Quiet mid-turn inventory (phase 13)

| Class | Count | Rows | Notes |
|-------|------:|------|-------|
| Dir-only (scoring) | ~110 | peels | Mid peels `(turn,nation,xy)→dir` |
| Multi-step (cleared) | 0 | — | River-first peels; pulse already loops |
| Mis-keyed overlays (retired) | 0 | was t3/t6 | Wrong unit’s end snapped onto neighbor |
| Spent-only (XY match) | 2 | t2 Apache; t2 Sioux | Hang AL; do not invent cost caps |

Quiet residual table: **2 rows** (both t2 spent-only).

### Phase 13 classification (AI_STEP_AUDIT)

| Row | Pulse without fix | Root cause | Fix |
|-----|-------------------|------------|-----|
| t1 Inca `(7,33)→(8,32)` tw2/mv7 | One NE cost6, tw1/mv6 | Peel collapsed river path | Peel E then N: cost1+6 |
| t1 Sioux `(48,39)→(49,42)` tw3/mv8 | One E cost9 | Need river S first | Peel S; quiet continues S/SE |
| t2 Arawak `(19,37)→(17,38)` tw2 | One N cost9 | Need river W first | Peel W then SW (+ cascade peel) |
| t3 `(38,20)→(40,19)` | Mis-keyed | Real: `(39,20)→(40,19)`, `(38,20)→(39,19)` | Fix `(39,20)` peel N→NE |
| t4 `(33,50)→(33,52)` tw2/mv7 | One N | Need river S | Peel S twice |
| t6 `(28,35)→(28,33)` | Mis-keyed | Real: `(27,34)→(28,33)`, `(28,35)→(27,35)` | Fix `(27,34)` peel S→NE |
| t2 Apache/Sioux spent | XY ok, spent 6/9 vs 3 | 465b AL unknown | Residual + park |

`097a` does **not** allow a further act after `spent >= max_mp`. Multi-step is
`cost=1` then another pick while `spent < 3`.

```bash
./build/smoke_ai_turns
AI_EMPIRICISM=1 ./build/smoke_ai_turns
AI_STEP_AUDIT=1 ./build/smoke_ai_turns   # per-step paths
./build/smoke_mapgen_seed100
```

## Spent-only RE note (parked)

TURN2→3 Apache `(45,52)→(46,53)` and Sioux `(49,40)→(49,39)`: dest has no
tribe bit; FROM has presence; `class*3` is 6/9; golden spent=3. Same terrain
class as TURN1 Sioux `(49,41)→(49,40)` which goldens spent=9. No cost-head
predicate distinguishes them without hang `AL=local_40`. From-presence caps
were tried and rejected (break TURN1 spent=9).

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

## Coarse fog (phase 9)

DOS plane `DS:0x9faa` (size `0x10e`): explore `+8` uses `(x>>2)+(y>>2)*18`;
tribe place uses `(y/5)+(x/5)*18`. Linux `s_ai_coarse_fog` mirrors this.

## Empiricism vs DOS quiet

Quiet ASM: `range(1,3)`, river/fa `+1` else `−2f76`, gated facing/fog, **+1 LCG
stay-shaped burn** per pick (stream sync).  
Empiricism: base 200, +4/−6/+3, home, −0x28, +5, stay — still the better match
for several tiles (hence peels until quiet terms catch up).
