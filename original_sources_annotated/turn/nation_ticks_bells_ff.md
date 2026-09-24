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
| 1 | 73340–43 | Accrue `param_2` bells into nation Europe block `*[0x84fc]+0xc` (the live FF **pool**) and `+0xe` (this turn's bells) |
| 2 | 73344–46 | Peacetime (`!(0x5382&1)`) and `next_ff ( +0x12 ) < 0` → Congress nominate `2a1f_0000`→`4345_06d2` |
| 3 | 73347–55 | Wartime independence chrome when bells still below total (latch `0x5382 \|= 4`, i.e. bit **index 2** / mask `0x04` — not "bit4"; the guard is `(0x5382 & 6) == 0`) |
| 4 | 73356–71 | Threshold `4345_0982` via `291f_0f66`: peacetime elect `291f_0fec`→`4345_0342(next)`; WoI + REF absent → `1528`/intervention (`turn.c` + `ai_king_spend_woi_bell_pool`); **zero** `+0xc` |

`+0xc` **is** the pool: it is zeroed outright on a peacetime elect (raw 73370),
on the WoI intervention spend, and at the declaration of independence
(`FUN_43f7_1a26`, raw 74738); surplus over the threshold is discarded. `+0xe`
is this turn's bells only — zeroed before the per-colony loop
(`FUN_3844_00f2`, raw 58382) and never read back by DOS. The Linux side table
`s_ff_bells_since_elect` and its save-time stash were deleted (bugs.md #933);
the port's `liberty_bells_pool` field is literally `+0xc`.

## Linux

| DOS | Linux | Notes |
|-----|-------|-------|
| Per-colony accrue in `0688` | `turn_run_nation_ticks` accrues **per colony** and re-runs the elect test after each one (bugs.md #934) | Human + AI (`control!=2`); dock immigrants human-only; the human's elect test stays in `TURN_PROC_FINISH` (bugs.md #434) |
| Idle / pressure crosses | **+2**/turn into `current_crosses` until first dock immigrant; then churches only | Human (AI always +2; spawn PARKED) |
| Needed threshold | Recalc each EOT from **584a** score `(pop+units)<<1+8` (EN ×2/3; AI difficulty scale) | Grows with empire (TURN5 9→TURN6 10) |
| Crosses threshold | Human → dock immigrant when `current > needed`; AI Free Colonist **PARKED** | Human **Done**; AI spawn unpark with golden refresh |

| Debate / elect | `founding_fathers_tick` / `founding_fathers_tick_human_elect` | AI Euro (`control==1`) here, human in `TURN_PROC_FINISH`; both loop while the pool still clears the next threshold; peacetime only; WoI spend in `turn.c` |
| WoI bell spend | `turn_run_nation_ticks` → `ai_king_spend_woi_bell_pool` | Pool ≥ `0982` → intervention/REF arrival; `founding_fathers_consume_woi_bell_pool`; no FF elect |
| FF candidate pick | `4345_06d2` / `015a` | Century-weighted RNG per `@FATHERS` category (**Done** 2026-08-22) |

Sources: [`src/core/turn.c`](../../src/core/turn.c)
`turn_run_nation_ticks`; [`src/core/founding_fathers.c`](../../src/core/founding_fathers.c).

## FF pointers (no effect table here)

| Doc | Use |
|-----|-----|
| [`docs/fandom_col1994.md`](../../docs/fandom_col1994.md) §Founding Fathers | Wiki elect + effect digest |
| [`docs/manual_gap.md`](../../docs/manual_gap.md) | Port fidelity (Congress; KINGGALLEON2 Done 2026-08-27) |
| [`colony_eot_production.md`](colony_eot_production.md) | `0688` callee table (bells row) |
| `founding_fathers.h` | Flow comment (`0a22` / `06d2` / `0342`) |
