# Debug environment variables

Diagnostic switches read via `getenv()`. None of these are needed for normal
play; they exist for RE/port debugging and golden-test bisection. Most are
**trace-only** (print extra output, change nothing about the simulation);
a few **change behaviour** — marked below — so don't leave them set when
comparing golden output.

| Var | File:line | Kind | Effect | Default |
|-----|-----------|------|--------|---------|
| `AI_SHIP_DOS` | `src/core/ai_euro.c` (`ai_euro_ship_dos_enabled`) | **behaviour** | bugs.md #530 prototype: AI hulls act through the DOS-literal `FUN_521d_5b66` + 20e6 hull path (`ai_euro_act_ship_dos`) instead of the fitted first-colony ship band; also turns off the invented FOUND producers and ports the #528 turn-top clear of act states 1-3. Fails `golden_ai_turns` TURN2+ (RNG alignment) | off |
| `DOS_RNG_TRACE` | `src/core/dos_rng.c:10` | trace | Logs every RNG draw with a running index | off |
| `AI_LCG_AUDIT` | `src/core/ai.c` (`ai_lcg_audit_enabled`) | trace | Logs init-pulse `pick_dir` burn counts (phase 5) | off |
| `AI_INIT_SCHED` | `src/core/ai_brave.c` (`ai_init_sched_apply`) | trace | `"n:idx:count[:R];..."` — init-pulse burn schedule sweep: burn `count` draws before Brave `idx` of nation `n` picks (`idx=-1` = before the pulse; `R` = reseed to the pulse seed first). Setting it disables the default Inca burns. `golden_mapgen_seed100` is the oracle | unset |
| `AI_SCORE_AT` | `src/core/ai.c` (`ai_score_at_match`) | trace | `"n:x:y[,n:x:y...]"` — extra `pick_dir` score-dump targets for the given nation/tile | unset |
| `AI_STEP_AUDIT` | `src/core/ai.c` (`ai_step_audit_enabled`) | trace | Logs mid-turn Brave step paths (phase 13 multi-step) | off |
| `AI_PEEL_AUDIT` | `src/core/ai.c` (`ai_peel_audit_enabled`) | trace | Classifies each firing peel row against both branch scorers | off |
| `COLONIZE_TEST_ONLY` | `tests/common/test_runner.h` | test-only | Table-driven test binaries (see `tests/README.md` "Test runner"): run exactly one case by name | unset (run all) |
| `COLONIZE_TEST_REVERSE` | `tests/common/test_runner.h` | test-only | Run cases in reverse declared order | off |
| `COLONIZE_TEST_SHUFFLE` | `tests/common/test_runner.h` | test-only | `<seed>` — run cases in a deterministic Fisher-Yates shuffle | unset (declared order) |
| `COLONIZE_TEST_LIST` | `tests/common/test_runner.h` | test-only | Print case names, one per line, and exit 0 without running anything | off |
| `AI_NO_BRAVE_PEELS` | `src/core/ai.c` (`ai_brave_peels_disabled`) | **behaviour** | Skips seed-100 dir peels entirely, to audit how many "quiet" misses remain without them | off (peels run) |
| `AI_021A_WATCH` | `src/core/ai_native_021a.c` (`ai_native_pick_dir_021a`) | trace | `"x:y"` — prints that tile's layer2/layer3 at every Brave act | unset |
| `AI_021A_TRACE` | `src/core/ai_native_021a.c` (`ai_021a_trace_enabled`) | trace | One line per Brave act: pick, flags, final direction | off |
| `AI_BRAVE_PICK` | `src/core/ai_brave.c` (`ai_brave_pick_20e6_fallback`) | **behaviour** | `=20e6` falls back to the retired 20e6-shaped quiet scorer instead of the current `021a` picker | current picker |
| `UNITS_FAR_BFS` | `src/core/units.c:9804` | **behaviour** | `=1` reverts to the pre-2026-08-27 whole-map BFS tier for far pathing (diagnostic fallback) | off (tiered BFS) |
| `AI_SET_GOTO_TRACE` | `src/core/ai_euro.c:3768` | trace | Logs `goto` order assignment | off |
| `AI_20E6_LOAD_TRACE` | `src/core/ai_euro.c:4665`, `13757` | trace | Logs cargo/unit load decisions | off |
| `AI_0A60_WORK_TRACE` | `src/core/ai_euro.c:9355` | trace | Logs `0a60` goal-consumption work steps | off |
| `AI_SHIP_TRACE` | `src/core/ai_euro.c:11843` (+5 more sites) | trace | Logs AI ship movement/dispatch decisions | off |
| `AI_4D2E_TRACE` | `src/core/ai_euro.c` (`ai_euro_20e6_wander_step`) | trace | Logs every LAB_521d_4d2e direction score (`[4d2e] u<id> dir d (x,y) score s attack a`) for land units and ships | off |
| `AI_20E6_HOP_TRACE` | `src/core/ai_euro.c:11960` | trace | Logs ship hop/waypoint choice | off |
| `AI_20E6_TREASURE_TRACE` | `src/core/ai_euro.c:12073` | trace | Logs treasure cash-in pickup logic | off |
| `AI_20E6_DEADEND_TRACE` | `src/core/ai_euro.c:12147`, `12235`, `13400` | trace | Logs dead-end/destroy detection for stuck ships | off |
| `AI_20E6_HS_TRACE` | `src/core/ai_euro.c:12463`, `12474` | trace | Logs High Seas transit decisions | off |
| `AI_20E6_DELIVER_TRACE` | `src/core/ai_euro.c:13134`, `13233` | trace | Logs cargo delivery scoring/choice | off |
| `AI_20E6_BOARD_TRACE` | `src/core/ai_euro.c:13610` | trace | Logs unit boarding decisions | off |
| `AI_20E6_SHIP_DUMP_TRACE` | `src/core/ai_euro.c:13838` | trace | Logs AI warehouse dump-sell via ship | off |
| `AI_20E6_SAIL_TRACE` | `src/core/ai_euro.c:16512` | trace | Logs sail/depart decisions | off |
| `AI_6D8E_DOS_LOOP` | `src/core/ai_euro.c:19546` | **behaviour** | `=0` disables the DOS-literal re-act-until-exhausted loop order for the Euro nation-turn dispatch loop | on (`1`, DOS loop order) |
| `AI_5952_INDOOR` | `src/core/ai_euro.c` (`ai_euro_5952_indoor_pass_enabled`) | **behaviour** | `=0` disables the DOS-literal `FUN_5952_035e` indoor-workplace pass (raw 94784-94860) in the AI colony tick and restores the pre-2026-09-17 "leftovers" field stand-in | on (DOS pass) |
| `AI_5952_BUILD_TRACE` | `src/core/ai_euro.c` (`ai_euro_5952_set_unit_project`) | trace | `=1` prints every UNIT project the `FUN_5952_035e` construction cascade picks (Wagon Train / Galleon / Privateer / Frigate / Artillery) with its raw code | off |
| `WOI_DEBUG` | `tests/golden/test_woi_ref01.c:172` | test-only | Prints per-turn WoI status line during `golden_woi_ref01` | off |

`diag_init` also reads `HOME` / `XDG_DATA_HOME` (`src/platform/diagnostics.c:181-182`)
for log-path resolution only — not a debug switch.

## `debug.logs` categories

`debug.logs` is a **settings.json / DEBUG-menu** toggle, not an env var (see
[settings.md](settings.md) "Who wins" and "What `debug.logs` records" for the
full table of tags: `VIEW`, `COMMAND`, `NEWGAME`, `TURN`, `PROD`, `COLONY`,
`CARGO`, `EUROPE`, `ORDER`, `MOVE`, `COMBAT`, `LCR`, `FF`, `POPUP`). Gated by
`diag_set_info_enabled` / `diag_info` in
[`src/platform/diagnostics.c`](../src/platform/diagnostics.c) and set from
`settings.json`'s `debug.logs` key or the DEBUG pulldown
(`diag_set_context` kept current by `game_track_screen` in
[`game_loop.c`](../src/core/game_loop.c)). WARN/ERROR lines always write
regardless of this setting.

## CLI flags

- `--seed <n>` — pins the campaign LCG seed (`src/main.c:75`); overrides
  `settings.json`'s `seed` key for that process. See [settings.md](settings.md).
- `--debug-menu` / `--no-debug-menu` — force the DEBUG pulldown on/off for
  this process (`src/main.c:78-83`), overriding `settings.json`'s
  `debug.menu`. No-op if the binary was built with `COLONIZE_DEBUG_MENU=OFF`.
