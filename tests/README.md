# Tests

Automated checks live under three directories. CMake target and `ctest` names
match the kind prefix.

| Kind | Directory | Target prefix | Meaning |
|------|-----------|---------------|---------|
| **Smoke** | [`smoke/`](smoke/) | `smoke_*` | Thin bring-up: boot, parse, dialog wire-up — “does it catch fire?” |
| **Unit** | [`unit/`](unit/) | `unit_*` | Module behavior suites (often large assertion matrices) |
| **Golden** | [`golden/`](golden/) | `golden_*` | Locked save / multi-turn fidelity gates |

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

## Golden

Examples: `golden_mapgen_seed100`, `golden_colony_prod01`/`02`.

**AI gates are all live (since 2026-09-05).** Nothing in `CMakeLists.txt`
carries a `DISABLED` property. The cluster was parked 2026-08-19 while the AI
transcription was still structural; `smoke_ai_mid01` / `smoke_ai_late01`
(renamed from `golden_ai_*` 2026-09-14, because they generate their own
fixture and assert self-consistency rather than a DOS-derived expectation)
came back 2026-08-27, and `golden_ai_turns` came back 2026-09-05 once all six
TURN steps went green (history in [`docs/port_plan.md`](../docs/port_plan.md)
T1.23 / T3.3).

`golden_ai_joint` is a **build-only convenience target**, not a ctest test: it
re-runs `golden_mapgen_seed100`, `golden_ai_turns`, the three `unit_ai_contact_*` slices,
`unit_ai_diplo`, `smoke_ai_mid01` and `smoke_ai_late01` in one shot. Registering
it as a test made a plain `ctest` run all six twice, so the `add_test()` was
dropped 2026-09-14 (duplication audit TT-14). Run it explicitly:

```
cmake --build build/debug --target golden_ai_joint
```

Most executables expect **repo root** as cwd (`WORKING_DIRECTORY` in CMake).

## Running a single test

Build and run one target directly from repo root (binaries land under
`build/<preset>/`, and must be run with repo root as cwd — same rule as
`ctest`):

```
cmake --build --preset debug --target unit_ff && ./build/debug/unit_ff
```

Swap `unit_ff` for any target name (`smoke_play`, `golden_mapgen_seed100`,
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
`tests/unit/test_ai_euro_expand.c`'s `unit_0a60_work_military_ai_plan_gate`
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
5. Update the bugs.md row status to FIXED.

Shorthand: `make test` runs build + `ctest` in one step, if a root
`Makefile` is present.

Architecture overview: [`docs/architecture.md`](../docs/architecture.md).
