Rules:
1. Keep familiar with the project documentation. Load it when starting work on something. Don't let context compacting/summarization wipe that.
2. Leave `git commit` and `git push` alone. Don't change branches. The user will handle that himself. Work in the folder provided by the user.
3. Be more terse than normal. By all means tell the user what he needs to know, but the user does not need to know every technical detail unless it's actually needed to make executive decisions.
4. When the user tells you to continue autonomously under some conditions, do that, and don't forget it between prompts. Still, apply common sense when to stop.
5. When it will not affect the end result, try to surgically edit a file rather than rewrite the entire thing.

## What this is

Linux port of Sid Meier's Colonization (DOS, 1994), C11 + SDL2, reverse-engineered
from the Ghidra decomp of VICEROY.EXE. Fidelity to DOS behaviour and Col1 save
interop beat "improvements" — see docs/project_goals.md.

## Dev loop

```
make test          # configure (if needed) + build preset debug + ctest --preset debug
make golden        # build + run golden_ai_joint (not in ctest; run when AI/turn code changed)
make build         # build only
cmake --build --preset debug --target unit_ff && ./build/debug/unit_ff   # one test, cwd = repo root
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
| Decomp / DOS behaviour question | docs/original_index.md, tools/address_mapping.csv, original_sources_annotated/MODULE_MAP.md |
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
- Line endings are LF everywhere (`.gitattributes` enforces). Never write CRLF.
- Every new modal popup joins `game_modal_open` (all popups block all sim).
- Gold: read `europe_nation_gold`, write `europe_nation_gold_add` (europe.h). Never
  assign one gold store from the other.
- Slim test targets only: `ai_contact_link_stubs.c` (guarded by `COLONIZE_SLIM_TEST`).
- Big files: `game_loop.c`, `ai_euro.c`, `units.c`, `ai_contact.c` have `Sections:`
  indexes at the top and `/* ===== ... ===== */` banners. Grep the banner, do not
  read the file.
- bugs.md: rows have permanent `#` ids and a `Status` (OPEN / FIXED / CLOSED / REFUTED).
  Agents set FIXED with a one-sentence resolution; the user sets CLOSED. Closed rows
  move to docs/archive/bugs_closed.md.
- Docs over ~800 lines get split or archived. Audit dumps get a `STATUS:` header.
- Port saves in port_saves/ are live player saves: never git-restore or overwrite.
