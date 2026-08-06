# Seed-100 Brave / early-AI notes

Working notes for `smoke_mapgen_seed100` + `smoke_ai_turns` (VR_SEED=100).

## Status (phase 9 — coarse fog)

| Gate | State |
|------|--------|
| Default (empiricism) | GREEN |
| `AI_QUIET_ASM=1` | Partial — `(47,53)`→NW fixed via coarse fog; other Brave XY still miss (e.g. `(32,53)`) |
| Far ocean/land vs SAV | **AGREE** (no map bug) |
| Complete Map / Reveal | **Irrelevant** — viewpoint only; `SEED100` ≡ `SEED100_UNREVEALED` on `seen[]` + Apache XY |
| DOS hang campaign | **Parked** (overlay/reloc); not required for W vs NW |

## Coarse fog unlock

DOS plane `DS:0x9faa` (size `0x10e`): explore `+8` uses `(x>>2)+(y>>2)*18`; tribe place uses `(y/5)+(x/5)*18` on the **same** buffer. After `6a09`, far `(43,53)` explore byte is non-zero → no `+8` → quiet picks **NW** (matches golden). Linux `s_ai_coarse_fog` mirrors this; `ai_quiet_fog_explore_ex` gates `+8` on explore-byte `== 0`.

```bash
AI_QUIET_ASM=1 AI_LCG_AUDIT=1 AI_ASM_STAY_SYNC=1 ./build/smoke_mapgen_seed100
# AI_SCORE_DUMP coarse farW=(43,53) explore=01 … unseen=0 → fog8=+0 → best=7 NW
```

Do **not** default-cutover until full init Brave XY is green under quiet ASM.

## Phase 8 — Far-ocean fidelity

```bash
./build/probe_far_ocean_4753
```

| Tile | Role | terr | class | ocean |
|------|------|------|-------|-------|
| `(47,53)` | unit | `0e` | 14 | no |
| `(46,52)` | golden NW dest | `22` | 28 | no |
| `(46,53)` | old ASM W dest | `0c` | 12 | no |
| `(43,49)` | far NW | `19` | 25 | **yes** |
| `(43,53)` | far W | `0a` | 10 | **no** |

Linux `map_generate(seed=100)` terrain bytes **match** `SEED100.SAV` at all probed cells.

## Phase 7 — Per-dir score dump `(47,53)` (pre-coarse-fog)

With all-unseen assume, fog `+8` on dir 6 flipped W over NW. That assume is **wrong** after tribe place; kept for history only.

## Empiricism vs DOS quiet

Quiet ASM: `range(1,3)`, river/fa `+1` else `−2f76`, gated facing/fog (`>>2` unseen).  
Not DOS quiet: base 200, +4/−6/+3, home, −0x28, +5, stay.

## Spent (parallel, hang parked)

`VR_B465R` → AL. See `tools/brave_dump/midturn_465b.md`. Overlay-unsafe; last resort only.
