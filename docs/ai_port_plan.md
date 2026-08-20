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

**Reordered 2026-08-20** after a heavy investigation pass through T1.1–T1.9
(old numbering) surfaced several shared blockers that gate multiple items at
once. Renumbered so those high-leverage prerequisites sort first — work top
to bottom as usual, but don't re-attempt an item marked "blocked pending
T1.x" without doing T1.x first, it'll just re-derive the same dead end.

- [ ] **T1.1 — Resolve `FUN_1000_8aac` field-index accessor, fields
  3/4/5/6/0xc.** New, highest-leverage item this pass. Field 2 of this
  accessor took `move_scoring_20e6_full.md` six passes to crack (the
  jump-table-dump method, not naive decompile); fields 3/4/5/6/0xc were
  explicitly never attempted. Resolving them unblocks three separate
  downstream items at once: **T1.2**'s explore-scan/found-contact-gate
  remainder, **T1.3**'s `3558` cargo matrix (both directly gated on this
  accessor per the 2026-08-20 trace in `move_scoring_20e6_full.md`), and
  **T1.9**'s Indian quiet-scoring formula (same accessor family, shared
  with `FUN_281f_08bc`). Do this before returning to any of those three.
  **2026-08-20, first pass: real progress, not closed.** Re-verified the
  `0xd78` jump table fresh via raw byte dump (matches prior citation
  exactly) and hand-disassembled cases 3/4/0xc at the byte level (case 6
  spot-checked, unchanged). Unrelated new lead surfaced: `FUN_0000_532e`,
  a transport-chain shared-movement-budget distributor, not yet scoped.
  **2026-08-20, second pass same day — calling convention fully cracked.**
  Disassembled `FUN_0000_4fa8`'s own entry prologue (never checked
  before — prior passes went straight to the case table). Physically
  confirmed (not push-order-guessed): `[BP+6]` = the unit id, resolved to
  its transport convoy's **head unit** via `FUN_0000_4272` (walks
  `prev_unit_idx`/`0x315c` to the head — same fields case 2 splices,
  already named in `col1_save.h`); `[BP+8]` = the field/case selector
  (bounds-checked `<=0xe`, matching the 15-case table exactly); a 3rd
  argument (nation) on `20e6`/`0a60`'s calls lands outside this 2-slot
  window, unread by the shared dispatch. `FUN_0000_4272`/`FUN_0000_42ba`
  are now cleanly named (convoy-head walker / next-link step), reusable
  findings on their own. **Mechanism fully closed; individual field
  semantics for 3/4/5/6/0xc still open.** Immediate follow-up: re-read
  case 3's full 81-byte body against the known frame — hit a real wall,
  a `[BP+DI+0x4c4]` access 1220 bytes past `4fa8`'s 6-byte frame,
  genuinely ambiguous statically (legit DOS memory-model idiom vs.
  something needing more context) and not worth guessing on. **Real next
  step for T1.1 is now a live DOSBox-X question** (what's really at that
  displacement), not more static case disassembly — first genuinely
  live-capture-gated sub-item in this whole T1.1 dig. Full trace:
  `move_scoring_20e6_full.md`'s three 2026-08-20 T1.1 updates.

- [ ] **T1.2 — Deep `20e6` remaining field fidelity.** Full arms +
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
  finding). Full `ctest` green after the change. Both remaining pieces
  (explore-scan box redesign, `0x42`/`0x65` gate) are gated behind the
  `FUN_1000_8aac` accessor — do **T1.1** first.

- [ ] **T1.3 — Full `LAB_521d_3558` cargo/colony sail matrix.** Still OPEN
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
  do that as its own item before re-attempting this matrix — now queued as
  **T1.1** above. Moved on to the landfall item this session rather than
  guess.

- [x] **T1.4 — Map `ai_goals_pick_founding_tile_ex` / `06ae`'s call-site
  success/failure expectations.** New, split out of the failed T1.5
  attempt below as its real prerequisite. 12+ call sites in `ai_euro.c`
  treat this function's return-0 as a deliberate "no target here, let
  other logic decide" signal; before any replacement geometry (outward
  ring search etc.) can go in safely, catalog what each call site actually
  expects on success vs. failure. Do this before re-attempting T1.5.
  **2026-08-20: catalogued, all 16 call sites of
  `ai_euro_06ae_first_colony_from_landfall` in `ai_euro.c`** (lines 879,
  9839, 12427, 12434, 12792, 12858, 13599, 13634, 13899, 13907, 14038,
  15317, 15333, 15350, 15365, 15382). They split into two behaviorally
  distinct groups:
  - **Group A — cascading fallback (11 sites: 879, 9839, 12427/12434,
    12792, 12858, 13599, 13634, 13899/13907, 14038).** `06ae` is treated
    as the cheapest first attempt in a chain: on failure, retry with
    `ai_euro_recover_landfall_from_ship`'s alternate landfall, or (in the
    resolver `ai_euro_resolve_first_found_tile`, line ~12770) fall
    through to `ai_euro_coastal_staging_from_landfall` +
    `ai_euro_pick_founding_tile`, then finally
    `ai_goals_pick_founding_tile`. Failure never aborts anything hard —
    worst case a maneuver block is skipped this act, or the enclosing
    helper returns 0 and its own caller picks a different top-level
    action for the unit. **A replacement geometry that succeeds more
    often is safe here** as long as it doesn't regress the exact-position
    matches Group B depends on (below).
  - **Group B — exact-position wake/skip gate (5 sites: 15317, 15333,
    15350, 15365, 15382, all inside the per-turn "skip units with 0 MP"
    dispatcher gate).** These don't just check success/failure — they
    compare the returned `(fx,fy)` against `u->x`/`u->y` for an *exact*
    match (`u->x==fx && u->y==fy`) to decide whether a unit already
    sitting on/near the found tile should be woken this pass. Because
    other code earlier in the same turn already committed a `goto` using
    this same function's output, **these sites need `06ae` to return the
    identical tile it returned earlier in the same turn, not merely "a
    valid tile."** This is a stronger constraint than T1.5's original
    framing (success/failure only) — a geometry swap that changes *which*
    valid tile gets picked, even without ever failing where the old code
    succeeded, would desync these comparisons and mis-wake/mis-skip
    units. Confirms and sharpens the T1.5 revert's root-cause note.
  **Net for T1.5**: safe to replace `06ae`'s geometry only if the new
  version is called consistently (same inputs → same output) everywhere
  in a turn — true of the current fixed-band code and needs to stay true
  of any replacement; the success-rate change alone (Group A) was already
  known risky, this pass adds the value-stability constraint (Group B) as
  an equally real second constraint. Not attempted this pass — T1.5 is
  next, informed by this catalog.

- [x] **T1.5 — Full multi-ring `06ae` first-colony landfall.** Current
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
  win — do **T1.4** above first.
  **2026-08-20, same day, second attempt — shipped, informed by T1.4's
  catalog.** Key realization: DOS's real `06ae` is a *per-act, 9-tile*
  decision (score the acting unit's own 8 neighbors + stay) — it has no
  "search from a remembered landfall" shape at all; that whole concept is
  a Linux-invented coordination device for the ship/pioneer choreography,
  not a direct port of anything. And `ai_goals_pick_founding_tile_ex`
  (the byte-faithful `06ae` port, already live at 10+ other call sites)
  **already has** a ring-2..4 fallback search past its own 8-dir arm —
  built for exactly this "nothing adjacent qualifies" case, just never
  reused here. So instead of inventing new outward-search geometry (the
  reverted attempt's mistake) or touching any of the 16 call sites: kept
  `ai_euro_06ae_first_colony_from_landfall`'s existing golden-tuned
  latitude-band seed arithmetic byte-for-byte (still a pure function of
  the same inputs, no hidden state — satisfies T1.4 Group B's
  value-stability need), and replaced only the seed tile's *validation* —
  previously a single point-check that failed outright if that one tile
  was water/HS/non-foundable — with a call into
  `ai_goals_pick_founding_tile_ex` (`score_extras=0` so it needs no
  `nation_id`/`col1`, `coastal_bonus=40` matching
  `ai_euro_pick_founding_tile`'s existing first-colony convention).
  Previously-succeeding seeds are byte-identical (same tile, same
  result); only seeds that used to fail outright can now succeed via a
  nearby real tile. Full `ctest` green, `unit_ai` included (40/40, 5
  golden AI suites still Disabled per T3.3, unaffected either way). Also
  found and **deliberately left alone**: a separate, differently-signed
  copy of this same-named function in `ai.c`, hardcoded to specific
  golden-turn coordinates (`test-saves-ai TURN3–6`) with all 8 call sites
  discarding its return value — that's frozen golden-fixture scaffolding,
  not general AI logic; touching it risks breaking the exact pinning it
  exists for once goldens re-enable, out of scope here.

- [x] **T1.6 — `2820` deep Haggle (`2f96`) / hard-bargain counter-offer
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

- [ ] **T1.7 — `4528` deep raid body: tail case-dispatch semantics.**
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

- [ ] **T1.8 — `FUN_6662_0f74` pathfinding subsystem.** Confirmed clean but
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
  **2026-08-20, same day, second attempt — the tail's anti-backtrack
  wiggle-retry recovered, ported, first attempt reverted (real blocker,
  not a mistake), second attempt shipped clean.** Recovered `0f74`'s full
  tail (not attempted before): if its best 8-neighbor pick is the exact
  reverse of the unit's last-taken step, DOS rerolls up to 8 random
  directions instead of taking it, avoiding visible ping-pong — the
  "own-tile bias" earlier notes couldn't place turns out to be this, not
  a same-tile score term. First port wrote to `unit+0x314f`'s obvious
  Linux home, `ColonizeUnit.last_dir` — reverted on discovering that
  field **already has a live owner**: `ai.c`'s Indian native Brave engine
  reads/writes it every act for its own anti-backtrack bias, and
  `ai_euro.c` already independently avoided this exact collision via its
  own `s_euro_last_dir[]` shadow array (a precedent the first attempt
  missed until after implementing). **Shipped**: re-ported using a
  dedicated `units.c`-local shadow array (`s_units_goto_last_dir[]`,
  same zero-init/no-reset-hook convention as `s_euro_last_dir`) instead
  of the shared struct field — `ColonizeUnit.last_dir` untouched by this
  change (confirmed via `git diff`). `units_greedy_next_step`/
  `units_next_goto_step` now take a `ColonizeDosRng*` (NULL-safe, wiggle
  simply skipped when absent); `units_advance_goto_one_step` writes the
  shadow entry after each committed step. Full `ctest` 40/40 green. Full
  trace: `euro_unit_act.md`'s 2026-08-20 update. Flood-fills
  (`0015bc`/`0015c1`) still untouched, still the big remaining lift; the
  8-neighbor score formula's toughness term stays blocked on `0x2f76`
  (`T4.1`) regardless.

- [ ] **T1.9 — Indian mid-game quiet scoring (goods/missions/capital
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
  accessor (shared blocker with **T1.1** above) all need names before a
  faithful port, comparable in size to the `euro_g_table_0a60.md` dig.
  Not attempted this pass. No golden currently exercises
  `colony_count>0` for a Brave, so still nothing to regression-check a
  port against even once mapped — document even if untestable, per the
  original note.

- [ ] **T1.10 — Resolve `DS:0x945a` / `FUN_1d1d_0ec6` division (profession/
  dock query family).** New, split out of **T2.1**'s 2026-08-20 finding.
  `5d04`'s structural hire-ladder tail is inert-by-construction today
  because its two list-iterator stubs always return "none" — resolving
  this profession/dock query family is what would make wiring the
  structural port live (Tier 3) an actual behavior change instead of a
  no-op. See `ai-5d04-structural-port` memory for what's known so far.
  **2026-08-20: mostly resolved, one false collision, not closed.**
  `FUN_1d1d_0ec6` is a generic 32-bit signed-division CRT primitive (not
  a game formula — no live capture needed, plain C `/` is equivalent).
  `FUN_281f_0b78`/`FUN_281f_0c9a`/`FUN_291f_0afc` all resolved by direct
  disassembly: a per-type profession-slot gate (`DS:0x30e`), a profession
  exclusion test (`unit+0x315b ∉ {0x13,0x19,0x1a,0x1b,0x1c}`, cross-
  confirmed against 6+ other docs already using `0x315b`=profession), and
  a real transport-chain insert-after splice (same `0x315c`/`0x315e`
  family as `20e6` case 2 / `4272`/`42ba`). `FUN_291f_0b26` hit a **false
  collision** — its apparent target force-redecompiled clean but turned
  out to be a glyph/bitmap renderer, unrelated to AI logic (same
  coincidental-thunk bug class as `4528`'s `a6e4` correction) — real
  target still needs the `rtlink_decode` jump-table treatment, genuinely
  open. `DS:0x945a`/`0x9456` placed as `census_tally.md`'s own "Europe-
  dock proxies" (concrete addresses now, exact per-nation slot meaning
  still open). Identity-resolution pass only, matching **T2.1**'s own
  precedent — none of this wired live (still Tier 3 scope regardless).
  `ai_euro.c`'s stub header comment updated. Full trace:
  `ai-5d04-structural-port` memory. `ctest` green (comment-only change).

- [ ] **T1.11 — Name the diplomacy accessor bit `0x10` (`peace_bit_0x10`,
  same family as the already-resolved `AI_DIPLO_MET==0x40`).** New, split
  out of **T2.2**'s 2026-08-20 finding. This bit is the one live,
  non-stubbed term in the `153e` worthiness-score structural port — if
  it's ever set for a real nation pair the function already asserts
  `worthy=1` with a real score, so naming it (or explicitly deciding to
  zero it) is the real gate before that port is safe to wire live.

- [ ] **T1.12 — `test_ai_king.c` fixture rewrite.** Not RE-blocked (the
  real formula is fully known and already implemented in `ai_king.c`),
  but **2026-08-20: scoped precisely, and it's a real conceptual redesign,
  not a mechanical seed/turn patch** — the row's own "mechanical" framing
  undersold it.
  **What's actually true, confirmed by reading the file**: `turn` is set
  exactly **once** in the whole 6274-line file (`uint32_t turn = 1;`,
  never reassigned) and nothing outside `ai_king_audience_roll` reads
  `ctx->turn_number` — so the blast radius is contained to the tax/
  audience-conceptual blocks specifically, not spread across the file
  (good news: the ~5500 lines of REF/MoW/Continental-Army/combat
  scenarios don't need touching). Bad news: those tax/audience blocks
  don't just need `turn≥30` + a seed — they test mechanics the real port
  **retired**, not approximated:
  - Block 1 (~lines 195-620): tests a deterministic "spring tax year
    always hikes," an `unknown46[2]`-gated persistent "refuse" state that
    blocks further hikes until an external "Fugger sync" clears it, and
    SoL-threshold-gated accept/refuse — **none of this exists in the real
    `ai_king_tax_event`/`ai_king_audience_roll`** (see that function's own
    header comment: the old boycott-gates-the-interval behav41ior "is not
    real DOS... has been dropped"). Real design: unconditional per-
    interval RNG score roll, ladder of outcomes (cut/+1/+2/+3-4/+5-8), no
    persistent refuse-state gate.
  - Block 2 (~lines 4221-4400): tests a CHOICE popup that asks the human
    to Accept/Refuse *whether the hike happens at all*, deferred until
    apply. Real design: the audience always applies its rolled delta
    unconditionally; the *only* CHOICE is a post-hike tea-party revert
    (Accept="keep it" / Refuse="hold a tea party", reverting the just-
    applied raise) — a materially different flow (decide after, not
    before).
  Both blocks would need genuinely new scenarios authored against the
  real formula (pick real seeds, run `ai_king_nation_turn`, observe and
  assert actual output — same method already used correctly elsewhere in
  this same file, e.g. the "Seed 1 hits the..." merc-hire block), not a
  reinterpretation of the existing assertions. **Real estimate: a few
  hundred lines of test redesign across 2 blocks, verified by actual runs
  — a multi-pass task in its own right, not a same-session add-on.** Not
  attempted this pass. See R6 "Known test-suite gap" note in
  `ai_transcription.md` for the up-to-date pointer.

- [ ] **T1.13 — KINGGALLEON2 (non-Cortes royal-galleon share) re-attempt.**
  Unpark #3, still PARKED "if evidence appears." Prior passes found the
  narrative-vs-condition reading contradictory and `FUN_48d3_06ba` a false
  lead (same-segment neighbor, not the target). Re-run with overlay-clean
  decompiles of that neighborhood — the corruption-diagnosis tooling that
  cracked `4528`/`417e`/etc. postdates the last attempt here per
  [`euro_unit_act.md`](../original_sources_annotated/ai/euro_unit_act.md)
  (search `KINGGALLEON2`).
  **2026-08-20: re-attempted with live Ghidra headless tooling (same
  method that cracked **T1.9**). Exhausted the "Treasure-type unit scan"
  search vector — all 9 `unit+0x3146=='\n'` sites in `viceroy_unpacked.c`
  now checked, none is it.** One near-miss worth flagging: `FUN_465b_0000`
  (the known move-foreign dispatcher) has a Treasure-gated thunk call
  that looked promising, resolved via canonical-decompile check to
  `FUN_5fef_1908` — already known as **Treasure ransom/loot** (a
  different, already-documented mechanic), not the Galleon offer. Real
  conclusion: this mechanic isn't gated by a per-unit-record scan at all,
  so repeating that grep won't find it. **Real next step**: search the
  Europe-screen tick/harbor-arrival family instead, or search for a
  CHOICE dialog site with exactly 3 `STRING` args and **zero** `NUMBER`
  args — KINGGALLEON2's own GAME.TXT text has no `%NUMBER0` placeholder
  (unlike KINGGALLEON3's, which shows the tax-rate percentage), a real,
  checkable signature nobody's searched on yet. Full trace in
  `euro_unit_act.md`'s 2026-08-20 update. Stays PARKED. Tier 1 is **not**
  exhausted — T1.1/T1.4/T1.10/T1.11 (new prerequisite items seeded this
  pass) are still open and sort earlier; this was just the last item this
  session reached, not the end of the tier.

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
  **2026-08-20: both sub-tasks done.** List-iterator callees resolved —
  `FUN_281f_07e0`/`FUN_281f_02e4` are ordinary thunks to `FUN_1427_005c`/
  `FUN_1427_004a`, a stack/tile-unit walker over the already-known
  `ColonizeCol1TransportChain` next/prev fields (not a mystery "dock
  list"); `ai_euro.c`'s stub comment updated with the full trace. Delta
  catalog: **empty, confirmed by construction** — every mutation in the
  hire-ladder tail is already gated behind these two stubs returning
  "none," so resolving their identity doesn't change that; wiring the
  structural port live today would produce zero observable change beyond
  what's already live (the treasury bump). No live side-by-side
  experiment was needed to establish that — it follows directly from the
  stub-gating already documented. A real delta only appears once the
  stubs return real values (a separate, later semantic-RE task on the
  profession/dock query family, `DS:0x945a`, `FUN_1d1d_0ec6`'s division —
  see `ai-5d04-structural-port` memory). **Not yet a Tier 3 candidate**:
  wiring it live today is a safe no-op, so there's nothing to confirm
  with the user yet — revisit once those remaining stubs go real, i.e.
  after **T1.10**.

- [ ] **T2.2 — Verify `ai_diplo_153e_worthiness_score_structural`
  against live behavior.** Same shape as T2.1: structural reference port
  exists (`ai_diplo.c`), not wired live. Catalog deltas vs. current
  behavior before it's ready for a Tier 3 decision.
  **2026-08-20: catalogued — and the answer is materially different from
  T2.1's.** First, confirmed what "current behavior" even is: Linux has
  **no standalone war-declare-eligibility decision at all** today — war
  currently only happens as a side effect of `ai_euro_try_attack`'s
  combat-scoring already picking a target (`@SNEAK`), not a deliberate
  "should I declare on nation X" process. So wiring `153e` live wouldn't
  replace an approximation, it would add a genuinely new proactive
  trigger.
  Second, traced the structural port's full control flow with every
  current stub at its neutral value — **unlike `5d04`'s tail, this
  function is not inert-by-construction.** Every stubbed term nets to
  exactly 0 *except* `peace_bit_0x10` (`ai_diplo_read(self,target)&0x10`,
  a real, non-stub, live-data diplomacy-bit read) — if that bit is ever
  set for a real nation pair, the function already asserts `worthy=1`
  and a real nonzero score, live-data-reactive today despite every other
  term being a stub. Safe right now only because nothing calls it. Bit
  `0x10` on this accessor (same family as the already-resolved
  `AI_DIPLO_MET==0x40`) has never been independently named — genuinely
  open RE, not attempted this pass. Full trace + the header-comment
  update: `ai_diplo.c` (search "T2.1 precedent, T2.2 delta catalog").
  Queued as **T1.11**. **Not a Tier 3 candidate yet** — resolve bit `0x10`'s meaning (or
  explicitly zero `peace_bit_0x10` to force full inertness, if wiring
  is wanted sooner) before this is safe to flip. Full `ctest` green
  (comment-only change, verified anyway).

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
  detour-route scoring (if T1.8's flood-fill tiers get ported) all at once.
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
  T1.6 static pass confirmed exhausted — promoting to active Tier 4, not
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
