# Debug environment variables

Diagnostic switches read via `getenv()`. None of these are needed for normal
play; they exist for RE/port debugging and golden-test bisection. Most are
**trace-only** (print extra output, change nothing about the simulation);
a few **change behaviour** — marked below — so don't leave them set when
comparing golden output.

| Var | File (function) | Kind | Effect | Default |
|-----|-----------|------|--------|---------|
| `DOS_RNG_TRACE` | `src/core/dos_rng.c` (`dos_rng_trace_enabled`) | trace | Logs every RNG draw with a running index | off |
| `RANGEDBG` / `RANGEBT` | `src/core/dos_rng.c` (`dos_rng_range`) | trace | `RANGEDBG=1` prints every draw as `RANGE lo hi draw N state S` (N = `dos_rng_draw_count`, S = the LCG state before the draw); `RANGEBT=1` adds a 5-frame backtrace (glibc-only, absent in the mingw build; resolve with `addr2line -f -e build/debug/<bin>`). Consumed by `tools/trcmp.py`. | off |
| `AI_LCG_AUDIT` | `src/core/ai.c` (`ai_lcg_audit_enabled`) | trace | Logs init-pulse `pick_dir` burn counts (phase 5) | off |
| `AI_INIT_SCHED` | `src/core/ai_brave.c` (`ai_init_sched_apply`) | **behaviour** | `"n:idx:count[:R];..."` — init-pulse burn schedule sweep: burn `count` draws before Brave `idx` of nation `n` picks (`idx=-1` = before the pulse; `R` = reseed to the pulse seed first). Normal play adds no draws. `golden_mapgen_seed100` is the oracle | unset |
| `AI_SCORE_AT` | `src/core/ai.c` (`ai_score_at_match`) | trace | `"n:x:y[,n:x:y...]"` — extra `pick_dir` score-dump targets for the given nation/tile | unset |
| `AI_STEP_AUDIT` | `src/core/ai.c` (`ai_step_audit_enabled`) | trace | Logs mid-turn Brave step paths (phase 13 multi-step) | off |
| `AI_021A_WATCH` | `src/core/ai_native_021a.c` (`ai_native_pick_dir_021a`) | trace | `"x:y"` — prints that tile's layer2/layer3 at every Brave act | unset |
| `AI_021A_TRACE` | `src/core/ai_native_021a.c` (`ai_021a_trace_enabled`) | trace | One line per Brave act: pick, flags, final direction | off |
| `UNITS_FAR_BFS` | `src/core/units_move.c` (`units_goto_director`) | **behaviour** | `=1` reverts to the pre-2026-08-27 whole-map BFS tier for far pathing (diagnostic fallback) | off (tiered BFS) |
| `COLONIZE_UNITS_STRICT` | `src/core/units_map.c` (`units_strict_init`, constructor) | **behaviour** | `=1` makes the pool allocator / `units_get` verify slot bookkeeping and `abort()` on a pool filled by hand without `units_pool_sync`. Checks only; sim output unchanged. CMake sets it for every ctest run | off (set by ctest) |
| `AI_6D8E_DOS_LOOP` | `src/core/ai_euro.c` (`ai_euro_dispatcher_turn_unit_waves`) | **behaviour** | `=0` disables the DOS-literal re-act-until-exhausted loop order for the Euro nation-turn dispatch loop | on (`1`, DOS loop order) |
| `PICKCTX` | `src/core/ai_euro.c` (`ai_euro_dispatcher_turn_unit_waves`) | trace | `=1` prints each Euro unit act with the DOS RNG draw count of the current golden run (`dos_rng_draw_count`), for placing music-pick reseeds | off |
| `AI_SET_GOTO_TRACE` | `src/core/ai_euro_expand.c` (`ai_euro_set_goto`) | trace | Logs `goto` order assignment | off |
| `AI_20E6_LOAD_TRACE` | `src/core/ai_euro_expand.c` (`ai_euro_try_wagon_haul`), `src/core/ai_euro_ship.c` (`ai_euro_20e6_load_holds`) | trace | Logs cargo/unit load decisions | off |
| `AI_0A60_WORK_TRACE` | `src/core/ai_euro_goals.c` (`ai_euro_colony_goals_colony_work`) | trace | Logs `0a60` goal-consumption work steps | off |
| `AI_SHIP_TRACE` | `src/core/ai_euro_act.c` (`ai_euro_20e6_ship_dos`), `src/core/ai_euro_land.c` (`ai_euro_20e6_wander_step`, `ai_euro_20e6_ship_far_roam`), `src/core/ai_euro_expand.c` (`ai_euro_4393_work_queue_haul_pick`) | trace | Logs AI ship movement/dispatch decisions | off |
| `AI_4D2E_TRACE` | `src/core/ai_euro_land.c` (`ai_euro_20e6_wander_step`, `ai_euro_20e6_attack_term`) | trace | Logs every LAB_521d_4d2e direction score (`[4d2e] u<id> dir d (x,y) score s attack a`) for land units and ships, plus the `[52aa]` attack-odds line | off |
| `AI_20E6_HOP_TRACE` | `src/core/ai_euro_land.c` (`ai_euro_20e6_ring_hop`) | trace | Logs ship hop/waypoint choice | off |
| `AI_20E6_TREASURE_TRACE` | `src/core/ai_euro_land.c` (`ai_euro_20e6_treasure_cash_in`) | trace | Logs treasure cash-in pickup logic | off |
| `AI_20E6_TREASURE_CASH` | `src/core/ai_euro_land.c` (`ai_euro_20e6_treasure_cash_enabled`) | **behaviour** | `=0` kill switch for the DOS treasure cash-in at any own colony (FUN_521d_20e6 47b9 pass); read via `ai_euro_env_flag` (set and not `"0"` = on) | on (`1`) |
| `AI_20E6_DEADEND_TRACE` | `src/core/ai_euro_land.c` (`ai_euro_20e6_47b9_dead_end`, `ai_euro_20e6_wagon_origin_walk`), `src/core/ai_euro_ship.c` (`ai_euro_20e6_wagon_village_errand`) | trace | Logs dead-end/destroy detection for stuck ships and errand wagons | off |
| `AI_20E6_HS_TRACE` | `src/core/ai_euro_land.c` (`ai_euro_20e6_457e_hs_cadence_snapshot`, `ai_euro_20e6_europe_dock_demand`) | trace | Logs High Seas transit decisions | off |
| `AI_20E6_HS_CADENCE` | `src/core/ai_euro_land.c` (`ai_euro_20e6_hs_cadence_enabled`) | **behaviour** | `=0` kill switch for the wired 457e high-seas cadence (sail-home spiral); read via `ai_euro_env_flag` | on (`1`) |
| `AI_20E6_DELIVER_TRACE` | `src/core/ai_euro_ship.c` (`ai_euro_20e6_delivery_colony_pick`, `ai_euro_20e6_delivery_sell_tail`) | trace | Logs cargo delivery scoring/choice | off |
| `AI_20E6_BOARD_TRACE` | `src/core/ai_euro_ship.c` (`ai_euro_20e6_transport_assemble`) | trace | Logs unit boarding decisions | off |
| `AI_20E6_SHIP_DUMP_TRACE` | `src/core/ai_euro_ship.c` (`ai_euro_20e6_ship_berth_arrival`) | trace | Logs the ship-at-own-colony hold dump (`[shipdump]`) | off |
| `AI_20E6_SHIP_DUMP` | `src/core/ai_euro_ship.c` (`ai_euro_20e6_ship_dump_enabled`) | **behaviour** | `=0` kill switch: a ship at an own colony stops emptying its holds on arrival (DOS md:2996-3017 dump sweep) and the 06e empty-hull gate substitution returns; read via `ai_euro_env_flag` | on (`1`) |
| `AI_5952_BUILD_TRACE` | `src/core/ai_euro_colony_jobs.c` (`ai_euro_5952_set_unit_project`) | trace | `=1` prints every UNIT project the `FUN_5952_035e` construction cascade picks (Wagon Train / Galleon / Privateer / Frigate / Artillery) with its raw code | off |
| `AI_28C8_TRACE` | `src/core/ai_euro_colony_jobs.c` (`ai_euro_28c8_score_full`, `ai_euro_5952_indoor_pass`, `ai_euro_colony_tick_run`) | trace | `=1` prints every 28c8 job search (colony, slot, restriction, winning job/plot, yield, score), the 5952 indoor election (per-job scores, field score, winner, local_1b0 rank byte, the four 00f8 ranks, human, turn), every `[rank refresh]` (turn.c) with its `[rank in]` score components (turn_colony.c) and every `[census]` call (col1_stuff_census.c) | off |
| `AI_28C8_CAND` | `src/core/ai_euro_colony_jobs.c` (`ai_euro_28c8_score_full`) | trace | `=1` prints every scored 28c8 candidate (plot, job, yield, score, modelled local_32 residue) | off |
| `COLONIZE_TEST_ONLY` | `tests/common/test_runner.h` (`tr_run_main`, `tr_single_flow_gate`) | test-only | Run exactly one case by name (table-driven binaries) or gate a single-flow driver on its flow name; `COLONIZE_TEST_LIST=1` lists names. A test that cannot run (no `COLONIZE/`) exits `TR_SKIP` (77 = ctest SKIP) | unset (run all) |
| `COLONIZE_TEST_REVERSE` | `tests/common/test_runner.h` (`tr_run_main`) | test-only | Run cases in reverse declared order | off |
| `COLONIZE_TEST_SHUFFLE` | `tests/common/test_runner.h` (`tr_run_main`) | test-only | `<seed>` — run cases in a deterministic Fisher-Yates shuffle | unset (declared order) |
| `COLONIZE_TEST_LIST` | `tests/common/test_runner.h` (`tr_run_main`) | test-only | Print case names, one per line, and exit 0 without running anything | off |
| `GOLDEN_IDLE_FROM` | `tests/golden/test_idle_campaign.c` (`main`) | test-only | `=path.sav` — start the idle campaign from that save instead of `year_FIRST.sav` (e.g. a DOSBox from-load autosave) | unset |
| `GOLDEN_IDLE_KEEP_GOING` | `tests/golden/test_idle_campaign.c` (`main`) | test-only | Any non-empty value: keep simulating past a mismatch (multi-turn from-load DOSBox targets; plays years the archive lacks). The dump then holds the last year | off |
| `GOLDEN_IDLE_DUMP` | `tests/golden/test_idle_campaign.c` (`compare_save`) | test-only | Path to write the simulated save to on mismatch (or always with `GOLDEN_IDLE_DUMP_ALL`); a `%d` takes the year, one file per turn under `KEEP_GOING` | unset |
| `GOLDEN_IDLE_DUMP_ALL` | `tests/golden/test_idle_campaign.c` (`compare_save`) | test-only | Any value: write the `GOLDEN_IDLE_DUMP` file every year, not just on mismatch | off |
| `WOI_DEBUG` | `tests/golden/test_woi_ref01.c` (`main`) | test-only | Prints per-turn WoI status line during `golden_woi_ref01` | off |

`diag_init` also reads `HOME` / `XDG_DATA_HOME` (`src/platform/diagnostics.c`, `diag_init`)
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

- `--seed <n>` — pins the campaign LCG seed (`src/main.c`, `parse_args`); overrides
  `settings.json`'s `seed` key for that process. See [settings.md](settings.md).
- `--debug-menu` / `--no-debug-menu` — force the DEBUG pulldown on/off for
  this process (`src/main.c`, `parse_args`), overriding `settings.json`'s
  `debug.menu`. No-op if the binary was built with `COLONIZE_DEBUG_MENU=OFF`.
