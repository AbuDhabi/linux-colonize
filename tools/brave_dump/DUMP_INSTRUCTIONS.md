# VR_BRAVE2 / VR_B25 — after `6a09`, before `0aac`

Hangs at the `CALLF FUN_291f_0aac` site in `FUN_75c2` (right after the `03ac` wait loop).

No DS scratch needed — read `unit[12]` at `DS:3294` (x) / `3295` (y) from the Memory dump.

- Still `(10,25)` → mover is `0aac` or later (`0a5c` / `03f4` / …)
- Already `(10,24)` → mover is the `03ac` wait loop (unlikely) or something odd

## Run

`VR_BRAVE2.EXE` or `VR_B25.EXE` → NEW WORLD → freeze → replace `dosbox_save_state_brave/`.

## Phase 9 — init fog / dir at `(47,53)`

**Hang parked.** Coarse fog plane + goldens are the path:

- Far ocean/land: `tools/probe_far_ocean_4753.c` → build target `probe_far_ocean_4753`
- Score / coarse bytes: `AI_QUIET_ASM=1 AI_LCG_AUDIT=1 AI_ASM_STAY_SYNC=1 ./build/golden_mapgen_seed100`
- Notes: [`init_20e6_4753.md`](init_20e6_4753.md) (optional last-resort recipe only)

Spent: [`midturn_465b.md`](midturn_465b.md) — use rebuilt `VR_B465R` (force-max
reloc-safe stub; do **not** use cave `0x3ECD0` — EMS `evm0015`).
