# Whole-Project Port Plan — Sequential Work Queue

A living, ordered queue for finishing the **entire** Linux Colonization port —
the cross-subsystem companion to [ai_port_plan.md](ai_port_plan.md) (which
stays the authoritative queue for AI transcription work and is *referenced*
from here, not duplicated). This file owns **whole-project sequencing**: which
non-AI (or cross-cutting) item an agent should pick up next and why.

It does not own status detail. Owners:

| Detail | Owner |
|--------|-------|
| Phase order / exit criteria | [roadmap.md](roadmap.md) |
| Feature Done/Partial/Missing | [manual_gap.md](manual_gap.md) |
| AI FUN inventory + fidelity claims | [ai_transcription.md](ai_transcription.md) |
| AI sequenced queue | [ai_port_plan.md](ai_port_plan.md) |
| Unresolved-meaning fields/functions | [mysteries_catalog.md](mysteries_catalog.md) |
| Architecture constraints | [architecture.md](architecture.md) |
| Fidelity bar / conflict order | [project_goals.md](project_goals.md) |

## How an agent should use this file

Same contract as `ai_port_plan.md` (read its "How an agent should use this
file" + "Method notes" sections — they apply verbatim here, including: read
raw decomp before trusting summaries; check `address_mapping.csv` /
`viceroy_globals.h` / existing `dosbox-x-dumps/*` before filing anything as
live-capture-blocked; never invent a constant; never `git commit`/`push`;
run **full** `ctest` before calling anything done; update the owning status
doc when a slice lands).

1. Read this file, [roadmap.md](roadmap.md), and the owning doc of whatever
   you touch (CLAUDE.md rule 1).
2. Default rule: **if the AI queue has open Tier-1 work, that track and this
   file's Tier 1 are equal-priority peers** — pick whichever the user asked
   about, or whichever unblocks a roadmap phase exit criterion. AI items are
   never worked from this file directly; go through `ai_port_plan.md`.
3. Tier numbering mirrors `ai_port_plan.md`: Tier 1 fully agent-autonomous,
   Tier 2 verification legwork, Tier 3 user-confirm switch flips, Tier 4
   needs the user's live DOSBox-X session, Tier 5 polish (last, per
   [project_goals.md](project_goals.md) acceptance order).
4. Check items off in place with a one-line dated result; add new discoveries
   to the tier matching their actual blocker.

---

## Tier 1 — Static RE + port (fully agent-autonomous)

- [ ] **W1.1 — AI transcription (the largest track).** Work
  [ai_port_plan.md](ai_port_plan.md) top to bottom. Open there as of
  2026-08-24 (later same day): **T1.8** (`0015bc`'s edge-cost formula now
  wired; `0015c1`/`0009ae` decompile clean and `000000` hand-transcribed
  but none byte-exact-ported yet, plus the still-unwired fort/colony `+8`
  mask-bit `0x40` — deprioritized, working substitute ships), **T1.13**
  (KINGGALLEON2, PARKED pending a narrower `38fd`-overlay
  hint), **T1.15** (`152e` worth-cap thunk `2a1f:0410` overlay-id — Ghidra's
  `41f2_0294` label is a misresolve). This row is done when that file's
  Tier 1 is empty.

- [x] **W1.2 — `@TOONEAR` colony founding-distance gate — closed
  2026-08-24.** Static trace succeeded (no DOSBox repro needed, `W4.1`
  stays unused). The map-key `Build Colony` dispatch is **not**
  `FUN_2b5a_3252` (that's the numpad/arrow-key movement dispatcher, a
  wrong lead already baked into a since-corrected code comment) — the real
  handler is `FUN_2b5a_1662`/`16ce`, an undocumented `FUNCTION_CATALOG.md`
  gap between `FUN_2b5a_1454` and `FUN_2b5a_199e`, found via
  `tools/GhidraListXRefs.java` on the resident thunk `FUN_291f_01fa`
  (→ `FUN_479b_076e`, the found-colony body) against the `OvlWork/Ovl`
  overlay Ghidra project — the canonical `viceroy_unpacked.c` export of
  this address range is corrupted (jumptable/EMS-mapping garbage). That
  handler calls `FUN_1000_8804` → `FUN_15eb_0142`/`FUN_0000_5ff2` (nearest
  colony, any nation/type) and bounces when the `FUN_0000_2500` distance
  metric equals 1 (exactly the 8 Chebyshev-adjacent tiles). This
  **confirms** the `dx<=1 && dy<=1` gate already shipped 2026-08-14
  (`3abe4c4`) is byte-faithful, not invented — only its code-comment
  citation was wrong (fixed this pass). AI founding already inherits it
  (`ai_euro_found_with_unit` + every AI found-tile call site gate through
  the same `colonies_can_found`) and `ai_goals_pick_founding_tile_ex`
  (the "second-wave" picker) already filters through `colonies_can_found`
  too — no separate AI-side wiring needed. New `unit_colonies` regression
  test locks in the adjacency case distinctly from the occupied-tile case.
  Full trace: [manual_gap.md](manual_gap.md) "Found colony" row.

- [ ] **W1.3 — Production / EOT formula fidelity.** The economy loop runs
  end-to-end but several formulas are DOS-unconfirmed:
  - Manufacturing tier rates + class scale
    ([building_production.md](building_production.md)).
  - [terrain_yields.md](terrain_yields.md) "Still open": Farmer/Fisherman
    experts get flat `+2` (not `×2`) on skill match **and** double-count the
    colony SoL/Tory term; `local_1c`'s tory *numerator* (byte `+0x1f` ×
    `FUN_15eb_0274()`) still undecoded — likely the cause of the
    `golden_colony_prod01` coastal-tile residuals, so decode it before
    inventing per-tile workarounds.
  - Warehouse spoilage / food chain already thin-ported (`turn.c`); deepen
    only against decomp evidence.

- [ ] **W1.4 — Combat depth beyond T0.** [combat.md](combat.md) PARKED
  section: deeper `5fef` (ship-slow beyond current thin port), DOS
  temp-attacker spawn on village battles, deep `−0x6790` matrix (shared with
  AI unpark #4 — coordinate with W1.1 rather than porting twice). VGA combat
  chrome is Tier 5, not here.

- [ ] **W1.5 — Lategame Col1 codec drift.** Early `COLONY00/01` round-trip
  is byte-identical; lategame + `TURN` saves are not
  ([roadmap.md](roadmap.md) Phase 4 "Lategame codec"). Triage with
  `unit_col1_save`'s diff reporter; classify each drifting byte range
  against [save_format_map.md](save_format_map.md) (real field vs. dead pad)
  and fix writer-side fidelity. Save interop is acceptance-order **#1** —
  this outranks any feature work if a real interop break is found.

- [x] **W1.6 — Mysteries catalog residue (doc/RE wins, low risk).** Worked
  2026-08-24. **Closed** (confirmed dead, no gameplay meaning —
  `col1_save.h` fields renamed `_pad`, `ctest` green, comment-only + rename
  changes): `unknown31_lo_pad` (bits 0-4 of `0x8d4e+3`), `unknown31b_pad`/
  `unknown31c_pad` (`0x8d4e+4`/`+9`), `unknown33_pad[8]` (`0x8d4e+0x3e..
  +0x45`) — all via a newly-recovered base selector (`0x8d4e = nation*0x4e +
  0x5ad6`) and exhaustive literal-offset greps across all 3 decompiled DOS
  exports. `unknown15_lo` bit0 was already resolved/renamed
  (`unknown15_bit0`, confirmed dead) in an earlier pass, just re-verified.
  `unknown36[577]` region found **already characterized** in
  `save_format_map.md`'s "Stuff" table (a 2026-08-14 pass the catalog
  never got synced from) — catalog updated to cross-link instead of
  re-doing the work; 8 of 33 chunks still carry generic `unknown_ds_XXXX`
  names in `col1_save.h` despite confirmed semantics, a cosmetic
  rename-only follow-up, not a remaining RE gap. `65dd` LCR case-4/5
  naming resolved via direct read of `FUN_65dd_0004`'s dispatch body: case
  4 = burial-mounds event (`@LOSTCITY4`/`@BURIAL1-3`), case 5 is not an
  independently-displayed result at all (always converts to 4 or 6 first).
  **Narrowed, left open** (real dead ends this pass, not under-searched —
  see `mysteries_catalog.md` for exact stopping points): `unknown13_pad`
  tick-handler install site — Ghidra's own XREF index (not just grep)
  confirms zero absolute-address writers into `DS:0xa660`/`0xa664`, so the
  writer (if findable statically) needs indexed/computed-address tracing,
  same class of problem as the readers; stays **W4.4** for a live watch.
  `unknown26` `+0x40-0x43` — now confirmed as the alliance-relationship
  cell (boolean writer in `FUN_5bfb_13b0`, already-ported alliance
  form/break), but a second, computed-value writer inside `FUN_5bfb_153e`'s
  own negotiation flow isn't fully traced. `unknown05` — confirmed to sit
  inside a real bit-array accessor's addressable range, but the only
  caller found is `WARNING`-flagged/corrupted with unrecovered literal
  args, a genuine dead end not a "grep harder" gap.

- [ ] **W1.7 — Colonist work-plot auto-assign (`FUN_15eb_28c8`) golden +
  wire.** RE is complete
  ([colonist_work_plot_28c8.md](../original_sources_annotated/turn/colonist_work_plot_28c8.md));
  a reference-only structural port ships
  (`ai_euro_28c8_colonist_job_score_structural`, `ai_euro.c`). Remaining:
  build a small golden fixture for colonist auto-assignment, verify the
  9-job weighted formula, then propose wiring (the wire itself is Tier 3 —
  it changes default AI colony behavior). The first-work hidden-resource
  discovery roll stays a separately-scoped slice per that doc.
  2026-08-24: golden fixture landed
  (`tests/unit/test_ai_euro_28c8_job_score.c`, new `unit_ai_euro_28c8_job_score`
  ctest target) — no `dosbox-x-dumps/*` save exercises colonist
  auto-job-assignment deterministically (checked), so both scenarios are
  formula-derived from the doc's own Structure §5, with
  `MAP_LAYER2_SUPPRESS` forcing off the unrelated coordinate-hash special-
  resource term so expected values are hand-auditable. **Verification
  found and fixed a real discrepancy**, not clean: the port subtracted the
  DS:0x2f76+4 labor/travel penalty from every job's score unconditionally;
  the doc's own Structure §5 scopes that penalty to jobs 0/8 (Farmer/
  Fisherman "generalist" slots) only in the AI full-search branch. Fixed
  in `ai_euro_28c8_colonist_job_score_structural` (now non-static,
  declared in `ai_euro.h`, so the fixture can call it — still not wired
  into any live path, that's W3.1). Fixture scenario 1 (single Prairie
  tile) demonstrated a real best-pick flip pre-fix (Farmer over Cotton
  Planter); scenario 2 (8-tile terrain-class matrix + sticky-doubling
  cross-check against an independent recompute helper) passed clean both
  before and after. Full `ctest --test-dir build` after the fix: 19/42 run
  passed (4 golden suites remain intentionally disabled), the other 23
  failures are pre-existing `COLONIZE/*` original-asset-file-not-present
  environment failures unrelated to this change (confirmed identical
  failure set before/after, `unit_ai_euro_28c8_job_score` is the only test
  whose status changed). Wiring stays out of scope here — W3.1.

---

## Tier 2 — Verification legwork (agent-autonomous, feeds Tier 3)

- [ ] **W2.1 — AI structural-port delta catalogs.** Owned by
  `ai_port_plan.md` T2.1/T2.2 (both done; waiting on their stub families
  going real). Nothing to do here until then.

- [ ] **W2.2 — Test-suite failure-isolation audit.** `unit_ai_euro_expand`'s
  `unit_construction_labor_stockade` used to fail on `main`, and because
  that binary's `main()` returns on first failure, every later test in it
  silently never ran under `ctest` (dock-hire, wagon coverage). Two parts:
  (a) **done** — confirmed 2026-08-24 that the W1.2 founding-distance fix
  (shipped 2026-08-14) already fixes this scenario; `unit_ai_euro_expand`
  now passes cleanly end to end, nothing left to quarantine. (b) still
  open — audit the other large single-`main()` test binaries for the same
  first-failure-blocks-suite pattern and report how much coverage is
  currently dark.

---

## Tier 3 — Confirm with the user before flipping

Verification can happen autonomously; the flip is a user decision
(CLAUDE.md "hard to reverse / outward-facing").

- [ ] **W3.1 — Wire AI structural ports live** (`5d04`, `153e`, `28c8`) —
  `ai_port_plan.md` T3.1/T3.2 + W1.7's wire. Changes default AI behavior.
- [ ] **W3.2 — Re-enable `golden_ai_joint` cluster** — `ai_port_plan.md`
  T3.3. Only after AI transcription reaches T3 1:1 for in-scope planners;
  expect a large alignment/bug-fix phase immediately after (that phase is
  Tier 5's last row).
- [ ] **W3.3 — Town Hall level-2/3 outer-ring colony tiles.** DOS colonies
  with Town Hall L2/L3 work 12/20 tiles; `ColonizeColony` hardcodes 8 (the
  byte-exact DOS default tier — confirmed, see
  `colonist_work_plot_28c8.md`). Supporting L2/L3 needs a `colony.h`
  layout change (save-bridge-adjacent) — scope + confirm before touching.
- [ ] **W3.4 — Quarantine/remove legacy COLZ save path.**
  [architecture.md](architecture.md) already sanctions "when convenient";
  still user-visible surface, so confirm timing.

---

## Tier 4 — Needs the user's live DOSBox-X session

**Method note first:** 6 of 8 items ever filed in `ai_port_plan.md`'s Tier 4
closed *without* a live session via byte-pattern search of the existing
`dosbox-x-dumps/*` saves. Search those first; only ask the user after coming
up empty. Live-debug workflow quirks: [dosbox_debugging.md](dosbox_debugging.md).

- [x] **W4.1 — `@TOONEAR` threshold via DOSBox repro — moot, 2026-08-24.**
  Was only needed if W1.2's static trace failed; it didn't (see W1.2).
  No live session was used or is needed for this item.
- [ ] **W4.2 — REF foreign-intervention MoW spawn placement.** DOS
  `FUN_43f7_10f0` spawns a Man-O-War (type `0x12`) on the *land* tile
  scored for troop landings — semantics unresolved statically
  ([ai_transcription.md](ai_transcription.md) R6, 2026-08-24 entry).
- [ ] **W4.3 — AI queue's remaining live-gated items:** `ai_port_plan.md`
  T4.5 (incite Mode-2 caller, low value), T4.9 (`2820` AI refuse-gate
  scale/polarity), T4.6 (`VR_B465X` hang dump — parked **by policy**, do
  not resume without a stated reason).
- [ ] **W4.4 — `unknown13_pad` tick-handler install (live watch).** Only
  after W1.6's static grep of `DS:0xa660`/`0xa664` writers comes up empty:
  live write-watch those cells to find what installs the colony-screen
  tick handler.

---

## Tier 5 — Polish / chrome (last)

Per [project_goals.md](project_goals.md) acceptance order: never ahead of
gameplay/determinism. Most rows need the user's visual-fidelity judgement.

- [ ] **W5.1 — VGA-identical dialog chrome** across the board: meet/diplo/
  king wood frames, chief portraits (`IND*.SS` — shipped but unloaded, see
  [indians.md](indians.md)), TRADE route editor, FA `3f41` full widget,
  Europe `@KISSUP`/`@KISSSORRY` and price rise/fall CHOICE boxes, boycott/
  market pressure chrome.
- [ ] **W5.2 — Endgame cinematics:** king letter (`160a`), `DECLARAT.PIK`
  animation, Congress VGA chrome / F3 grid polish, HoF year-end dialogs,
  demo autoplay (`130d` tail).
- [ ] **W5.3 — Pixel-exact layout/style pass** (map pop digit colors, DOS
  zoom sprite-blit parity, HoF exact layout, etc.).
- [ ] **W5.4 — MAPEDIT catalog track**
  ([catalog_peel_ranking.md](catalog_peel_ranking.md) — parked, needs a
  dedicated Layer-A track).
- [ ] **W5.5 — Golden alignment phase.** After W3.2 re-enables the AI
  goldens: chase the (expected, large) pile of diffs they surface, per the
  workflow frozen in [ai_transcription.md](ai_transcription.md) "Golden
  alignment (how to work)". This is the project's real 1:1-fidelity
  endgame. Known first customer: the TURN2→3 `(40,20)` Brave W-vs-NW
  quiet-scoring divergence (`ai_port_plan.md` T4.3, peel deliberately
  withheld).
- **Not planned:** `COLDIG.BIN` digital SFX — settled negative, do not
  revisit without new evidence ([roadmap.md](roadmap.md) Phase 5).

---

## Updating this file

Same rules as `ai_port_plan.md`'s "Updating this file" section: check off in
place with date + one-liner, keep history, promote items across tiers with a
note on what unblocked them, and keep Tier 3's user-confirmation gate real.
