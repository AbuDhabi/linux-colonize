# Seed-100 Brave / early-AI notes

Working notes for `smoke_mapgen_seed100` + `smoke_ai_turns` (VR_SEED=100).

## Status (phase 5)

| Gate | State |
|------|--------|
| `smoke_mapgen_seed100` | GREEN (empirical restored) |
| `smoke_ai_turns` TURN1→7 | GREEN |
| Brave residual **t2–t6** | **50 rows** — unchanged |
| Quiet ASM + `54f5` | Annotated |
| Quiet Linux cutover | **Blocked** — phase 5 LCG-aligned cutover still red |

## Phase 5 — Init-pulse LCG audit

Run: `AI_LCG_AUDIT=1 ./build/smoke_mapgen_seed100` (stderr).

| Shape | Burns inside pick |
|-------|-------------------|
| **Empirical** | stay `range(0,(tech+1)*4)` + `range(1,5)` × accepted |
| **ASM quiet** | `range(1,3)` × accepted; **no stay** |

Accepted sets match. **Stay surplus = +1 per Brave pick.**

### Init-pulse totals (empirical)

| nation | braves | emp_burns | asm_burns | stay_surplus |
|--------|--------|-----------|-----------|--------------|
| 4 Inca | 6 | 50 | 44 | 6 |
| 5 Aztec | 1 | 8 | 7 | 1 |
| 6 Arawak | 7 | 31 | 24 | 7 |
| 7 | 5 | 36 | 31 | 5 |
| 8 | 6 | 33 | 27 | 6 |
| 9 | 3 | 24 | 21 | 3 |
| 10 | 3 | 21 | 18 | 3 |
| 11 Tupi | 3 | 16 | 13 | 3 |
| **TOTAL** | **34** | **219** | **185** | **34** |

`post_first` Inca=6 / Tupi=1: **does not absorb** stay (wrong timing). Kept unchanged.

### Locked policy (applied in cutover attempt)

No stay; `range(1,3)` per accepted dir; keep `post_first`.

### Cutover result

Gated ASM + that LCG policy → **same** `missing unit … nation=7 at (46,52)`.  
Burn count aligned to ASM (`delta=0` by construction) but **XY still wrong**.

**Finding:** LCG stay surplus was real but **not sufficient**. Next RE: quiet ASM incompleteness vs empiricism-only terms (home-dist / −0x28 / +5 / facing shape), or burns elsewhere in init before Indian pulse.

## Quiet cutover log

| Phase | Result |
|-------|--------|
| 2–4 | formula/fog/gate — Apache miss; reverted |
| 5 | LCG aligned + gated ASM — same Apache miss; reverted |

## Apache / Sioux spent (parallel)

Hang next: `VR_B465R` (`BX==0x1F8`) → AL. See `tools/brave_dump/midturn_465b.md`.

## Smoke

```bash
cmake --build build --target smoke_mapgen_seed100 smoke_ai_turns
AI_LCG_AUDIT=1 ./build/smoke_mapgen_seed100
./build/smoke_mapgen_seed100 && ./build/smoke_ai_turns
```
