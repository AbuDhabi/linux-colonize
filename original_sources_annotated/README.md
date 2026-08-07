# Annotated VICEROY / MAPEDIT sources

Readable working copy and **light function catalog** for the Ghidra exports.
**Not compiled** into the Linux binary. **Never edit** the raw export under
[`../original_sources_decompiled/`](../original_sources_decompiled/) to “fix”
names — put renames and labels here instead.

## How to look something up

1. **[`MODULE_MAP.md`](MODULE_MAP.md)** — segment prefix → system cluster.
2. **[`FUNCTION_CATALOG.md`](FUNCTION_CATALOG.md)** — every `FUN_*` (line, size,
   one-line purpose or `unknown`).
3. **Deep extracts** (AI today) — files under `ai/` + [`SYMBOL_MAP.md`](SYMBOL_MAP.md).
4. **Raw export** — `viceroy_unpacked.c` / `.asm` or `mapedit.c` when still
   unlabeled. Bytes there always win on control-flow conflicts.

Regenerate the catalog after a Ghidra re-export:

```bash
python3 scripts/gen_fun_catalog.py
```

Human-filled catalog cells and
[`scripts/fun_catalog_seed.json`](../scripts/fun_catalog_seed.json) are merged
by symbol on re-run.

## Light catalog vs deep annotation

| Mode | Artifact | Depth |
|------|----------|-------|
| **Light** | `FUNCTION_CATALOG.md` / `MODULE_MAP.md` | System tag + ≤1 sentence purpose + confidence |
| **Deep** | `ai/*.c`, types/globals, callgraph notes | Provenance headers, renamed locals, control flow |

Default for the whole decomp is **light**. Deepen a cluster only when porting
needs it (peel layer D).

## Catalog peel protocol (one layer per session)

Each session peels **one** thin layer of ignorance. Do not mix layers.

| Layer | Work | Exit criteria |
|-------|------|----------------|
| **A — Segment labeling** | Skim callees/callers + asm strings for the next unlabeled high-def segment; assign a system tag (bulk-apply in seed / catalog) | `MODULE_MAP` row no longer `unknown` |
| **B — String/XREF pass** | `rg` strings in `.asm` for one game area (`SAVEGAME`, `EUROPE`, colony, combat, …); label hit functions | +N `inferred` / `known` purposes |
| **C — Call-tree from known entry** | From one known entry (`FUN_684c_08c0`, `FUN_521d_6d8e`, turn EOT, …), label direct callees one hop | Entry’s 1-hop neighborhood catalogued |
| **D — Selective deepen** | Extract annotated stub into `original_sources_annotated/<system>/` (same bar as `ai/`) | `SYMBOL_MAP` (or sibling map) + catalog links updated |

## Catalog peel status and roadmap

**Layer A (VICEROY) is closed** for segment tagging (batch twelve). Only two
segments stay parked: opaque table lookup `205f` (2 defs) and empty stub
`1d1c` (1 def). Live counts are in [`MODULE_MAP.md`](MODULE_MAP.md)
**Progress**.

| Metric | Value |
|--------|------:|
| Functions | 2380 |
| Segments | 166 (164 labeled / 2 parked unknown) |
| Confidence | known 116 · inferred 2261 · unknown 3 |
| Purpose one-liners | 922 / 2380 |
| System unknown | 3 funcs (`205f`×2 + `1d1c`) |

**MAPEDIT is parked** (no new Layer A labels on this track).

Catalog confidence is **not** the same as AI port status in
[`docs/ai_transcription.md`](../docs/ai_transcription.md) — a function can be
light-labeled `inferred` in the catalog while still **unknown** for a 1:1 port.

Purpose one-liners are **not** Layer A. Mid/high-value Layer B closed for game
logic, map accessors/fog, trade/diplo UI, pathfinding, unit-order UI, map
viewport, and dialog compositor `6f74`. Layer C closed for Euro/Indian AI
entries and NEW WORLD mapgen `FUN_684c_08c0`. Remaining backlog is mostly
thunk/platform/`15eb` mapdraw bulk and leftover dialog widgets (`6cb2`/`4b58`/
`104b`).

### Roadmap (committed order)

1. **Layer A VICEROY** — done (park `205f` / `1d1c` unless new evidence appears).
2. **Layer B** — purpose one-liners from structure / strings / docs.
   - **Done:** SAVEGAME — all `75c2` + `7562` funcs labeled (slot path/list/Save/Load,
     header probe, write/load blobs, title menu, new-game bootstrap). Corrected
     `7562` cluster label (was mis-tagged HoF).
   - **Done:** colony (`2f2b`/`647e`/`479b`); Europe trade `4345` (13/13) +
     `38fd` (81/81); turn chrome `1984` (18/18); combat residual `465b`
     (2/2; `65dd` already closed); status overlays `1009` (15/15).
   - **Done:** turn leftovers `130d` (5/5) + `3844` (3/3) — splash/autosave +
     Euro EOT treasure/ship-ready/year-end chrome.
   - **Done:** Indian diplo/growth `5bfb`/`4cc6`/`15dc`/`41f2`/`4962` (33);
     combat `5fef`/`157e` + remaining `4d56` (23); unit MP chrome `1427` (47);
     colony leftovers `364b`/`5952`/`478c` + Euro landfall `48d3` (27).
   - **Done:** map accessors/fog/mapgen leftovers `137f`/`13e4`/`7455`/`684c`/
     `13f1`/`67f4`/`682a` (37); trade+diplo UI `15b3`/`5f7a`/`3f41` + last
     `521d` (33); path/orders `6662`/`112b`/`49dd` (22); unit-order UI `2b5a`
     (52).
   - **Done:** map viewport `6ba1`/`6a9f`/`6afa`/`6b22`/`6b7e` (42+3 via C);
     dialog compositor `6f74` (57/58 incl. known `1198`).
   - **Next (optional):** dialog widgets `6cb2`/`4b58`/`104b`, or thunks only
     as hops need them. MAPEDIT stays parked.
3. **Layer C** — one-hop from known entries.
   - **Done:** `FUN_4d56_1816` (10 callees); `FUN_521d_6d8e` (23 `521d` bodies +
     26 `2a1f` act thunks + 16 helpers).
   - **Done:** `FUN_684c_08c0` (30 non-thunk bodies + 67 `281f`/`291f` thunks).
   - **Next:** turn EOT neighborhood, or other known entries as ports need them.
4. **Layer D** — selective deep extracts when a port needs them (same bar as
   `ai/`).

AI port roadmap stays in [`docs/ai_transcription.md`](../docs/ai_transcription.md);
the catalog only mirrors light-label status.

## Layout

| Path | Role |
|------|------|
| [`FUNCTION_CATALOG.md`](FUNCTION_CATALOG.md) | All VICEROY + MAPEDIT `FUN_*` (light) |
| [`MODULE_MAP.md`](MODULE_MAP.md) | Segment → system cheat sheet + peel summary |
| [`SYMBOL_MAP.md`](SYMBOL_MAP.md) | Deep AI: Ghidra ↔ annotated ↔ Linux |
| [`include/viceroy_types.h`](include/viceroy_types.h) | Unit / tribe / map-plane layouts |
| [`include/viceroy_globals.h`](include/viceroy_globals.h) | Named DS addresses used by AI |
| [`ai/accessors.c`](ai/accessors.c) | Map / RNG / move-cost helpers |
| [`ai/move_spent.c`](ai/move_spent.c) | `FUN_465b_0000` cost / ADD / post-ADD chrome |
| [`ai/unit_mp.c`](ai/unit_mp.c) | Real `FUN_1427_*` MP / stack chrome behind `281f` thunks |
| [`ai/indian_nation_turn.c`](ai/indian_nation_turn.c) | `FUN_4d56_1816` + `14fe` act |
| [`ai/quiet_brave_scoring.c`](ai/quiet_brave_scoring.c) | ASM `LAB_521d_4ea9` quiet Brave scoring |
| [`ai/brave_spent_callgraph.md`](ai/brave_spent_callgraph.md) | Quiet spent `0x3149` call graph + hang X target |
| [`ai/euro_dispatcher.c`](ai/euro_dispatcher.c) | `FUN_521d_6d8e` shell |
| [`ai/move_scoring.md`](ai/move_scoring.md) | Quiet cutover; peels; 2 spent residuals |

## Deep naming rules (extracted `.c` only)

1. Every function keeps a provenance header: `/* Ghidra: FUN_…. | annotated_name */`.
2. Prefer `snake_case` intent names; leave unverified bytes as `unk_*`.
3. Do **not** drop LCG burns that look unused — call order is part of T2 fidelity.
4. Inline only trivial far-call thunks (`FUN_281f_*` → real body); keep large bodies intact.
5. Addresses stay in comments and in `SYMBOL_MAP.md` for hang dumps / docs.

## Sync policy

- **Source of truth for bytes:** `original_sources_decompiled/viceroy_unpacked.c`
  (+ `.asm`) and `mapedit.c`.
- When re-exporting from Ghidra, re-run `gen_fun_catalog.py`; re-apply deep
  annotations by symbol, not by line number.
- If annotated control flow disagrees with the raw export, the export wins until
  RE proves otherwise.
- Catalog labels (`inferred`) are **not** port fidelity claims.

## Deep AI status (phase 1 + 2 + 9–17)

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
  residuals = **2** Sioux/Apache (post-ADD; hang X last resort).
- Call-graph annotation: `14fe` act resolved; post-ADD chrome → `FUN_1427_*`
  with `0x3149` R/W table — chrome does not write spent for Brave; hang
  **VR_B465X** remains the named localizer. Raid bodies / full `20e6` still
  out of scope.
