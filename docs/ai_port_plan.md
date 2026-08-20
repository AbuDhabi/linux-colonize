# AI Port — Sequential Work Plan

A living, ordered queue for finishing the Euro/Indian/King AI port. This file
owns **sequencing** — which item to pick up next and why it's placed there.
It does not own status detail: that stays in
[ai_transcription.md](ai_transcription.md) (FUN_* inventory, per-cluster
fidelity claims) and the `original_sources_annotated/ai/*.md` thin maps.
Phase priority for the whole project stays in [roadmap.md](roadmap.md).

**Ordering principle:** tiers are ordered from *fully agent-autonomous* to
*requires the user's live DOSBox-X session*. Work the earliest unfinished
item in the earliest tier that has one. Only drop to a later tier when every
item ahead of it is done or genuinely blocked (blocker stated in its row).
The intent: exhaust what's solvable from decomp/ASM/overlay tooling alone
before asking the user to sit at DOSBox-X again — prior passes have
repeatedly found that a thing filed as "needs a live dump" turned out to be
reachable once better static tooling (`tools/rtlink_overlay_extract.py`,
`address_mapping.csv`, `rtlink_decode`'s jump-table parser) was applied
properly. Re-check that before trusting an old "blocked" verdict at face
value — but don't rabbit-hole re-litigating closed items either.

## How an agent should use this file

1. Read this file **and** [ai_transcription.md](ai_transcription.md) first
   (CLAUDE.md rule 1 — don't let context compaction wipe either).
2. Pick the first unchecked item in the first tier that has one. If it's
   marked blocked-on-something already done, re-verify the blocker before
   starting (tooling may have moved on since it was written).
3. Do the work using the method notes below. Run the **full** `ctest` suite
   before calling anything done, not just the touched target.
4. Update: this file's checkbox + a one-line result note, the owning status
   table in `ai_transcription.md` or the relevant
   `original_sources_annotated/ai/*.md`, and — if it's a substantial
   session — the `ai-transcription-fulldraft` memory file.
5. If new work surfaces mid-task (a new blocker, a new candidate function),
   add a row to the right tier here rather than chasing it immediately,
   unless it's a one-line fix directly enabling the item in progress.
6. **Never** `git commit`/`push`/switch branches — the user handles version
   control. Work stays uncommitted in the tree.
7. Tier 3 items are flip-a-switch decisions with user-visible behavior or
   policy impact (re-enabling a golden gate, wiring a reference-only port
   live as the default path) — do the verification work, but confirm with
   the user before actually flipping the switch, per CLAUDE.md's "hard to
   reverse / outward-facing" rule. Everything in Tiers 1–2 is safe to just
   do.

## Method notes (don't relearn these)

Condensed from `ai-transcription-fulldraft` memory (full detail there if a
lesson below needs more context):

- Read the **raw decompiled function** directly before trusting a secondary
  annotated-doc summary — those have drifted from the actual bytes before.
- A `CALLF <loader>; JMPF 0x0000:XXXX` in a decompile is an **unpatched
  RTLink placeholder**, not real content — resolve it via `rtlink_decode`'s
  jump-table parser (info mode), never by naive tail-following or reading
  the raw bytes as data. This mistake produced at least two false "looks
  like the turn loop" / "looks like a data table" leads before the fix.
- A `completed=true` decompile can still carry real corruption, either in
  its own body (`WARNING: Instruction ... overlaps` / `bad instruction
  data` / decompile timeout) or **inlined from a callee** — check the text
  for `WARNING:` and check whether a cited address actually falls inside
  the target function's own boundary before concluding "corrupted." Ghidra
  can also silently pull in *wrong but plausible* content via jump/call
  misresolution with no warning at all (`684c_08c0`, `15eb_1d4c` — both
  false alarms on first pass, confirmed clean on the second, boundary-first
  check). Prefer `tools/rtlink_overlay_extract.py` +
  `tools/GhidraImportOverlays.java` (re-disassembles each RTLink segment at
  its true DOS address) over the flattened `viceroy_unpacked.c` export for
  anything flagged suspicious.
- Cross-check any unnamed DS global / resident helper against
  `original_sources_annotated/include/viceroy_globals.h` and
  `tools/address_mapping.csv` before assuming it needs a live dump — many
  "unlabeled" globals turn out to already be named for a sibling function.
- **Never invent a constant.** If a price/byte table has no captured value
  anywhere in the project, leave it stubbed with a comment, don't guess —
  this is why `417e`'s two byte tables and `0x2f76`'s terrain-cost table
  are still open (Tier 4) rather than wired with made-up numbers.
- Structural confidence (params line up, globals are named, formula shape
  fits) is **not** semantic confidence (what real-world mechanic this is).
  Keep the two separate in write-ups; the `417e` teach-price/incite
  identification saga is the cautionary example.
- When a fidelity fix changes behavior, the existing unit test usually
  encodes the *old* behavior — expect to rewrite test scenarios. For large
  cumulative single-`main()` test files, use a Python line-range splice
  instead of Edit's exact-string match.
- The harness `TaskList` does **not** persist across sessions — this file
  plus `ai_transcription.md`'s status tables plus `git log` are the
  continuity mechanism, not `TaskCreate`/`TaskList`.

---

## Tier 1 — Static RE + port (fully agent-autonomous)

Pure decomp/ASM reading, semantic RE, C port, `ctest` verification. No user
needed beyond periodic status updates.

- [ ] **T1.1 — Deep `20e6` remaining field fidelity.** Full arms +
  combat-resolve field fidelity is still OPEN per R5 Phase 1
  (`ai_transcription.md`). Entry points:
  [`move_scoring_20e6_full.md`](../original_sources_annotated/ai/move_scoring_20e6_full.md),
  [`move_scoring_land.md`](../original_sources_annotated/ai/move_scoring_land.md),
  [`move_scoring_ship.md`](../original_sources_annotated/ai/move_scoring_ship.md).
  Linux side: `ai_euro_score_step` / `ai_euro_land_*` / `ai_euro_naval_*` in
  `ai_euro.c`. **2026-08-20: epilogue roam-abort slice wired** (the
  "smaller, real, immediately portable slice" `move_scoring_20e6_full.md`
  flagged) — idle-roam gotos (`s_euro_roam_wander`, set only by
  `ai_euro_move_scoring_gate`'s explore-scan/fallback-west arms) now abort
  and force a re-decide the moment a MET foreign unit lands adjacent,
  matching DOS's `unit+0x314c==5` clear. Goal-directed AI_MOVE gotos (found-
  tile, hunt, wagon, ship staging) untouched. Still OPEN: the windowed
  best-tile-in-box explore-scan redesign itself (needs `−0x6b1a`/`−0x6a8e`
  semantics) and the `0x42`/`0x65` found/contact gate (blocked on
  `FUN_0000_4fa8`'s real resident target — see file for the false-collision
  finding). Full `ctest` green after the change.

- [ ] **T1.2 — Full `LAB_521d_3558` cargo/colony sail matrix.** Still OPEN
  (R5 Phase 2/3 note it repeatedly). Current Linux only has thin tip/peel
  ports (`ai_euro_ocean_3558_first_leg_tip`,
  `ai_euro_ocean_3558_empty_cruise_tip`), plus Series I/L/O/R partials
  (war-cargo unload, mil unload, colony-sail score, work-queue haul —
  see `move_scoring_ship.md`'s Linux-vs-OPEN table). Full multi-good
  cargo/colony scoring matrix (the `local_9c` 8-dir unload bit-word +
  16×6 work-queue matrix) still unported.
  **2026-08-20 re-check: freshly confirmed genuinely blocked, not just
  unattempted — don't re-pick this expecting a quick static win without
  reading this note first.** Traced the raw `LAB_OVL14_L0000__003558`
  body (`move_scoring_20e6_full.md` line ~1064) directly: `local_9c`'s
  bit tests are built almost entirely from `FUN_1000_8aac`'s field-3/4/
  5/6/0xc query family (`iStack_4a/48/16/46` + wartime mode-0xc probe) —
  the same accessor whose field-2 semantics took `move_scoring_20e6_full.md`
  six investigation passes to (partially) resolve, and whose fields 3/4/
  5/6/0xc were **never touched** by that investigation (it explicitly
  scoped down to field 2 only). `−0x6790`/`−0x6b5a`/`−0x7a38` (the three
  continent tables the doc's bit-cheat-sheet cites) genuinely are
  resolved now — that part of the doc's "PARKED" note is stale — but
  every branch that reads them is gated by one of these still-opaque
  `8aac` field queries, so the gating logic can't be ported without
  guessing. Also: the land-capability bitmask `DS:0x523d` gating which
  units act on a set `local_9c` bit is separately confirmed elsewhere in
  this project to live in unrecoverable binary resource data (see
  `ai_euro_0a60_unit_can_pursue_goal`'s header in `ai_euro.c`) — a second,
  independent hard blocker on top of the first. **Real prerequisite,
  not scoped this pass**: extend the `FUN_1000_8aac` field investigation
  (`move_scoring_20e6_full.md`'s "Update, Nth pass" chain) to fields 3/4/
  5/6/0xc using the same jump-table-dump method that cracked field 2 —
  do that as its own item before re-attempting T1.2's matrix. Moved on
  to T1.3 this session rather than guess.

- [ ] **T1.3 — Full multi-ring `06ae` first-colony landfall.** Current
  `ai_euro_06ae_first_colony_from_landfall` is a live west-box + coastal
  bias approximation; "adj `06ae` still misses some coastal first towns"
  (R0 dated entry). Needs the real DOS multi-ring search.
  **2026-08-20 attempt, reverted — real blocker found, not a quick fix.**
  DOS's actual `06ae` only scores the immediate 8+stay ring (already
  byte-faithful in `ai_goals_pick_founding_tile_ex`); tried replacing this
  function's fixed-band offsets with an outward ring search using that
  same real terrain-founding byte. Broke `unit_ai`'s "ship far from
  landfall/goto" sanity check even with the ring radius capped small —
  root cause wasn't the radius, it's that this function's **failure**
  return (0) is a deliberate "no target here, let other logic decide"
  signal at several of its 12+ call sites in `ai_euro.c`, and a search
  that (almost) always succeeds silently changed which branch multiple
  unrelated call sites took. Reverted clean, full `ctest` green again.
  **Real prerequisite**: map each of those 12+ call sites' success/
  failure expectations before attempting a replacement — a drop-in swap
  of this one function isn't safe regardless of how faithful its own
  geometry is. Bigger lift than it looked; don't re-attempt as a quick
  win.

- [ ] **T1.4 — `2820` deep Haggle (`2f96`) / hard-bargain counter-offer
  (`306c`) sub-loops + multi-good cargo-select CHOICE (`0x15a0`).**
  Deliberately scope-parked (not corruption-blocked) per
  [`indian_trade_2820.md`](../original_sources_annotated/ai/indian_trade_2820.md)
  "Open RE" — worth re-attempting now that `2820`'s own body is a
  confirmed-clean 595-line re-disasm. Two data-offset blockers noted there
  (`*(int*)0x8d4e+2`, per-(nation,cargo) throttle at `-0x7b44`) may need
  Tier 4 if they're genuinely never-captured — check
  `address_mapping.csv`/`viceroy_globals.h` first, they only cover function
  entry points today so this may still need a capture; confirm before
  moving it to Tier 4.
  **2026-08-20: checked, real progress, item is done as far as Tier 1 can
  take it.** Two findings: (1) `2f96`/`306c`/`311e` ("deep Haggle" /
  "hard-bargain" in the old call graph) **aren't separate functions at
  all** — same false-lead shape as `4528`'s "8 raid actions"/`a6e4`'s
  "data table" — they're internal control flow already sitting in `2820`'s
  own clean, already-recovered 595-line body (the `iStack_5e==2`/`3`
  resume-loop branches). No further RE needed to *find* this logic, just
  porting. (2) `*(int*)0x8d4e+2` is resolved — `ColonizeCol1Indian.tech`,
  already wired elsewhere (`ai_contact_meet_economics_2154`), just hadn't
  been cross-referenced back to this item. **What's left is genuinely
  Tier 4**: the per-(Euro-nation, cargo) throttle table at `-0x7b44` and a
  running scratch value at `0x8dc4`, both re-confirmed absent from
  `address_mapping.csv`/`viceroy_globals.h` this pass — load-bearing for
  the whole remaining formula (both AI and human paths), not invented.
  Moved the live-capture need to **T4.4** below (updated in place, was
  already conditionally seeded there); full details in
  `indian_trade_2820.md`'s 2026-08-20 update.

- [ ] **T1.5 — `4528` deep raid body: tail case-dispatch semantics.**
  **2026-08-20: re-scoped, old framing was stale.** The "ASM-faithful map
  `84216→end`" item is **moot** — that range was the *canonical export's*
  corrupted spillover into an unrelated overlay segment; the real function
  is complete, clean, and already fully quoted (312 lines) in
  [`indian_settlement_4528.md`](../original_sources_annotated/ai/indian_settlement_4528.md).
  The caller return-code wiring was already traced (fulldraft memory,
  "third pass") — what's actually unconfirmed is whether Linux's
  `ai_contact_indian_raids` needs an equivalent 3-way signal at all, not
  the DOS semantics. String XREF `0x1710`–`0x172e` stays open but is
  cosmetic-only (VGA chrome priority, not gameplay logic) — the head/warn/
  ship-abort thresholds it labels are already byte-exact in
  `ai_contact_try_village_raid_warn` (checked this pass, matches DOS's
  `0x4b`/`0x32`/`0x19` bands exactly).
  **Real remaining item**: the tail `switch(uStack_56)` case-dispatch
  (which of 9 outcome codes fires — case 3 specifically) needed two DS
  fields. Both now resolved: `DS:0x917c` (Euro-nation wealth rank, new
  `VICEROY_DS_EURO_WEALTH_RANK` in `viceroy_globals.h` — traced its writer,
  `FUN_5bfb_00f8`, a genuine treasury-based rank table, also unblocks the
  same field in `colony_tick_5952_035e.md`/`move_scoring_20e6_full.md`)
  and `0x8d4a+5` (settlement-record owner/capital/valid flags, already
  mapped in `settlement_record_8d4a.md`, just not cross-referenced here
  before). One loose end before porting case 3: `+5`'s owner-nibble
  meaning is confirmed for *colony* records but not independently
  verified for a *village*'s own record (which this call reads) — quick
  check, not a live-capture blocker. Cases 1/4/5/6/7/8/9 don't touch
  either field and look portable now with no new blockers. Not
  implemented this pass (scoping only) — see
  `indian_settlement_4528.md`'s 2026-08-20 update for the full trace.
  **2026-08-20, same day, follow-up — real functional gap found and
  closed, separate from the case-3 semantic question above.** Tracing
  where DOS's AI-path switch would even be *called from* in Linux's
  architecture surfaced a genuine, confirmed hole: AI land units at war
  with a tribe could already walk into an undefended *enemy colony* and
  seize it (`ai_euro_land_try_adjacent_colony_seize`), but had no
  equivalent for an undefended *village* — `ai_euro_land_best_adjacent_foe`
  only ever targets actual unit occupants (a Brave standing on the tile),
  never the village tile itself, so a war-hunted village with no Brave
  garrison was simply never attacked at all. `units_try_move` already
  resolves combat against an empty village correctly on its own (same
  internals the human Attack-CHOICE path in `game_loop.c` already uses —
  synthesizes a temp defender, applies raid fallout, attacker doesn't
  occupy the tile). Added `ai_euro_land_try_adjacent_village_seize` in
  `ai_euro.c` (mirrors the colony-seize function's shape, wired at the
  same 3 call sites) — opens hostilities + attacks an adjacent undefended
  war-target village. Full `ctest` green (40/40, no regression). This is
  the case-1/4/5/6/7/8/9-shaped "AI acts on a village" behavior DOS's
  switch dispatches to, reached via existing Linux architecture rather
  than a literal case-by-case port of the DOS switch — case 3's own
  RNG/wealth-rank-gated variant (a *different* raid intensity/outcome
  roll, not "raid vs. don't") stays open per the note above.

- [ ] **T1.6 — `FUN_6662_0f74` pathfinding subsystem.** Confirmed clean but
  large: `0015bc`/`0015c1` are two separate BFS flood-fill searches (16×16
  and 18×18 windows), `0015b7` is a small direct-step helper (the one
  genuinely quick win here, do it first within this item), a fifth helper
  `0009ae` untraced, and `000000` hits the known pcode-error decompiler bug
  (hand-transcribe like `FUN_5fef_0000`/`OVL12_L0000` were). Target: bring
  `units_next_goto_step`/`units_greedy_next_step` (`units.c`) — which
  already implements the same three-tier *shape* — closer to byte-exact.
  The toughness-deduction term and any wiggle-retry logic stay blocked on
  `0x2f76` (Tier 4) regardless — port everything else around that hole.
  See [`euro_unit_act.md`](../original_sources_annotated/ai/euro_unit_act.md)
  "2026-08-15" / "2026-08-19" updates.
  **2026-08-20: row is stale — the "quick win" this row itself points at
  is already done.** `euro_unit_act.md`'s own 2026-08-19 update (read
  before starting, per this file's rule 2) found `0015b7` decompiled
  clean and confirmed Linux's existing `units_sign_i`-based direct step
  already produces the identical result — "no further src/ changes from
  this specific thread — it was already done," written the day before
  this plan file's own row was seeded, just never cross-synced. What's
  actually left is genuinely large: two independent BFS flood-fill
  searches (`0015bc` 16×16, `0015c1` 18×18, each its own scratch-table
  relaxation loop) plus a fifth untraced helper (`0009ae`) and one
  pcode-error function (`000000`, needs hand-transcription like
  `FUN_5fef_0000`) — "not one move-driver function, a small pathfinding
  *subsystem*... genuinely large and interlocking... real, standalone,
  multi-pass-scale work," per the same doc's own honest assessment, not
  attempted then or since. Skipped this pass rather than rush a
  flood-fill port and risk the relaxation order or cost formula going
  subtly wrong — exactly the risk that doc's own write-up flags. Real
  next step if resumed: `0015bc` first (smaller of the two searches),
  fresh session, budget for a multi-pass RE + port effort, not a
  same-pass add-on.

- [ ] **T1.7 — Indian mid-game quiet scoring (goods/missions/capital
  pull) formula mapping.** R2 used to flag this as blocked on "a
  `2244`-style overlay re-recovery... canonical boundary looks wrong" for
  `FUN_291f_0a14`. **2026-08-20: checked with the mature overlay tooling
  as instructed — the "wrong boundary" diagnosis was itself wrong,
  retracted.** `FUN_291f_0a14` is a completely normal thunk to the
  already-known, already-ported `FUN_5fef_1b0e`
  (`combat_apply_1b0e_peels`) — verified by direct disassembly
  (`GhidraListInstrs.java`) showing the standard `CALLF <loader>; JMPF`
  thunk pattern, and by comparing against a known-good sibling thunk
  rendered identically. The "arg-count mismatch" that drove the old
  diagnosis was a stale line-number citation pointing at an unrelated,
  already-ported function (`FUN_364b_03f6`, coastal fort fire) — not a
  property of `FUN_291f_0a14` itself. Full trace in
  `quiet_brave_scoring.c`'s `quiet_score_colony_pull` comment.
  **Real remaining scope, now correctly stated**: this is *not*
  corruption-blocked any more, but a genuine formula-mapping task —
  `DS:0x5239` (stride-0xe table), `DS:0x523d` bitmask reuse, `DS:0x53d2`
  comparison, and the generic `FUN_281f_08bc`/`FUN_1000_8aac` field-index
  accessor (shared blocker with T1.1/T1.2) all need names before a
  faithful port, comparable in size to the `euro_g_table_0a60.md` dig.
  Not attempted this pass. No golden currently exercises
  `colony_count>0` for a Brave, so still nothing to regression-check a
  port against even once mapped — document even if untestable, per the
  original note.

- [ ] **T1.8 — `test_ai_king.c` fixture rewrite.** Mechanical, not
  RE-blocked: the file predates the real `38fd_5be8`/`38fd_3dc8` tax
  formula and still assumes deterministic year-gated, RNG-free audiences.
  Needs seeded RNG streams per scenario and `turn_number ≥ 30` setup
  instead of year-based. See R6 "Known test-suite gap" note in
  `ai_transcription.md`. Currently the whole 6000+-line single-`main()`
  file bails on assertion #1 — this blocks real coverage of everything
  else in that file, so it's higher-value than its size suggests.

- [ ] **T1.9 — KINGGALLEON2 (non-Cortes royal-galleon share) re-attempt.**
  Unpark #3, still PARKED "if evidence appears." Prior passes found the
  narrative-vs-condition reading contradictory and `FUN_48d3_06ba` a false
  lead (same-segment neighbor, not the target). Re-run with overlay-clean
  decompiles of that neighborhood — the corruption-diagnosis tooling that
  cracked `4528`/`417e`/etc. postdates the last attempt here per
  [`euro_unit_act.md`](../original_sources_annotated/ai/euro_unit_act.md)
  (search `KINGGALLEON2`).

---

## Tier 2 — Larger mechanical lifts (still agent-autonomous)

Bigger in scope than Tier 1 items but not blocked on anything static tooling
can't reach. Pick up after Tier 1 thins out, or interleave if a Tier 1 item
is genuinely stuck mid-session.

- [ ] **T2.1 — Verify `ai_euro_5d04_nation_planning_structural` against
  live behavior.** The full structural port of `5d04` exists
  (`ai_euro.c`) but is reference-only; the live `ai_euro_nation_planning`
  only took the treasury-bump formula from it. Before this can become a
  Tier 3 "wire it live" decision, do the legwork: run it side-by-side
  against `ai_euro_nation_planning` on existing goldens/fixtures, catalog
  every behavioral delta the swap would cause, and resolve the two
  remaining ambiguous list-iterator callees if feasible (see
  `ai-5d04-structural-port` memory for the resolved-symbol table and what's
  still open).

- [ ] **T2.2 — Verify `ai_diplo_153e_worthiness_score_structural`
  against live behavior.** Same shape as T2.1: structural reference port
  exists (`ai_diplo.c`), not wired live. Catalog deltas vs. current
  behavior before it's ready for a Tier 3 decision.

---

## Tier 3 — Confirm with the user before flipping

The engineering/verification for these can happen autonomously (do that
part under Tier 2 above); the actual switch-flip is a deliberate,
user-visible behavior or policy change — confirm before doing it, per
CLAUDE.md's "hard to reverse" guidance.

- [ ] **T3.1 — Wire `5d04` structural port live** (replace/extend
  `ai_euro_nation_planning`), once T2.1's delta catalog exists. This
  changes default AI economic/hire behavior.

- [ ] **T3.2 — Wire `153e` worthiness-score port live**, once T2.2's delta
  catalog exists. Changes default war-declare eligibility scoring.

- [ ] **T3.3 — Re-enable `golden_ai_turns`/`golden_ai_joint`.** Explicitly
  parked 2026-08-19 (`ai_transcription.md` top status note, `roadmap.md`
  phase 3) until the AI planner reaches T3 1:1 for in-scope planners —
  re-enabling early just chases "porting incomplete" symptoms. Don't flip
  this back on piecemeal; do it once Tier 1/2 above has meaningfully closed
  the gap, and confirm with the user first since it changes what `ctest`
  gates on by default.

---

## Tier 4 — Needs the user's live DOSBox-X session

Confirmed genuinely blocked without a live capture (register/stack trace,
memory dump, or hang-dump). Re-verify the blocker is still real (tooling
moves fast in this project) before asking the user to spend DOSBox-X time —
but don't resume speculatively either.

- [ ] **T4.1 — `DS:0x2f76..0x2f88`ish terrain-cost/toughness table.**
  Highest-leverage single capture identified so far: unblocks case-8
  Pioneer-improvement reward scale, `0f74`'s toughness-deduction term, and
  detour-route scoring (if T1.6's flood-fill tiers get ported) all at once.
  See `euro_unit_act.md` "highest-leverage single capture" note.

- [ ] **T4.2 — `FUN_41f2_0294` (village founding-worth cap) semantics.**
  Confirmed decompiler-corrupted (no recovered stack frame, `(void)`
  signature despite real call-site arguments) — needs a live
  register/stack trace, not just re-disassembly. Currently stubbed as
  `ai_indian_152e_worth_cap_stub`. Known *not* to be the cause of the
  TURN2→3 golden failure below (isolated separately) but is still a
  guaranteed future diff once goldens are back on.

- [ ] **T4.3 — TURN2→3 Brave quiet-pulse movement/RNG divergence.** Root
  cause not found (missing Brave `type=19 nation=6 xy=(39,20)` + 1-point
  `relation_by_indian` drift, reproduces identically regardless of the
  `152e` capital-gate fix). Deep, open-ended — try a fresh static pass with
  current overlay tooling first (nothing in the last investigation
  confirms a live dump is strictly required, just that static analysis
  hadn't found it yet); fall back to a hang-dump only if that's exhausted.

- [ ] **T4.4 — `2820` remaining unresolved DS fields.** **2026-08-20:
  T1.4 static pass confirmed exhausted — promoting to active Tier 4, not
  conditional any more.** `*(int*)0x8d4e+2` **resolved** (it's
  `ColonizeCol1Indian.tech`, already wired into
  `ai_contact_meet_economics_2154` — struck from this row's blocker list).
  Still needed: per-(Euro-nation, cargo) throttle at `-0x7b44` (load-
  bearing — gates both the AI auto-pick loop and a term inside the price
  formula itself, blocks the whole remaining Haggle/hard-bargain
  resume-loop port), a running scratch value at `0x8dc4` (raw multiplier
  in the same formula), and string/format IDs `0x15a9`/`0x2e0c`/`0x2e0e`
  (cosmetic only, lower priority than the two data values). Also
  2026-08-20: confirmed the "Haggle (`2f96`)/hard-bargain (`306c`)"
  framing was a false lead (not separate functions, no RE needed there —
  see `indian_trade_2820.md`) — once `-0x7b44`/`0x8dc4` are captured, the
  actual port is reading-and-transcribing already-recovered code, not a
  fresh RE hunt.

- [ ] **T4.5 — Incite (`417e`) Mode-2 trigger/caller.** Low value: Mode-1
  (human path) is fully ported and byte-faithful; whether an AI-vs-AI or
  AI-internal Mode-2 auto-incite is even a real mechanic is unconfirmed
  (the one capture that looked like it turned out to be a mis-decoded
  stack offset — see fulldraft memory passes 17–18). Optional; skip unless
  specifically requested.

- [ ] **T4.6 — `VR_B465X` hang dump.** Explicitly parked **by policy**
  (R0). Do not resume without a new, stated reason — this was a deliberate
  stop, not a stall.

---

## Tier 5 — Polish / chrome (last)

Deliberately last per `roadmap.md` phase 5 and `project_goals.md`'s
visual-polish-last rule. Some of these need the user's judgement call on
visual fidelity, not just RE.

- [ ] **T5.1 — VGA-identical dialog chrome** (meet/diplo/king wood frames,
  FA `3f41` full widget body, chief portrait `FUN_281f_04ac`).
- [ ] **T5.2 — Letter cinematic** (`43f7` `160a`).
- [ ] **T5.3 — Congress UI / F3 portrait grid.**
- [ ] **Not planned:** `COLDIG.BIN` digital SFX — investigated at length,
  no reachable DOS trigger found; treat as settled, not a queue item
  (`roadmap.md` phase 5).

---

## Updating this file

- Check an item off in place with a one-line result + date, don't delete
  the row — the history of what was tried is as useful as the current
  state (matches how `ai_transcription.md`'s changelog-style entries work).
- New discoveries go in the tier that matches their actual blocker, not
  wherever's convenient — a static-solvable item belongs in Tier 1 even if
  found while working a Tier 4 item.
- If a Tier 4/5 item turns out to be reachable statically after all (this
  has happened repeatedly — see method notes), promote it to Tier 1 with a
  note on what unblocked it, don't just solve it quietly in place.
- Keep tier 3's user-confirmation gate real — don't let "the verification
  looks fine" become an implicit yes.
