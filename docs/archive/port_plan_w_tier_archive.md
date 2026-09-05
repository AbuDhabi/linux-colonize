# Archive — pre-2026-08-24 W-tier queue (moved verbatim from port_plan.md 2026-09-05)

Kept verbatim for history. Mapping to the new tracks: W1.1 → D1/D2;
W1.3 → P4.1; W1.4 → P5.3; W1.7 → P4.9; W1.8 → P5.4; W2.2 residue → P5.8;
W3.1 → D1; W3.2 → D1; W3.3 → P4.2; W3.4 → P10.3; W4.2 → P5.5;
W4.3/W4.4 → deferred; W5.x → D4; W5.5 → D1/D3.

### (archived) Tier 1 — Static RE + port (fully agent-autonomous)

- [ ] **W1.1 — AI transcription (the largest track).** Work
  `ai_port_plan.md` (git history) top to bottom. Open there as of
  2026-08-24 (later same day): **T1.8** (closed 2026-08-29 — 0015bc/0906/
  09ae/0b4e/0f74 aligned to the decompiles; residue is the land-MP-thirds
  model, see P5.8 below), **T1.13**
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

- [x] **W1.3 — Production / EOT formula fidelity — closed 2026-08-29.**
  Every sub-item below had already landed; the one thing still marked
  pending was re-verifying the 2026-08-24 `colony_yield` fix against
  `golden_colony_prod01`/`02` in an environment with the original assets.
  This worktree has them: `golden_colony_prod01`, `golden_colony_prod02`
  and `golden_colony_preview01` all pass (ctest 48/48), and `prod01`
  asserts exact per-tile values for New Amsterdam / Guadeloupe / Fort
  Nassau / St. Louis — so the "coastal-tile residual" hypothesis is moot,
  not merely unverified. Manufacturing tier rates / class scale were
  DOS-confirmed 2026-08-15 (`building_production.md`); spoilage stays the
  `FUN_15eb_0a50` thin port with no open evidence against it. Original
  row: The economy loop runs
  end-to-end but several formulas are DOS-unconfirmed:
  - Manufacturing tier rates + class scale
    ([building_production.md](building_production.md)).
  - [terrain_yields.md](terrain_yields.md) "Still open", **2026-08-24 —
    all 3 sub-items closed statically, golden re-verification still
    pending:**
    - Farmer/Fisherman expert flat `+2` vs `×2`: **confirmed correct as
      shipped**, direct decompile read (`FUN_15eb_18ec` ~11890-11899,
      `viceroy_unpacked.c`) — `local_26 += 2`, not `<<=1`, for a matching
      food/fish expert. `colony_yield_pipeline`'s `is_expert_food_fish`
      branch already had this right; the doc's "not wired" framing was
      stale relative to the 2026-08-18 code that landed it.
    - Colony SoL/Tory term double-count: **real DOS behavior, not a bug**
      (the same `local_1c` variable is re-added a second time in the
      expert branch, on top of its once-only fold earlier in the same
      function) — but the port's own replication of the re-add was wrong:
      it rebuilt the value from `colony_flags` latch bits alone instead of
      reusing the `sol_bonus` parameter already carrying the identical
      value. Fixed in `colony_yield.c`'s `colony_yield_pipeline`.
    - `local_1c`'s tory numerator (byte `+0x1f` × `FUN_15eb_0274()`):
      **fully decoded, and it isn't new** — `+0x1f` is already named
      colonist count/population elsewhere in the project
      (`colonist_work_plot_28c8.md`, `colony_eot_production.md`), and
      `FUN_15eb_0274()` is already ported as `colony_prod_sol_percent()`.
      `local_1c` is field-for-field `colony_prod_sol_bonus_field()`'s
      return value — not a separate, unnamed term. See
      [terrain_yields.md](terrain_yields.md) "Field Farmer/Fisherman
      expert formula" for the full trace.
    - **Not independently re-verified against `golden_colony_prod01`/`02`**
      this pass — this worktree has no `COLONIZE/` original-asset
      directory, so both goldens fail at `NAMES.TXT` load before any
      assertion runs (pre-existing environment gap, same 23/42 `ctest`
      failure set as `W1.7`'s 2026-08-24 entry, unchanged by this fix).
      The coastal-tile residual hypothesis stays open pending a run with
      the real assets present — new unit regression added instead
      (`test_colony_yield.c`, expert Farmer + `sol_bonus=3`/
      `colony_flags=0`, isolates the fixed re-add from the old
      latch-reconstruction path). Full `ctest --test-dir build`: same
      19/42 pass, 23/42 pre-existing-environment fail, 4 disabled — no
      regressions, `unit_colony_yield` (now 1 test larger) still green.
  - Warehouse spoilage / food chain already thin-ported (`turn.c`); deepen
    only against decomp evidence.

- [x] **W1.4 — Combat depth beyond T0.** Worked 2026-08-24. **2 of 3
  sub-items closed, 1 confirmed correctly left to W1.1 (not "still open" by
  neglect).**
  - **Ship-slow — closed, real gap found and ported.** Re-decompiled
    `FUN_5fef_1b0e` both from the flattened export and fresh via
    `GhidraDecompileAt.java` at `OVL17_L0000:1b0e` (they agree): every
    combat-entry call (`param_5`/`param_6` attack flag set) drains a flat
    `+3` to `unit+0x3149` (`moves_spent`) **before the roll, win or lose**
    (viceroy_unpacked.c ~100340-100343), stacking with the ordinary per-tile
    step cost DOS charges unconditionally in the caller (`FUN_465b`
    ~75640) — so attacking costs `(step_cost + 3)` MP total regardless of
    outcome. Land units' max MP (≤4) is consumed either way ("attack ends
    the turn"); ships' much higher max MP survives it as a genuine slow —
    that's what "ship-slow" names. Linux's `units_try_move` previously
    charged only the step cost, and only on a **win**; a loss charged
    nothing at all. Fixed in `src/core/units.c` (`units_try_move`): the
    surcharge is folded into the shared step-cost/RNG-overspend gate's
    `cost` value (not pre-subtracted from `moves_left`, which would corrupt
    that gate's "started this move at full MP → always allowed" bypass
    check) via a new `combat_attack_mp_surcharge` local, and the loss path
    now also charges `(step_cost + 3)`, clamped ≥0. New regression test
    `tests/unit/test_units.c` "naval combat-entry ship-slow MP surcharge"
    spawns an 8-movement ship, wins a combat-entry attack, and asserts
    exactly 4 MP remain (8 − (1 ocean step + 3)) — proving the ship is
    slowed, not stopped. The native raid-stay-put branch (`FUN_4d56_4528`,
    a different DOS function) keeps its own pre-existing separate MP model
    untouched.
  - **DOS temp-attacker spawn on village battles — confirmed already
    correctly ported; found and filed a real sibling gap (new W1.8).**
    Traced `FUN_5fef_1b0e`'s "no live defender found" branch: it calls
    `tile_tribe_owner` (`FUN_281f_06be`) and `colony_at_xy` (`FUN_281f_07be`)
    to tell an Indian dwelling tile from a Euro colony tile. When it's a
    dwelling (`colony_at_xy` < 0), it spawns a temp Brave/Armed
    Braves/Mtd. Braves/Mtd. Warriors from the tribe struct's `muskets`
    (+7) and `horse_breeding` (+10, >0x18) fields
    (viceroy_unpacked.c ~100400-100416) — this is a **defender** stand-in,
    not an "attacker" (the port_plan phrasing is the DOS call table's own
    loose term, reused from the fort-fire call site where the spawned unit
    genuinely is the aggressor). `src/core/units.c`'s
    `units_spawn_village_temp_defender` already matches this field-for-field
    (same +7/+10 offsets, same 0x13/0x14/+2 type-selection logic) — real,
    not a stub. **New finding:** the sibling branch (`colony_at_xy` ≥ 0 —
    i.e. an undefended **Euro** colony, not a village) spawns a *different*
    temp defender via `FUN_281f_02c6` (→ `FUN_112b_0002`,
    profession→ICONS.SS index) using colony fields at `+0x1f`/`+0xb8`
    (viceroy_unpacked.c ~100417-100432) — Linux currently has no equivalent
    at all; `units_try_capture_foreign_colony` walks straight into any
    colony with zero live defenders, no token-militia combat. Filed as
    **W1.8** below rather than ported here (needs its own RE pass on the
    colony field offsets and the profession→type mapping) — out of this
    row's stated scope ("village battles").
  - **Deep `−0x6790` matrix — confirmed not done anywhere, correctly left
    to W1.1/unpark #4, not ported here.** Checked `ai_port_plan.md` /
    `ai_transcription.md` first per this row's own "coordinate with W1.1"
    instruction: the *other* `−0x6790` site (`0a60`/`5d04` G-table stance
    nibbles) was closed 2026-08-14, but the one this row and
    `ai_transcription.md`'s "unpark #4" both point at — the deep Euro
    land/ocean `20e6` explore-ring combat-scoring matrix — is still
    explicitly **OPEN** there (`ai_transcription.md` lines ~741, 1032-1038;
    `roadmap.md` line 132; `port_plan.md` W1.1's own open-item list). Not
    duplicated here; leave to W1.1.
  - VGA combat chrome untouched (Tier 5, out of scope, as stated).
  - Full `ctest --test-dir build`: 41/42 run passed (4 golden suites
    intentionally disabled). The one failure, `unit_ai_king`'s
    "multi-unload capture should fortify one or two Regulars," is
    **pre-existing** — reproduced identically on the unmodified tree before
    any of this row's edits, unrelated to combat (that scenario never
    enters `units_try_move`'s combat branch at all).

- [x] **W1.5 — Lategame Col1 codec drift.** Worked 2026-08-24. **Closed —
  was already fixed, doc-stale.** Re-ran `unit_col1_save`'s diff reporter
  (upgraded from first-byte-only to full contiguous-range reporting, a real
  gap the row flagged) against every Col1 `.SAV` fixture in the repo: all 19
  (2 starters + 10 `valid-lategame-saves/COLONY*` + 7 `test-saves-ai/TURN*`,
  plus `mapgen/SEED100.SAV` and `tests-save-misc/unit flags error.sav`) are
  byte-identical on read→write — zero drift found. The "not byte-identical"
  claim in `roadmap.md` (git history)/[savegame.md](savegame.md) dated from
  2026-08-22, 42 minutes *before* the same day's `753662d` "Fix FF + I work"
  commit fixed it (stash/restore of nation `unknown21_pad`
  `FF_POOL_STASH_MARKER` alongside `liberty_bells_last_turn` in
  `col1_save.c`'s write path); nobody circled back to the doc note after.
  `unit_col1_save`'s `k_fixtures` table promoted all 12 lategame/TURN rows
  from diagnostic-only to strict `byte_identical=true` (real regression
  coverage now, not just a smoke pass) — `ctest` green (41/42; the one
  failure, `unit_ai_king`, is pre-existing/unrelated, confirmed at baseline
  before this row's changes). No writer-side fix was needed. See
  [save_format_map.md](save_format_map.md) and [savegame.md](savegame.md)
  Phase 5 for the dated writeup.

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
  see `archive/mysteries_catalog.md` for exact stopping points): `unknown13_pad`
  — **superseded 2026-08-27**: resolved statically (colony array literal
  `0x5e04`, writer `FUN_364b_1b4c`); W4.4 closed.
  `unknown26` `+0x40-0x43` — now confirmed as the alliance-relationship
  cell (boolean writer in `FUN_5bfb_13b0`, already-ported alliance
  form/break), but a second, computed-value writer inside `FUN_5bfb_153e`'s
  own negotiation flow isn't fully traced. `unknown05` — **resolved
  2026-08-27** (static, asm PUSH-immediate reads): bits 17-32 of the
  woodcut/splash once-only array that `event` is bits 1-16 of; only the
  demo-autoplay loop reaches ids ≥14. See `archive/mysteries_catalog.md`.

- [x] **W1.7 — Colonist work-plot auto-assign (`FUN_15eb_28c8`) golden +
  wire — closed 2026-08-29, wire landed under W3.1.** RE is complete
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

- [x] **W1.8 — Undefended Euro colony: missing token-militia combat —
  closed 2026-08-26 (see P5.4 above for the full fix writeup).**
  Found 2026-08-24 while tracing W1.4's village temp-defender mechanic.
  DOS `FUN_5fef_1b0e`'s "no live defender found" branch spawns a temp
  defender whenever the target tile is a **Euro colony** with zero live
  garrison (`colony_at_xy`/`FUN_281f_07be` ≥ 0), not just when it's an
  empty Indian dwelling — a different code path (`FUN_281f_02c6` →
  `FUN_112b_0002`, profession→ICONS.SS index; colony fields `+0x1f`/`+0xb8`;
  viceroy_unpacked.c ~100417-100432) from the already-ported village-Brave
  arm. Linux's `units_try_capture_foreign_colony` currently walks straight
  into *any* colony with no live defenders and captures it — no combat, no
  chance to lose, unlike DOS which apparently always makes the attacker
  fight a token colonist-militia defender first. Static RE only (no live
  capture needed — `FUN_281f_02c6`'s target and the colony fields are
  already resolvable per `FUNCTION_CATALOG.md` / `save_format_map.md`), but
  real work: resolve what `+0x1f`/`+0xb8` are on the colony record, what the
  profession→type selection actually produces as a defender's strength, and
  whether an empty colony can therefore ever repel an attacker. Cite:
  [combat.md](combat.md) PARKED table.

---

### (archived) Tier 2 — Verification legwork (agent-autonomous, feeds Tier 3)

- [ ] **W2.1 — AI structural-port delta catalogs.** Owned by
  `ai_port_plan.md` T2.1/T2.2 (both done; waiting on their stub families
  going real). Nothing to do here until then.

- [x] **W2.2 — Test-suite failure-isolation audit.** `unit_ai_euro_expand`'s
  `unit_construction_labor_stockade` used to fail on `main`, and because
  that binary's `main()` returns on first failure, every later test in it
  silently never ran under `ctest` (dock-hire, wagon coverage). Two parts:
  (a) **done** — confirmed 2026-08-24 that the W1.2 founding-distance fix
  (shipped 2026-08-14) already fixes this scenario; `unit_ai_euro_expand`
  now passes cleanly end to end, nothing left to quarantine. (b) **done**
  2026-08-24 — audited every multi-scenario `main()` test binary
  (CMakeLists `add_test` entries under `tests/unit/`, `tests/golden/`).
  Full ctest baseline: 41/42 active tests pass (4 `golden_ai_*`/`joint`
  disabled per W3.2, unrelated). The first-failure-blocks-suite pattern
  (`return 1`/`return fail(...)` on first check, no accumulate-and-continue)
  is the house style across essentially every multi-scenario binary, not
  just `unit_ai_euro_expand` — confirmed present in `unit_ai_euro_expand`
  (160 scenarios), `unit_ai_euro_war` (70), `unit_ai_king` (~430 inline
  checks), `unit_ai_contact` (~406), `unit_ai_diplo` (~346),
  `unit_founding_fathers` (~254), `unit_units` (443 exit points),
  `unit_turn` (293), `unit_colony_screen` (109), `unit_col1_save` (86),
  `unit_europe` (88), `unit_colonies` (48), `unit_ai` (2 scenarios), plus
  smaller multi-check binaries (`unit_map`, `unit_map_menu`,
  `unit_map_panel`, `unit_colony_yield`, `unit_reports`, `unit_pedia`,
  `unit_hall_of_fame`, `unit_new_game`, `unit_ff`, `unit_kill_indians`).
  `golden_colony_prod01`/`02` are single-scenario despite their size (one
  `run_pair` golden check each) — not at risk. All of the above currently
  pass end to end, so risk is **latent only** (nothing dark today) —
  except **`unit_ai_king`, which is a real, live instance right now**:
  its pre-existing "multi-unload fortify count" failure sits at line
  ~2890 of ~6335, and 204 `return fail(...)` checks after it (covering
  later WoI/REF/SoL/founding-father scenarios in that file) do not
  execute under `ctest` today. Underlying bug intentionally left
  unfixed (out of scope, tracked separately). No restructuring done —
  converting the shared-mutable-state monoliths (`ai_king`, `ai_contact`,
  `ai_diplo`, `founding_fathers`, `turn`, `units`, `colony_screen`,
  `col1_save`, `colonies`, `europe`) to accumulate-and-continue isn't
  safe as a mechanical edit (scenarios share state across checks within
  one `main()`); `ai_euro_expand`/`ai_euro_war` use independent
  self-contained scenario functions so a mechanical fix is plausible
  there, but 230 call sites across two files is a real refactor, not a
  trivial one — left as a documented follow-up, not attempted here.

---

### (archived) Tier 3 — Confirm with the user before flipping

Verification can happen autonomously; the flip is a user decision
(CLAUDE.md "hard to reverse / outward-facing").

- [x] **W3.1 — Wire AI structural ports live — closed 2026-08-29.**
  `5d04` was already live (T3.1). `153e` stays unwired on purpose: DOS
  only reaches it for a human self via the encounter dialog, so an AI-side
  wire is a trigger that never fires (T3.2 analysis) — closed, not deferred.
  `28c8` is now live: `ai_euro_colony_tick_28c8_reassign` (`ai_euro.c`),
  a port of the colonist-placement block of DOS's AI colony tick
  `FUN_5952_035e` (the AI-turn caller of 28c8 via resident stub
  `FUN_281f_0b6e`; the other callers are colony-UI `2f2b_348c/628a` and the
  Europe probes `38fd_3694/4f6e`). Per AI colony each turn: clear field
  plots, food pass over the slots that were Farmer/Fisherman until town
  commons + placed food ≥ population×2, then two general passes, winner's
  raw yield < 3 ends a pass (DS:0x8dbe gate); building workers untouched
  (DOS's statesman/carpenter passes remain the expert-workplace
  heuristics' job). Scorer now takes the real profession through
  `colony_yield_for_worker`; the structural/test entry point keeps plain
  yields so `unit_ai_euro_28c8_job_score` stays hand-auditable. Runs at
  the end of `ai_euro_dispatcher_turn` so the admit-time expert
  field-assign paths (need a free tile) still land first. Fallout:
  `unit_ai_euro_expand`'s nine "expert X admit + matching-tile assign"
  checks now assert admit + field-working (e.g. a plain Mountain yields
  Silver 1×2 = 2 < Ore 4, so 28c8 rightly refuses the old heuristic's
  pick). mid01/late01/colony goldens unchanged. ctest 48/48.
- [ ] **W3.2 — Re-enable `golden_ai_joint` cluster** — `ai_port_plan.md`
  T3.3. Only after AI transcription reaches T3 1:1 for in-scope planners;
  expect a large alignment/bug-fix phase immediately after (that phase is
  Tier 5's last row).
- [x] **W3.3 (→ P4.2, closed 2026-08-29) — Town Hall level-2/3 outer-ring colony tiles.** DOS colonies
  with Town Hall L2/L3 work 12/20 tiles; `ColonizeColony` hardcodes 8 (the
  byte-exact DOS default tier — confirmed, see
  `colonist_work_plot_28c8.md`). Supporting L2/L3 needs a `colony.h`
  layout change (save-bridge-adjacent) — scope + confirm before touching.
- [x] **W3.4 (→ P10.3, done 2026-08-29) — Quarantine/remove legacy COLZ save path.**
  [architecture.md](architecture.md) already sanctions "when convenient";
  still user-visible surface, so confirm timing.

---

### (archived) Tier 4 — Needs the user's live DOSBox-X session

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
  (`ai_transcription.md` (git history) R6, 2026-08-24 entry).
- [ ] **W4.3 — AI queue's remaining live-gated items:** `ai_port_plan.md`
  T4.5 (incite Mode-2 caller, low value), T4.9 (`2820` AI refuse-gate
  scale/polarity), T4.6 (`VR_B465X` hang dump — parked **by policy**, do
  not resume without a stated reason).
- [x] **W4.4 — `unknown13_pad` (closed 2026-08-27, static).** Writer is
  `FUN_364b_1b4c` on tile reveal, addressed via colony array literal
  `0x5e04`, not the `0x8542` pointer. Renamed `fortification_on_map`. See
  `archive/mysteries_catalog.md`. ISR/`0xa660` lead retracted.

---

### (archived) Tier 5 — Polish / chrome (last)

Per [project_goals.md](project_goals.md) acceptance order: never ahead of
gameplay/determinism. Most rows need the user's visual-fidelity judgement.

- [ ] **W5.1 — VGA-identical dialog chrome** across the board: meet/diplo/
  king wood frames, chief portraits (`IND*.SS` — shipped but unloaded, see
  [indians.md](indians.md)), TRADE route editor, FA `3f41` full widget,
  Europe `@KISSUP`/`@KISSSORRY` and price rise/fall CHOICE boxes, boycott/
  market pressure chrome.
- [ ] **W5.2 — Endgame cinematics.** Reworked 2026-08-30; only the demo
  autoplay is still open, and two of the original five rows were wrong.
  - [x] **King letter (`160a`) — Done 2026-08-30.** Full port of the
    Declaration signing cinematic as `src/core/declaration.c`
    (`declaration_open/update/skip_to_end/handle_input/render`), armed from
    `game_update` the frame the `AI_POPUP_TAG_KING_LETTER` popup is
    presented so it plays in front of the @INDEPENDENCE letter, exactly
    where `FUN_43f7_1a26` calls `thunk_FUN_2a1f_009a`. DOS reads: the
    background is DS:`0x12e8` = **`DECOIND.PIK`**, the glyph sheets are
    DS:`0x12f0`/`0x12f9` = `DEC-UPP0`/`DEC-LOW0` with the 8th character
    overwritten by the letter, and DS:`0x1302` = `DEC-SQIG`. The signature
    text is the human's `player[slot].country_name` (DS:`0x53f6` +
    slot*0x34 + 0x18 — the "United Colonies" rename 1a26 has just done),
    `strlwr`'d (`FUN_1d1d_0d46`) then word-initial upper-cased. Layout is
    DOS-exact: x starts 0x7e and advances by the sheet's own glyph width,
    y starts 0x94 and rises −3 per upper-case / −2 per lower-case / −1 per
    space-or-punctuation cell, a space is a 3px gap with no sprite, and the
    run stops with `DEC-SQIG` the moment x reaches 0xdc (so a long name is
    cut off and finished with a flourish — that is DOS behaviour, not a
    bug). Sprite 0 of every sheet is empty and only carries the advance
    width (DOS reads it at +0x4a of the loaded sheet = record 1); the
    drawn frames are sprites 1..10 (upper / flourish) and 1..7 (lower),
    matching DOS's `local_520` literals. Frame period is DOS's 5 ticks of
    the DS:`0x8338` counter — PIT divisor 0x7a8 (`FUN_0000_a443`),
    incremented once per tick by the INT 8 handler at `0000:a294`, so
    1193182/1960 = 608.77 Hz and 8.213 ms/frame.     Any key or click
    fast-forwards, then dismisses. Test: `unit_declaration`.
  - [x] **CLOSING.EXE rebel-victory cinematic — Done 2026-09-02.** Not in
    VICEROY: after `@KINGLOSE`, VICEROY execs `closing -gok` (string at
    `VICEROY.EXE` `0x1dabb`). Player is `COLONIZE/CLOSING.EXE` (shared
    engine with `OPENING.EXE`); art `CLOS-BKG.PIK` +
    `CLOS-{HAT,LDY,MAN,MIL,FWK,ROC,BEL}.SS`; timeline `CLOSING.TXT`
    `@CLOSING` (series / frame / repeats / baseX / delay, end marker
    series −1 frame 390). Hats, liberty bell, fireworks, rock gag.
    Ported as `src/core/closing.c`, armed after KING_THRONE payload 1
    (or a WON latch with no popups); any key/click skips to the retire
    score. Soundtrack is CLOSING.EXE's own `FUN_12d8_000e` calls: BGM `0x3d`
    at start, event `0x5a` (COLDIG 15 cheer+fireworks) on each CLOS-HAT
    sprite 1, event `0x59` on CLOS-FWK frames 1/27/37/42. Not VICEROY pool 3.
    Test:
    `unit_closing`.
  - [x] **OPENING.EXE title intro — Done 2026-09-02.** `COLONIZE.BAT` is
    `opening -g`. Player `COLONIZE/OPENING.EXE`; art `OPENING.PIK`
    (960×132 panorama) + `OPENBORD.PIK` (colour-0 window y=24..155),
    `OPENSHIP.SS` + `PATH.DAT` (701 world points), series sheets from
    `OPENING.TXT` `@OPENING`, credit plates `@CREDITS` / `OPENCRD1/2/3`.
    Camera follows the ship (clamped to 640). Two key/click skip.
    `skip_intro` in settings.json (absent file plays intro and writes true;
    afterwards the key is the player's).
    Test: `unit_opening`.
  - [x] **`DECLARAT.PIK` animation — retracted 2026-08-30, wrong premise.**
    `DECLARAT.PIK` is the *signed* parchment and is referenced by **no**
    executable in the shipped game (`grep -abo DECLARAT COLONIZE/*.EXE`
    finds nothing) — it is an unused leftover. The cinematic's art is
    `DECOIND.PIK` + `DEC-UPP*/DEC-LOW*/DEC-SQIG.SS`, covered by the row
    above. Every "DECLARAT.PIK anim PARKED" note elsewhere in the docs was
    naming the wrong asset.
  - [~] **Congress VGA chrome / F3 grid polish — partly closed 2026-08-30.**
    The liberty-bells bar was blitting all `pool` (four-digit) bell icons
    across a ~180px bar, which overlapped into a solid black block.
    `reports_draw_icon_bar` grew a `max_icons` cap and the bells call site
    passes `w / REPORTS_CONGRESS_BELLS_PITCH` (5), measured off
    `continental_p1.png`: pool 1135 / need 1849 → w = 180, bell marks at
    x = 24, 29, 34 … 177, 181 = 36 marks at a ~4.86px pitch. The other two
    bars are unchanged and already match (REF regulars: 56 icons at a 2px
    pitch; crosses: 8 icons at ~7.9px). **Still open (W5.3-shaped):** DOS's
    bell mark is a 2×7 glyph (a brown dot over a 1px grey stroke) that is
    *not* `ICONS.SS` #62 — no `ICONS.SS` sprite is ≤4px wide — so the port
    still draws the full 10×12 bell and reads fatter than the golden.
    Page-2 portraits: all 25 draw from CC-xx.SS sprite anchors
    (`reports_render_congress_page2`); the old `k_ff_portrait_slots[]`
    10/25 table is gone.
  - [x] **HoF year-end dialogs — already Done, row was stale.** The
    `FUN_41f2_14a8` retire chain (`@RETIRE` confirm → F10 → `0b70`
    exploits → `0f56` Hall of Fame → title) and the peacetime
    `@SCORED`/`@RETIRING`/`@SOONRETIRING` dialogs all landed 2026-08-29,
    after this row was written. See [manual_gap.md](manual_gap.md)
    "Hall of Fame" / "Retire → score / HoF".
  - [ ] **Demo autoplay (`130d` tail) — open, needs an attract mode.**
    Traced 2026-08-30 (`viceroy_unpacked.asm:6890-7010`). Gated on the demo
    flag DS:`0x828`; on every turn where `turn % 4 == 0` it does one of
    three things by `turn % 3`: **0** — advance DS:`0x150` and call
    `FUN_12fd_006c(id)` for the next woodcut/splash, looping while the call
    returns non-zero and `id < 0x19` (this is the sole producer of ids
    14-25, i.e. `unknown05`'s bits); **1** — pick a colony via
    `FUN_15eb_0142` over the map extents `[0x853c]-2` / `[0x853a]-2` and
    open it with `FUN_281f_0608`; **2** — `FUN_281f_05fa(human, -1)`. It
    then latches DS:`0x82b` once scoring is complete (`0x5382 & 1`) or the
    year passes 0x6bd (1725), and quits the demo when the tick counter
    passes 0x3840 or that latch is set. The blocker is not the tail itself
    — it is that the port has no attract mode (no all-AI, no-human-input
    game driver) and no `FUN_12fd_006c` woodcut-art presenter (woodcut
    events are text popups today, e.g. `@PACIFIC`). Both are bigger than
    this row.
- [ ] **W5.3 — Pixel-exact layout/style pass** (map pop digit colors, DOS
  zoom sprite-blit parity, HoF exact layout, etc.).
- [ ] **W5.4 — MAPEDIT catalog track**
  (`catalog_peel_ranking.md` (removed) — parked, needs a
  dedicated Layer-A track).
- [ ] **W5.5 — Golden alignment phase.** After W3.2 re-enables the AI
  goldens: chase the (expected, large) pile of diffs they surface, per the
  workflow frozen in `ai_transcription.md` (git history) "Golden
  alignment (how to work)". This is the project's real 1:1-fidelity
  endgame. Known first customer: the TURN2→3 `(40,20)` Brave W-vs-NW
  quiet-scoring divergence (`ai_port_plan.md` T4.3, peel deliberately
  withheld).
- ~~**Not planned:** `COLDIG.BIN` digital SFX — settled negative~~ **Retracted
  2026-08-27**: triggers exist (AX-passed event ids, invisible in the
  decompile); playback wired, see [assets.md](assets.md) "COLDIG.BIN".

---

---
