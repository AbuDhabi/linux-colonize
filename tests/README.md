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

Examples: `smoke_game_flow`, `smoke_assets`, `smoke_popup`, `smoke_sound`.
Keep these short and low-dependency.

## Unit

Examples: `unit_units`, `unit_turn`, `unit_ai_contact`, `unit_col1_save`.
Exercise one subsystem (or a tight cluster) with concrete expected outcomes.

## Golden

Examples: `golden_mapgen_seed100`, `golden_colony_prod01`/`02`.

**AI goldens partly re-enabled (2026-08-27).** `golden_ai_mid01` and
`golden_ai_late01` pass again after the Indian alarm-store consolidation and
the `5d04` live wire, and are back on as regression gates — they run in a
default `ctest`. Only `golden_ai_turns` and the aggregate `golden_ai_joint`
(which depends on it) are still `DISABLED` in CMake; `golden_ai_turns` fails
on 3 TURN1→2 Braves, the parked seed-100 quiet-pulse divergence
([`docs/seed100_brave.md`](../docs/seed100_brave.md)), not a planner
regression. The parking rationale below still applies to those two: they chase
turn-for-turn DOS parity
against an AI planner that is still only structurally/T0-T1 ported (not T3
1:1); every remaining unported/stubbed callee is a guaranteed future diff,
so a red run there means "AI transcription incomplete", not "regression".
See [`docs/ai_transcription.md`](../docs/ai_transcription.md) R0. Run them
explicitly if you need to look (`ctest -R golden_ai_turns
--force-new-ctest-process`, or `cmake --build build --target
golden_ai_joint`); do not chase them to green piecemeal — re-enable
(`set_tests_properties(... DISABLED FALSE)` in `CMakeLists.txt`) only once
the AI port is actually complete.

Most executables expect **repo root** as cwd (`WORKING_DIRECTORY` in CMake).

Architecture overview: [`docs/architecture.md`](../docs/architecture.md).
