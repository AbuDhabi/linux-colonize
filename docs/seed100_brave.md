# Seed-100 Brave / early-AI notes

Durable notes for `smoke_mapgen_seed100` + `smoke_ai_turns` (VR_SEED=100).
Companion status: [ai_transcription.md](ai_transcription.md),
[`original_sources_annotated/ai/move_scoring.md`](../original_sources_annotated/ai/move_scoring.md).

## Status (call-graph annotation + phase 17)

| Gate | State |
|------|--------|
| Init pick (default) | Quiet ASM + stay LCG + 13 peels — **green** |
| Mid-turn pick (default) | Quiet ASM + stay LCG + mid peels + **2** spent residuals — **green** |
| Multi-step / Inca tw | Cleared (phase 13) |
| Spent-only Sioux/Apache | **Post-ADD / after-`465b` writer**. Chrome helpers do **not** write `0x3149` |
| Annotation | `ai/brave_spent_callgraph.md` — `14fe` act, post-ADD → `FUN_1427_*`, 3149 table |
| Port | **Parked** — no T1-safe rule; keep `k_quiet_brave_t2` |
| Hang EXEs | R/F done; **VR_B465X → `dump_b465x3`** named last resort (spent at RETF?) |
| Force empiricism | `AI_EMPIRICISM=1` or `AI_QUIET_ASM=0` (keeps emp residual set) |
| `smoke_mapgen_seed100` / `smoke_ai_turns` | GREEN |

## Quiet mid-turn inventory (phase 13)

| Class | Count | Rows | Notes |
|-------|------:|------|-------|
| Dir-only (scoring) | ~110 | peels | Mid peels `(turn,nation,xy)→dir` |
| Multi-step (cleared) | 0 | — | River-first peels; pulse already loops |
| Mis-keyed overlays (retired) | 0 | was t3/t6 | Wrong unit’s end snapped onto neighbor |
| Spent-only (XY match) | 2 | t2 Apache; t2 Sioux | Post-ADD; keep overlays |

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
| t2 Apache/Sioux spent | XY ok, spent 6/9 vs 3 | post-ADD `0x3149` writer | Residual + park |

`097a` does **not** allow a further act after `spent >= max_mp`. Multi-step is
`cost=1` then another pick while `spent < 3`.

```bash
./build/smoke_ai_turns
AI_EMPIRICISM=1 ./build/smoke_ai_turns
AI_STEP_AUDIT=1 ./build/smoke_ai_turns   # per-step paths
./build/smoke_mapgen_seed100
./build/probe_sioux_spent                # T1/T2 cost-head + neighborhood oracle
```

## Spent-only — dump-free conclusion (phase 17)

TURN2 pulse-start contrast (`tools/probe_sioux_spent`):

| Step | Golden | Linux head | FROM l2 | DEST tribe | Notes |
|------|-------:|-----------:|---------|------------|-------|
| `(49,41)→(49,40)` T1 | 9 | 9 | `01` | no | Agree — must stay 9 |
| `(49,40)→(49,39)` T2 | 3 | 9 | `01` | no | Holdout |
| `(45,52)→(46,53)` T2 | 3 | 6 | `01` | no | Holdout |
| `(47,46)→(48,46)` T2 | 9 | 9 | `01` | no | Control — same presence shape, stays 9 |

Same cost-head shape as the agreeing T1 row (presence on FROM, no tribe/FA/river
pair on DEST). From-presence caps break T1 spent=9 — rejected.

### Existing dumps (re-parsed, no new hangs)

| Dump | Finding |
|------|---------|
| `dump_b465r3` | Sioux ADD **AL=9** (= Linux). Apache already at `(46,53)` spent=3 |
| `dump_b465f3` | Force-max **not entered**; Sioux ends `(49,39)` spent=3. Distrust Apache path on F |
| `dump_vrb465x2` | Broken X: Sioux `(49,40)` **spent=9** without XY commit ⇒ writer **after** ADD chrome / after `465b` return |

**Apache “AL≈3”** (old r3 inference: force stubbed + already spent=3) is **not** proof of a
cost-head mismatch — the same unknown post-ADD writer that turns Sioux 9→3 can turn
Apache 6→3. SAV/DOS terrain → head **6** for Apache SE.

### `0x3149` writers reachable from quiet act (static)

| Site | Role | Quiet Brave? |
|------|------|--------------|
| `465b:05f0` ADD | cost ADD | yes |
| `465b:0628` ocean force | spent=max_mp | **ruled out** (f3) |
| `465b:08f8` → `0934`/`155e` | cargo/wagon + `07be(dest)≥0` | no (type 19; lone stack) |
| `465b:0bd1` act>0x13 → `0934` | anti-spin | no (act=1) |
| `1816` act≥0x15 → `0934` | loop clear | no |
| `14fe` dir==8 → `0934` | stay exhaust | no (XY moves) |
| `1427_155e` via colony `15eb` | recruit | wrong context |

Prime remaining suspect: **conditional `0934`/`155e` after `465b` returns** (or an
unlabeled thunk). Call-site needs hang X.

Post-ADD chrome (`0916`→`12f6`, `0948`→`040c`, `08da`→`0968`, `084e`→`0ce6`,
`07fe`→`0c72`, …) verified **no** `0x3149` write. In-`465b` `0934` paths ruled
out for lone Brave. See
[`original_sources_annotated/ai/brave_spent_callgraph.md`](../original_sources_annotated/ai/brave_spent_callgraph.md).

### Dump-free predicates tried (all reject)

| Predicate | Result |
|-----------|--------|
| Dest ocean-adjacent + cost>max | Breaks T1 Sioux + many cost=6/9 goldens |
| Dest capital `dos_dist≤1` + cost>max | Breaks T1 Sioux `(49,40)` (capital at `(50,40)`) |
| FROM presence / DEST unowned | Same shape as T2 control `(47,46)→(48,46)` which stays 9 |

**Keep** `k_quiet_brave_t2` overlays (port-or-park → **park**). Next evidence:
rebuild **`VR_B465X`** → `dump_b465x3` (spent at `465b` RETF already 3 or still 9?).

`FUN_465b` write map (ASM): friendly land path only **ADD local_40** then
optional ocean force-to-max; post-ADD chrome is not a spent writer for Brave.
**Do not invent Sioux cost-head caps.**

Hang recipes: [`tools/brave_dump/midturn_465b.md`](../tools/brave_dump/midturn_465b.md).

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
