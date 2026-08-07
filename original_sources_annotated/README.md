# Annotated VICEROY sources (phase 1 — AI)

Readable, labeled working copy of selected VICEROY logic extracted from the
raw Ghidra export. **Not compiled** into the Linux binary. **Never edit** the
raw export under [`../original_sources_decompiled/`](../original_sources_decompiled/)
to “fix” names — put renames here instead.

## Purpose

Make AI-critical DOS control flow followable without chasing absolute DS
offsets (`0x3146`, `0x8d4e`, …) and unlabeled `FUN_*` / `func_0x…` symbols.
This is RE evidence for porting ([`src/core/ai.c`](../src/core/ai.c)), not a
second runtime.

## Layout

| Path | Role |
|------|------|
| [`include/viceroy_types.h`](include/viceroy_types.h) | Unit / tribe / map-plane layouts |
| [`include/viceroy_globals.h`](include/viceroy_globals.h) | Named DS addresses used by AI |
| [`ai/accessors.c`](ai/accessors.c) | Map / RNG / move-cost helpers |
| [`ai/move_spent.c`](ai/move_spent.c) | Phase 12: full `FUN_465b_0000` section map |
| [`ai/indian_nation_turn.c`](ai/indian_nation_turn.c) | `FUN_4d56_1816` + apply_step wrapper |
| [`ai/quiet_brave_scoring.c`](ai/quiet_brave_scoring.c) | ASM `LAB_521d_4ea9` quiet Brave scoring |
| [`ai/euro_dispatcher.c`](ai/euro_dispatcher.c) | `FUN_521d_6d8e` shell |
| [`ai/move_scoring.md`](ai/move_scoring.md) | Phase 11–14: quiet cutover; peels; 2 spent residuals (hard stop) |
| [`SYMBOL_MAP.md`](SYMBOL_MAP.md) | Ghidra ↔ annotated ↔ Linux |

## Naming rules

1. Every function keeps a provenance header: `/* Ghidra: FUN_…. | annotated_name */`.
2. Prefer `snake_case` intent names; leave unverified bytes as `unk_*`.
3. Do **not** drop LCG burns that look unused — call order is part of T2 fidelity.
4. Inline only trivial far-call thunks (`FUN_281f_*` → real body); keep large bodies intact.
5. Addresses stay in comments and in `SYMBOL_MAP.md` for hang dumps / docs.

## Sync policy

- **Source of truth for bytes:** `original_sources_decompiled/viceroy_unpacked.c` (+ `.asm`).
- When re-exporting from Ghidra, diff against the raw tree; re-apply annotations here by symbol, not by line number.
- If annotated control flow disagrees with the raw export, the export wins until RE proves otherwise.

## Phase 1 + phase 2 + phase 9–11 status

- Phase 1: AI-critical accessors, Indian nation turn entry, Euro dispatcher shell.
- Phase 2: Quiet Brave `LAB_521d_4ea9` annotated in `ai/quiet_brave_scoring.c`.
- Phase 9: Coarse fog `DS:0x9faa` dual index + Linux buffer.
- Phase 10–11: Seed-100 **init and mid-turn** quiet ASM cutover (stay LCG +
  peels). `AI_EMPIRICISM=1` for legacy.
- Phase 12: `FUN_465b_0000` annotated end-to-end in `ai/move_spent.c` (cost head,
  foreign gate, ocean force-to-max, ADD/gamble; combat PARKED). Linux ocean gate
  uses `euro_settlement_owner`.
- Phase 13: Multi-step / Inca tw cleared via river cost=1 peels (`097a` loop).
- Phase 14–17: Spent-only static RE + dump-free predicates exhausted; quiet
  residuals = **2** Sioux/Apache (post-ADD; hang X last resort). Raid bodies / full
  `20e6` still out of scope.
