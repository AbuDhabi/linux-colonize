# Tests

Automated checks live under three directories. CMake target and `ctest` names
match the kind prefix.

| Kind | Directory | Target prefix | Meaning |
|------|-----------|---------------|---------|
| **Smoke** | [`smoke/`](smoke/) | `smoke_*` | Thin bring-up: boot, parse, dialog wire-up — “does it catch fire?” |
| **Unit** | [`unit/`](unit/) | `unit_*` | Module behavior suites (often large assertion matrices) |
| **Golden** | [`golden/`](golden/) | `golden_*` | Locked save / multi-turn fidelity gates |
| **Regression** | [`regression/`](regression/) + [`../tests-automated-regression/`](../tests-automated-regression/) | `regression_replay` | Recorded play sessions replayed from input logs |

Shared stubs/helpers: [`common/`](common/) (e.g. `platform_cursor_stubs.c`).

## Smoke

Examples: `smoke_game_flow`, `smoke_play` (real assets: new game → landfall → colony → Europe → save), `smoke_assets`, `smoke_popup`, `smoke_sound`.
Keep these short and low-dependency.

## Unit

Examples: `unit_units_core`, `unit_turn_core`, `unit_ai_contact_core`, `unit_col1_save`.
Exercise one subsystem (or a tight cluster) with concrete expected outcomes.

The unit-pool suite is **split by feature** (2026-09-23): `tests/unit/test_units.c`
had grown to 12k lines, so adding one case meant reading all of it. The slices
share only the include set in `tests/unit/test_units_common.h`; each keeps its
own statics and its own `main()`.

| Target | File | Covers |
|--------|------|--------|
| `unit_units_core` | `test_units_core.c` | the original inline `main()` body: new-world start, spawn/stack/move/board/unload, combat + analysis, LCR, colony/village interactions |
| `unit_units_types` | `test_units_types.c` | @UNIT/@JOB catalog rows, display names, kind resolver, promotion/demotion, cap bits, the 2026-09-09 smell audit matrix |
| `unit_units_pioneer` | `test_units_pioneer.c` | clear-forest, used-up tools, pioneer order gates and the case-8 tail |
| `unit_units_ships` | `test_units_ships.c` | sea lanes, amphibious landing, colony docks, Europe arrival, drydock refit, warehouse, King's Galleon offer |
| `unit_units_combat` | `test_units_combat.c` | fort fire + dissolve, combat SFX/music stings, native tile attack + alarm, capture/raze, artillery gates, 1b0e handicaps |
| `unit_units_movement` | `test_units_movement.c` | fog/visibility, flood-fill river pairs, goto tails, wake/select, MP entry gating |

The same treatment was applied 2026-09-23 to the next five oversized suites.
Each slice is its own ctest executable with the original target's sources and
flags; cases moved verbatim (no case added, dropped or reworded), and the
shared include set plus any helper used by more than one slice lives in
`test_<name>_common.h`. The former entangled `main()` bodies were then turned
into named cases: `test_turn_core.c` and `test_units_core.c` use independent
`fx_open()`/`fx_stage*()` fixtures; `test_ai_contact_core.c` and
`test_ai_king_core.c` keep their narratives as ordered spines (`sp_00..sp_59` /
`sp_00..sp_35`) replayed as cumulative prefix cases, so one early regression
fails many cases.

| Original (lines, cases) | Slices |
|---|---|
| `test_ai_contact.c` (7456, 7) | `unit_ai_contact_core` (old narrative kept as an ordered cumulative spine; 59 prefix cases, each replays sp_00..sp_N, so an early regression fails many), `_meet` (first contact, encounter scan, chain order), `_colony` (5952 war tick, prelude alarm band, missionary arm) |
| `test_ai_euro_expand.c` (7089, 57) | `unit_ai_euro_expand_purchase` (5d04 buys), `_haul` (wagon/ship errands, food delivery), `_europe` (exports, privateer loot, treasure), `_labor` (colony labor bind, AI flags), `_build` (construction ladder), `_settle` (Indian-land founding, pioneer timer) |
| `test_turn.c` (6539, 13 + inline `main`) | `unit_turn_core` (60 named cases on an fx_open/fx_stage2/fx_stage3 spine: calendar, production, EOT phases), `_school` (training arms), `_colony` (century cargo-ready, hammers, fog reveal, tools need) |
| `test_ai_euro_war.c` (5381, 42) | `unit_ai_euro_war_core` (merc hire, 5952 labor, peace/goal tails, g-stance), `_naval` (hunts, privateers, ambush), `_land` (adjacent combat + defender ladder), `_garrison` (fortify/quota), `_transport` (unload, war transport) |
| `test_ai_king.c` (4735, 14) | `unit_ai_king_core` (old narrative kept as an ordered cumulative spine; 35 prefix cases + 1 independent `dump_goods_pick_api`), `_war` (war event, non-combat gate, 43f7 spawn types), `_revolution` (end ladder: @LOSING/@WARN/@WINNING/@RETIRING2/@SCORED) |

### Regression guards for FIXED port_bugs.md rows

`tests/unit/test_regress_*.c` (`unit_regress_units`, `_ai_tables`,
`_production`, `_trade_raid`, `_save`) came out of the 2026-09-26 test-gap
audit: 455 FIXED rows in
[`docs/archive/bugs_fixed_pending.md`](../docs/archive/bugs_fixed_pending.md)
were awaiting user verification while only two test cases anywhere named a bug
id, so nothing traced a row to a guard. Cases are named
`case_<bugid>_<what>` and each carries the `FUN_ssss_oooo` / raw offset in a
comment. Add new guards to the slice that matches the subject area (one slice
per area, so concurrent authors do not collide), and mutation-check every new
case: revert the fix in `src/`, confirm the case fails, undo the revert.

Typed-English-fallback regressions (the no-DOS-text rule) are guarded
separately by `lint_no_catalog_text`
([`tests/no_catalog_text.cmake`](no_catalog_text.cmake)), a `strings` scan of
the built binary. Its needle list is per-phrase, so a removed fallback stays
unguarded until its wording is added there. Needles shorter than 5 characters
cannot be caught at all (`strings -n 5`), and generic words (`ship`, `units`,
`Foreign`) false-positive against legitimate strings.
Its source-level twin `lint_dos_text` (`tools/check_dos_text.py`, allowlist
`tools/check_dos_text.allow`) looks every `src/` string literal up in `COLONIZE/*.TXT` and
the `VICEROY.EXE` strings, so a new hit needs a catalog read or an allowlist row with a reason.

## Golden

Examples: `golden_mapgen_seed100`, `golden_colony_prod01`/`02`,
`golden_market_prices01` (full turn, all four nations' ledgers + demand pool;
its two dutch2 exclusions are port_bugs.md #1085). Every exclusion a golden makes
must name its reason in the test and, if it hides an unresolved DOS
difference, hold a port_bugs.md row. A `golden_` test's expectation comes from a
DOS save; a check of port code against port code (or against port-chosen
thresholds) is `smoke_` (`smoke_colony_preview01`, `smoke_woi_ref01`, `smoke_ai_*`).

**AI gates are all live (since 2026-09-05).** No test in `CMakeLists.txt` is
`DISABLED`. The cluster was parked 2026-08-19 while the AI
transcription was still structural; `smoke_ai_mid01` / `smoke_ai_late01`
(renamed from `golden_ai_*` 2026-09-14, because they generate their own
fixture and assert self-consistency rather than a DOS-derived expectation)
came back 2026-08-27, and `golden_ai_turns` came back 2026-09-05 once all six
TURN steps went green (history in [`docs/port_plan.md`](../docs/port_plan.md)
T1.23 / T3.3).

`golden_ai_joint` is a **build-only convenience target**, not a ctest test: it
re-runs `golden_mapgen_seed100`, `golden_ai_turns` (plus the proven #530 DOS
TURN2→3 through TURN6→7 transitions), the three `unit_ai_contact_*` slices,
`unit_ai_diplo`, `smoke_ai_mid01` and `smoke_ai_late01` in one shot. Registering
it as a test made a plain `ctest` run all eight twice, so the `add_test()` was
dropped 2026-09-14 (duplication audit TT-14). Run it explicitly:

```
cmake --build build/debug --target golden_ai_joint
```

The VR_SEED.EXE seed-100 idle campaign gate, CTest `golden_idle_continuous`, plays
`year_1492.sav` continuously through 1599 and compares every archived yearly autosave
in `original_saves/1492-1600-seed-100` byte for byte. It ignores the human's
first-turn UI words, the port extension tail and the DOS-only residue years
1505/1506. Run and debug it, and see what is ignored and why, in
[docs/idle_campaign.md](../docs/idle_campaign.md).
`GOLDEN_IDLE_KEEP_GOING=1` keeps simulating past mismatches (the dump then holds
the last year), for comparing against multi-turn from-load DOSBox runs, and plays
through years the archive lacks. A `%d` in `GOLDEN_IDLE_DUMP` takes the year, so
`1492 1599` with `sim_%d.sav` dumps one continuous run year by year.
`GOLDEN_IDLE_FROM=path.sav` starts from that save instead of `year_FIRST.sav`.
`GOLDEN_IDLE_SAVES=dir` reads the archive's file names from `dir` instead and lifts its last
save (e.g. saves deleted from the repo, restored from git history with `git archive`).
`GOLDEN_IDLE_RELOAD=1` reloads the port save before every turn (CTest `golden_idle_reload`,
`golden_idle_independence_reload`, `golden_idle_america_reload`): the `RTST` chunk must carry what a continuous run does.
`GOLDEN_IDLE_RESYNC=1` (triage, not a gate) restarts every step from the archived DOS save
plus the run's own port tail, so each pair is judged from DOS state;
`GOLDEN_IDLE_RESYNC_SAVE=dir/start_%d.sav` writes those starts for one-pair replays through
`GOLDEN_IDLE_FROM` ([docs/idle_america.md](../docs/idle_america.md) "Resync").

The same binary with a leading `independence` argument runs the second campaign,
`original_saves/independence-seed-100` (Dutch every-turn autosaves `000001`..`000021`,
1680-1690, VR_SOUND.EXE, see its readme.md). Arguments are save numbers, and the `%d` in
dumps is the save number. The player loaded 000001 from the main menu and declared
independence during that turn; before simulating toward save 2 the harness declares through
`ai_king_menu_declare_independence` (with no popup queue that confirms, as the player's second
option did). It plays through turns missing from the archive (turn 288).
CTest `golden_idle_independence` runs `independence 1 21` (the whole archive; save 18 is
simulated but not compared, `known_residue_year`). Per-interval draw comparison against a DOSBox trace: `tools/trcmp.py`. Status and
quirks in [docs/idle_independence.md](../docs/idle_independence.md):

```
GOLDEN_IDLE_KEEP_GOING=1 GOLDEN_IDLE_DUMP=/tmp/x/sim_%d.sav ./build/debug/golden_idle_campaign independence 1 21
```

A leading `america` runs the third campaign, `original_saves/golden_idle_america_seed-100`
(Spanish every-turn autosaves `000001`..`000063`, 1501-1569, Americas map, VR_SOUND.EXE),
the same way. CTest `golden_idle_america` runs `america 1 63` (the whole trimmed archive; save
64 turns on a DOS stack residue). Status in [docs/idle_america.md](../docs/idle_america.md).

A leading `america2` runs the fourth, `original_saves/golden_idle_america_2-seed-100`
(Spanish every-turn autosaves `000001`..`000012`, 1572-1584, recorded after the first Americas
archive's save-64 break). CTest `golden_idle_america2` runs `america2 1 12` (the whole trimmed
archive; save 13 turns on the same DOS stack residue). Status in
[docs/idle_america2.md](../docs/idle_america2.md).

Most executables expect **repo root** as cwd (`WORKING_DIRECTORY` in CMake).

## Input-replay regressions

`regression_replay` (in `ctest`) runs every subdirectory of
`tests-automated-regression/` as one test. A test directory holds:

| File | What |
|------|------|
| `input.sav` | save the test starts from, taken on the main map |
| `input.jsonl` | `debug.input_logger` log (docs/settings.md "Input log") of the test span |
| `output.sav` | save taken on the main map right after the last input |
| `input.steps` | optional: the step script `input.jsonl` was compiled from (see "Authoring from a step script") |

The harness loads `input.sav`, waits for the main map, replays the log one
logged frame per `platform_poll_input` call (nested polls inside
`game_update` — combat analysis, the combat popup pump — eat the same frames
they did live; the game clock follows the logged `t_ms`), then waits until the
game has sat idle on the main map for 60 frames, saves, and requires the save to
equal `output.sav` byte for byte. The whole replay may take at most twice the
log's input span (last `t_ms` − first; floor 1 s), else it fails as a timeout.

Recording one:

1. Launch with `"no_sound": true` (or `--nosound`) and `debug.input_logger` on.
   Music picks reseed the game RNG from the wall clock; the harness warns when
   the session line says `"audio":true`. Do not resize the window.
2. Load the start save, dismiss the load popup, play, end on the main map, save.
3. `input.jsonl` = the log's first (`session`) line plus the records between
   dismissing the load popup and opening the save menu. Copy the loaded save
   as `input.sav`, the new save as `output.sav`.
4. Optional: `python3 tools/squeeze_input_log.py input.jsonl input.jsonl`
   caps idle gaps at 20 frames (the replay costs ~5 ms a frame), then re-run
   the test to confirm it still matches.

A test must stay inside one turn: the harness fails it ("crossed a turn
boundary") when the turn number at the end differs from the loaded one, so AI
and end-of-turn work never moves an expected save.

### Authoring from a step script

`python3 tools/regression_script.py NAME [--bless]` compiles
`tests-automated-regression/NAME/input.steps` (keys, clicks, drags with an
optional Shift, waits; syntax in the script's docstring) into `input.jsonl`
and runs the test with `REGRESSION_SHOTS=1`: the harness writes the screen
before every mouse/key press, plus the start and end screens, to
`build/debug/regression_out/NAME/` (the tool turns them into PNGs) and prints
the map view origin, cursor and selected unit for each, so map tiles can be
hit by pixel. A missing `output.sav` is seeded with `input.sav`, which makes
the harness's field diff list exactly what the steps changed; once that diff
is right, `--bless` copies the run's `actual.sav` to `output.sav`. Start saves
may be fixture-edited through `sav_json` (moves, stock, gold); re-save an
edited start through one no-op replay first so the diff carries no load noise. A fixture that
changes `head.active_unit` must move the map cursor (`stuff.x`/`stuff.y`) to
that unit too, or cursor commands such as B act on the old tile. A no-op
re-save needs a `shot` step (a `wait`-only script compiles to "no steps"); do not
put `shot` between a popup opening and the click that answers it (the click
misses). To prove a test draws no game RNG, compare the RTST `RT_RNG` record
(id 1, version 1, 12 bytes: LCG state, reseed word, DS:0x83a6) of `input.sav`
and `output.sav`: it must not change. `RANGEDBG=1` lists `dos_rng_range` draws,
but cosmetic locally seeded streams (`colony_screen_assign_slot_positions_ex`)
print there too and `dos_rng_next` does not.

On failure the test prints the reason (timeout with phase/frame/status, quit,
load/save error, or the first differing byte plus a `sav_json` field diff) and
leaves `actual.sav`, `expected.json`, `actual.json` in
`build/debug/regression_out/<name>/`. One test:
`make test T=regression_replay CASE=<dir name>`. Each test runs in a forked
child, so module statics never leak between tests.

## Running a single test

Build and run one target directly from repo root (binaries land under
`build/<preset>/`, and must be run with repo root as cwd — same rule as
`ctest`):

```
cmake --build --preset debug --target unit_font_file && ./build/debug/unit_font_file
```

Swap `unit_font_file` for any target name (`smoke_play`, `golden_mapgen_seed100`,
...). `golden_ai_joint` is the one exception: it's a build-only convenience
target, not a `ctest` test (see above), so it must be run explicitly:
`cmake --build build/debug --target golden_ai_joint`.

## Test runner

Many `tests/unit/*.c`, `tests/smoke/*.c` and `tests/golden/*.c` binaries used
to be a hand-written `int main` calling case functions in a fixed sequence —
easy to grow an accidental order dependency (a case that only passes because
an earlier case left some static/global set). `tests/common/test_runner.h`
replaces that boilerplate with a small table-driven harness:

```c
static const TestCase k_cases[] = {
    {"unit_mid_hire_mil", unit_mid_hire_mil},
    {"unit_soldier_board_empty_transport", unit_soldier_board_empty_transport},
};
TEST_MAIN(k_cases)
```

`TEST_MAIN` expands to a full `main()`. Default execution order is always the
declared order, so plain `ctest` behaviour is unchanged. It honours four env
vars (also in [`docs/debug_env_vars.md`](../docs/debug_env_vars.md)):

- `COLONIZE_TEST_LIST=1` — print case names, one per line, exit 0.
- `COLONIZE_TEST_ONLY=<name>` — run exactly one named case.
- `COLONIZE_TEST_REVERSE=1` — run cases in reverse declared order.
- `COLONIZE_TEST_SHUFFLE=<seed>` — deterministic Fisher-Yates shuffle.

Each case is `int (*)(void)`, returning 0 on pass; the runner prints
`PASS <name>` / `FAIL <name>` per case. A file whose main does real work
between/around case calls (loops over a fixture array, env-var-driven
branches, inline assertions not in a separate named function, or a
`check()`/shared-`failures`-counter style — see `tests/common/test_fail.h`)
either keeps its own `main`, or wraps each void case
(`int case_foo(void) { int before = failures; foo(); return failures !=
before; }`) so it still fits the table; not every file in `tests/` uses this
harness.

Variants: `TEST_MAIN_WRAPPED(cases, setup, teardown)` runs statements around
the whole table (e.g. `diag_init(0, NULL)` / `diag_shutdown()`). Single-flow
drivers that keep a hand-written `main()` (`unit_col1_save`,
`unit_founding_fathers`, `unit_ai_indian_census`, the long smokes and
goldens) call `tr_single_flow_gate("<name>")` first, so `COLONIZE_TEST_LIST=1`
prints their one flow name and `COLONIZE_TEST_ONLY=<other>` fails with the
runner's "matched no case" error instead of silently running everything. A
case that cannot run here (no `COLONIZE/` assets) returns `TR_SKIP` (77); the
runner prints `SKIP <name>` and, when every case that ran skipped, exits 77,
which `colonize_add_test` maps to a ctest "Skipped" (`SKIP_RETURN_CODE`).

**Hunting order dependencies**: for a converted binary, run it with
`COLONIZE_TEST_REVERSE=1`, a few `COLONIZE_TEST_SHUFFLE=<seed>` values, and
each case alone via `COLONIZE_TEST_ONLY` (enumerate names with
`COLONIZE_TEST_LIST=1`) — from repo root, same cwd rule as `ctest`. A case
that fails in any of those but passes in declared order depends on state an
earlier case left behind. Two examples found and fixed this way (both were
missing a reset call in the test's own fixture setup, not production bugs):
`tests/unit/test_ai_euro_20e6.c`'s `fixture_init` was missing
`ai_goals_reset()` (ai_goals.c's per-nation `s_goals`/`s_work`/`s_inv`/
`s_plan` statics leaked between cases sharing nation 1), and
`tests/unit/test_ai_euro_expand_labor.c`'s `unit_0a60_work_military_ai_plan_gate`
hand-built its fixture instead of going through `tests/common/ai_fixture.h`'s
`fx_units_init()`, so it skipped `ai_euro_reset()`/`ai_native_reset()`/
`turn_reset()`/`units_reset_state()`/`units_set_occupancy_map(NULL)`.

## Verifying a fix

1. Build: `cmake --build --preset debug`.
2. Run the relevant unit/smoke test(s) directly (see above) for a fast signal.
3. Run the full suite: `ctest --test-dir build/debug`.
4. If the change touches AI or turn code, also run
   `cmake --build build/debug --target golden_ai_joint` (not covered by
   plain `ctest`).
5. Update the port_bugs.md row status to FIXED.

Shorthand: `make test` runs build + `ctest` in one step, if a root
`Makefile` is present.

Architecture overview: [`docs/architecture.md`](../docs/architecture.md).
