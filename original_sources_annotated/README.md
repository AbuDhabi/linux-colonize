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

**What to peel next** (stable ranked queue — do not re-sort ad hoc):
[`docs/catalog_peel_ranking.md`](../docs/catalog_peel_ranking.md).
Update that file when a batch finishes.

**Layer A (VICEROY) is closed** for segment tagging — all 166 segments labeled.
Live counts are in [`MODULE_MAP.md`](MODULE_MAP.md) **Progress**.

| Metric | Value |
|--------|------:|
| Functions | 2380 |
| Segments | 166 (166 labeled / 0 unknown) |
| Confidence | known 190 · inferred 2190 · unknown 0 |
| Purpose one-liners | 2380 / 2380 |
| System unknown | 0 |

**MAPEDIT is parked** (no new Layer A labels on this track).

Catalog confidence is **not** the same as AI port status in
[`docs/ai_transcription.md`](../docs/ai_transcription.md) — a function can be
light-labeled `inferred` in the catalog while still **unknown** for a 1:1 port.

Purpose one-liners are **not** Layer A. **VICEROY Layer B purposes are closed**
(2380/2380). Further light work is MAPEDIT (parked) or Layer D when a port needs
deep extracts.

### Roadmap (committed order)

1. **Layer A VICEROY** — done (all segments labeled; `205f`/`1d1c` unparked).
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
   - **Done:** Colonizopedia panels `6cb2` (21); menu-bar/pulldown `4b58` (24);
     text/number blit `104b`/`1097`/`1101` (42).
   - **Done:** nation/independence UI `43f7` (21); CUSTOMIZE `733a` + input
     `1262` + discovery `12fd` + BGM `129f`/`2059` (28); mouse INT33 `1a58`
     (18).
   - **Done:** map/pedia draw `15eb` (105 — all purpose-unknown closed);
     platform mid `275d`/`7314`/`124c`/`2047`/`7a05`/`7ada`/`7b08` (56).
   - **Done:** small UI leftovers (68 — blit/cursor/RLE/splash/menu/tips).
   - **Done:** small platform leftovers (59 — timer/LCG/XMS/resource/stream).
   - **Done:** `291f` megaseg thunk bulk (161 — purpose-closed).
   - **Done:** `2a1f` mapgen megaseg thunk bulk (195 — purpose-closed).
   - **Done:** platform megasegs `1d1d`+`210d` (182 — purpose-closed).
   - **Done:** unpark `205f`/`1d1c` (3 — VGA A000 addr + DS:0x26f0 table).
   - **Next:** see [catalog peel ranking](../docs/catalog_peel_ranking.md)
     (VICEROY Layer B closed; MAPEDIT parked / Layer D on demand).
3. **Layer C** — one-hop from known entries.
   - **Done:** `FUN_4d56_1816` (10 callees); `FUN_521d_6d8e` (23 `521d` bodies +
     26 `2a1f` act thunks + 16 helpers).
   - **Done:** `FUN_684c_08c0` (30 non-thunk bodies + 67 `281f`/`291f` thunks).
   - **Done:** turn/EOT neighborhood `FUN_130d_0290` ∪ `FUN_3844_00f2` ∪
     `FUN_3844_0442` (36 callees).
   - **Done:** move-spent `FUN_465b_0000` (26 callees — post-ADD chrome /
     foreign tails).
   - **Done:** map unit-order ∪ colony people-band
     `FUN_2b5a_3b68` ∪ `FUN_2f2b_51ec` (47 thunks).
   - **Done:** colony keyboard/mouse `FUN_2f2b_6372` ∪ `FUN_2f2b_628a` (79);
     Indian raid `FUN_4d56_4528` excl (24); colony sim ticks
     `FUN_5952_035e` ∪ `FUN_364b_0688` excl (19).
   - **Done:** Europe input ∪ colony assign/entry crumbs (19); map menu
     `FUN_2b5a_2464` (23).
   - **Done:** boot `FUN_75c2_2d46` (26); BGM `FUN_129f_0008` (14).
   - **Done:** embark/naval `FUN_4720_049e` (16); title menu `FUN_75c2_2778`
     (15); pedia index `FUN_6cb2_2322` excl (11); RM* `FUN_78ef_0002` excl (10).
   - **Done:** remaining mid ≥8 hops (map-key/Move/new-game/splash/dialog/
     map-refresh, 38) + `281f` megaseg thunks (75).
   - **Done:** `291f` megaseg thunks (161) via Layer B address shards.
   - **Done:** `2a1f` mapgen megaseg thunks (195) via Layer B address shards.
   - **Done:** platform megasegs `1d1d`+`210d` (182) via Layer B address shards.
   - **Done:** unpark `205f`/`1d1c` (3).
   - **Next:** [catalog peel ranking](../docs/catalog_peel_ranking.md) —
     VICEROY light catalog closed; MAPEDIT parked / Layer D on demand.
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
| [`ai/euro_dispatcher.c`](ai/euro_dispatcher.c) | `FUN_521d_6d8e` shell + sectioned `0a60` |
| [`ai/euro_goals.c`](ai/euro_goals.c) | Goal tables + founding helpers (`0000`…`0906`) |
| [`ai/euro_unit_act.md`](ai/euro_unit_act.md) | Thin section-map for `FUN_521d_5b66` |
| [`ai/move_scoring.md`](ai/move_scoring.md) | Quiet cutover; peels; Euro/ocean thin map; 2 spent residuals |

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

## Deep AI status (phase 1 + 2 + 9–17 + Euro early-settle D)

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
  **VR_B465X** remains the named localizer.
- **Euro early-settle Layer D:** `6d8e` thunk wiring corrected (`0554`→`5d04`,
  `0578`→`0342`, `050c`→`0a60`, `0488`→`5b66`); goal helpers in `euro_goals.c`;
  sectioned `0a60`; thin maps for `5b66` + Euro/ocean `20e6`/`06ae`. Full
  `5d04` / `5b66` / land `20e6` mid-planner **OPEN** (unpark #4); ocean/T3 + raid
  deep bodies still PARKED. See `docs/ai_transcription.md` Unparked queue.
