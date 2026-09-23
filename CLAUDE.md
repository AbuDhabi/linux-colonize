Rules:
1. Keep familiar with the project documentation. Load it when starting work on something. Don't let context compacting/summarization wipe that.
2. Leave `git commit` and `git push` alone. Don't change branches. The user will handle that himself. Work in the folder provided by the user.
3. Be more terse than normal. By all means tell the user what he needs to know, but the user does not need to know every technical detail unless it's actually needed to make executive decisions.
4. When the user tells you to continue autonomously under some conditions, do that, and don't forget it between prompts. Still, apply common sense when to stop.
5. When it will not affect the end result, try to surgically edit a file rather than rewrite the entire thing.
6. When using high-level, expensive models (Fable, Opus) use lesser subagents where it will save on usage credits.

## What this is

Linux port of Sid Meier's Colonization (DOS, 1994), C11 + SDL2, reverse-engineered
from the Ghidra decomp of VICEROY.EXE. Fidelity to DOS behaviour and Col1 save
interop beat "improvements" — see docs/project_goals.md.

## Dev loop

```
make test          # configure (if needed) + build preset debug + ctest --preset debug
make golden        # build + run golden_ai_joint (not in ctest; run when AI/turn code changed)
make build         # build only
make test T=unit_ff [CASE=name]   # one test target (cheapest; prefer over full ctest)
./build/debug/colonize_linux --data-dir COLONIZE
```

Canonical build dir is `build/debug`. `build-*/`, `deps-*/`, `dist/` are release /
cross-build artefacts; never build or test there. Tests expect repo root as cwd.

## Read before working

| Task type | Read first |
|-----------|-----------|
| Anything | docs/conventions.md (jargon, code rules, fixture traps, evidence hierarchy, verify loop) |
| Where does code / a doc live, who owns what | docs/architecture.md (Authority table + layer map) |
| Bug from bugs.md | bugs.md row by `#` id; docs/manual_gap.md for the feature; docs/<feature>.md |
| Decomp / DOS behaviour question | `python3 tools/decomp_fn.py FUN_ssss_oooo` (one body, cheap), docs/original_index.md, original_sources_annotated/MODULE_MAP.md |
| AI (Euro / Indian / King) | docs/port_plan.md, docs/ai_euro_logic_map.yaml (+ tools/ai_logic_map.py check) |
| Tests / fixtures | tests/README.md, tests/common/ai_fixture.h |
| Debugging env vars, trace switches, debug.logs | docs/debug_env_vars.md |
| Tools and scripts | tools/README.md |

Docs are authoritative over auto-memory notes. Historical audit dumps live in
docs/archive/ (indexed by docs/archive/README.md) and docs/smell_audit_*.md; they are
not specs.

## Conventions (short form; full text in docs/conventions.md)

- Source of truth order: user-observed DOS behaviour > decomp/ndisasm static read >
  golden fixtures > memory notes. Never invent a constant; find it in the decomp.
- `/* DOS-LITERAL FUN_ssss_oooo raw NNNNN */` marks a verbatim port. Do not "improve" it.
  Cite `FUN_ssss_oooo` (Ghidra name) and raw EXE offset in comments when porting.
- `_id` = stable identifier, `_index` = 0-based array slot. Do not mix.
- No MicroProse text in the binary: wording comes only from `COLONIZE/*.TXT` (miss =
  empty string, never a typed fallback); identify units/buildings/jobs/menu items by
  catalog ROW (`units_kind_type_index`, `colonies_building_row`), never by English name.
  See docs/data_vs_hardcoded.md Part D.
- Line endings are LF everywhere (`.gitattributes` enforces). Never write CRLF.
- Every new modal popup joins `game_modal_open` (all popups block all sim).
- Gold: read `europe_nation_gold`, write `europe_nation_gold_add` (europe.h). Never
  assign one gold store from the other.
- Layering: `colonize_sim` (units/colony/turn/ai/save/map + shared text helpers) must
  not call `colonize_ui` (fb/font/screens/dialogs/renderers). `colonize_sim_linkcheck`
  fails the build if it does. Sim talks to UI only via ai_popup queue + hooks.
- Stage functions of split DOS bodies are declared in `*_internal.h` (COLONIZE_INTERNAL);
  tests may call them directly. Cross-stage DOS locals live in the ctx struct and must be
  written back on every stage exit (docs/conventions.md "Ctx write-back trap").
- Slim test targets only: `ai_contact_link_stubs.c` (guarded by `COLONIZE_SLIM_TEST`).
- Big files have `Sections:` indexes at the top and `/* ===== ... ===== */` banners.
  Grep the banner, Read only that range. Split families (cross-file seams in the
  matching `*_internal.h`): `ai_euro_*`, `game_loop_*`, `units_*`, `ai_contact_*`, `ai_king_*`,
  `europe_*`, `ai_*` (ai_indian/ai_brave/ai_native_*), `colony_*`, `colony_screen_*`, `reports_*`, `turn_*`;
  docs/architecture.md names what each file holds. Header design prose lives in
  docs/colony.md, docs/europe.md, docs/units.md (headers keep one-line pointers).
  Tests are split the same way (`test_units_*.c`, `test_ai_euro_expand_*.c`, ...);
  run one with `make test T=unit_units_core CASE=<case>`.
- Concurrent agents: give each a private subdirectory of the scratchpad (shared
  scratchpad files get overwritten) and disjoint source files.
- bugs.md: rows have permanent `#` ids and a `Status` (OPEN / FIXED / CLOSED / REFUTED).
  bugs.md holds OPEN rows only. Agents set FIXED (short resolution) and move the row to
  docs/archive/bugs_fixed_pending.md; the user sets CLOSED and moves it to
  docs/archive/bugs_closed.md. Grep a row by `| NNN |`, never Read the whole archive.
- Docs over ~800 lines get split or archived. Audit dumps get a `STATUS:` header.
- Port saves in port_saves/ are live player saves: never git-restore or overwrite.
