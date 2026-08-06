# Seed-100 Brave / early-AI notes

Working notes for `smoke_mapgen_seed100` + `smoke_ai_turns` (VR_SEED=100).

## Status (R0 partial + phase 2/3 annotate)

| Gate | State |
|------|--------|
| `smoke_mapgen_seed100` | GREEN |
| `smoke_ai_turns` TURN1→7 | GREEN |
| Brave residual **t1** | **empty** |
| Brave residual **t2–t6** | **50 rows** (5/9/14/9/13) — unchanged (cutover not landed) |
| Quiet ASM + fog annotate | **Done** — `original_sources_annotated/ai/quiet_brave_scoring.c` |
| Quiet Linux cutover | **Blocked** — phase 2 and phase 3 attempts reverted |

## Quiet cutover log

- **Phase 2:** base `range(1,3)` + `−2f76`/`+1` + facing `−diff²×2` (no fog) → both smokes fail.
- **Phase 3:** same + Indian fog (`+8` far unseen, `−2` presence; Euro `+2` skipped) → still fails (e.g. Apache unit missing at expected XY on mapgen). Empiricism restored.

Next RE before retry: `LAB_521d_54f5` entry conditions, military-neighbor −10 path, coarse fog `−0x6056` / `0682` bit0 fidelity. Do not mix ASM terms onto empirical base-200.

### Intended ASM LCG (when cutover lands)

Per scored dir 0..7: one `range(1,3)`. Rejected dirs burn nothing. Stay outside loop.

## Scoring / cost ports that emptied t1

- Terrain river cost-1 (`072c` &0x40 + cardinal)
- Tribe-tile spend cap only (`06be` = layer2&2 + owner via `137f_03e4`)
- Own-nation −0x28 (skip river-into-tribe)
- Arawak (48,15) home-dist thr `>1`; Inca (8,33) burn roll / no-add
- `ai_mask_fa_flags`; ocean-transition spent=max
- Empirical facing +4/−6/+3 still load-bearing in Linux

## Full quiet `521d:4ea9` (annotated; Linux not cut over)

Brave type 19 flags `0x38`:

- Base `range(1,3)`; river/fa `+1` else `−2f76[terr]`; facing `−diff²×2`
- Fog: `+8` coarse-unseen far; Indians skip Euro `+2` explore; `−2` presence
- Colony-pull no-op early

## Apache / Sioux spent (still open)

- Apache T2: XY OK via tile bridge; spent 6 vs golden 3
- Sioux T2: XY OK; spent 9 vs golden 3 — `465b` ADD1 hang-dump still open

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
