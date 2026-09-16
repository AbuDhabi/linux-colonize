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

Examples: `unit_units`, `unit_turn`, `unit_ai_contact`, `unit_col1_save`.
Exercise one subsystem (or a tight cluster) with concrete expected outcomes.

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
re-runs `golden_mapgen_seed100`, `golden_ai_turns`, `unit_ai_contact`,
`unit_ai_diplo`, `smoke_ai_mid01` and `smoke_ai_late01` in one shot. Registering
it as a test made a plain `ctest` run all six twice, so the `add_test()` was
dropped 2026-09-14 (duplication audit TT-14). Run it explicitly:

```
cmake --build build/debug --target golden_ai_joint
```

Most executables expect **repo root** as cwd (`WORKING_DIRECTORY` in CMake).

Architecture overview: [`docs/architecture.md`](../docs/architecture.md).
