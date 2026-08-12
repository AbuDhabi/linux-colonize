# Nation ticks — liberty bells + FF (`FUN_4345_0a22`)

| Item | Value |
|------|-------|
| Lines | **73333–73372** (~40 body; catalog ~144 w/ thunks) |
| Thunk | `FUN_291f_09f8` |
| Caller | `FUN_364b_0688` prologue @57230-31: nation id + bells yield `local_ba` |

Bridge: [`between_turns.md`](between_turns.md). Effects / elect UI: do **not**
duplicate — see pointers below.

## Phases (`0a22`)

| # | Lines | Role |
|---|-------|------|
| 1 | 73340–43 | Accrue `param_2` bells into nation Europe block `*[0x84fc]+0xc` (total) and `+0xe` (last-turn) |
| 2 | 73344–46 | Peacetime (`!(0x5382&1)`) and `next_ff ( +0x12 ) < 0` → Congress nominate `2a1f_0000`→`4345_06d2` |
| 3 | 73347–55 | Wartime independence chrome when bells still below total (set `0x5382` bit4) |
| 4 | 73356–71 | Threshold `4345_0982` via `291f_0f66`: peacetime elect `291f_0fec`→`4345_0342(next)`; else wartime branch; **zero** `+0xc` |

DOS spends/clears the total counter on elect; Linux keeps cumulative bells
(`FUN_4345_0982` via `founding_fathers_bells_needed` — difficulty / human-vs-AI /
year bands / WoI override **Done** thin; zero-on-elect still PARKED).

## Linux

| DOS | Linux | Notes |
|-----|-------|-------|
| Per-colony accrue in `0688` | `turn_run_nation_ticks` sums colony bells/crosses per Euro nation in SETUP | Human + AI (`control!=2`); dock immigrants human-only |
| Idle / pressure crosses | **+2**/turn into `current_crosses` until first dock immigrant; then churches only | Human (AI always +2; spawn PARKED) |
| Needed threshold | Recalc each EOT from **584a** score `(pop+units)<<1+8` (EN ×2/3; AI difficulty scale) | Grows with empire (TURN5 9→TURN6 10) |
| Crosses threshold | Human → dock immigrant when `current > needed`; AI Free Colonist **PARKED** | Human **Done**; AI spawn unpark with golden refresh |

| Debate / elect | `founding_fathers_tick` | **Human first**, then AI Euro (`control==1`); ≤1 elect / nation / call; threshold `4345_0982` **Done** thin |

Sources: [`src/core/turn.c`](../../src/core/turn.c)
`turn_run_nation_ticks`; [`src/core/founding_fathers.c`](../../src/core/founding_fathers.c).

## FF pointers (no effect table here)

| Doc | Use |
|-----|-----|
| [`docs/fandom_col1994.md`](../../docs/fandom_col1994.md) §Founding Fathers | Wiki elect + effect digest |
| [`docs/manual_gap.md`](../../docs/manual_gap.md) | Port fidelity (Congress / KINGGALLEON2 PARK) |
| [`colony_eot_production.md`](colony_eot_production.md) | `0688` callee table (bells row) |
| `founding_fathers.h` | Flow comment (`0a22` / `06d2` / `0342`) |
