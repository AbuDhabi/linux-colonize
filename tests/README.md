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

Examples: `golden_mapgen_seed100`, `golden_ai_turns`, `golden_ai_mid01`,
`golden_ai_late01`.

Aggregate gate (mapgen + early turns + contact/diplo units + mid/late goldens):

```bash
cmake --build build --target golden_ai_joint
```

Most executables expect **repo root** as cwd (`WORKING_DIRECTORY` in CMake).

Architecture overview: [`docs/architecture.md`](../docs/architecture.md).
