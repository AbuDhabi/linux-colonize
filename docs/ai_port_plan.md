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
  this is why `417e`'s two byte tables are still open (Tier 4) rather than
  wired with made-up numbers. (`0x2f76`'s terrain-cost table was the same
  kind of gap — resolved 2026-08-21 not via a live session but by
  byte-pattern-searching already-existing `dosbox-x-dumps/*` saves; check
  those before assuming a fresh capture is the only way in.)
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

**Reassessed 2026-08-20, later same day**, after a further session landed
four items (T1.4, T1.5, T1.6, T1.12 — all now `[x]`) and pushed T1.1/T1.10/
T1.11 forward without closing them. No renumbering needed this pass —
priority order still holds — but split one item: T1.1's case-3 sub-problem
hit a genuine live-capture wall while its other fields (4/5/6/0xc) remain
static-tractable, so that specific need is now **T4.7**, not a reason to
treat all of T1.1 as stuck. Also fixed a stale cross-reference in T1.13
(listed T1.4 as still-open after T1.4 had already landed).

**Reassessed 2026-08-21**, after a heavy session closed T1.1's remaining
static scope (fields 0/4/5/6 hit a *second* live-only wall, **T4.8**, same
shape as T4.7's), closed T1.9's terrain-table sub-blocker, closed T1.10,
and fully resolved T4.1 (the `0x2f76` terrain table — no live session
needed after all, found in existing `dosbox-x-dumps/*` saves). Fixes this
pass:
- **T1.1 marked `[x]`.** Every constituent field (2 already done, `0xc`
  closed generic, `5`≡`0`, `3`→`T4.7`, `0`/`4`/`5`/`6`→`T4.8`) is now
  either resolved or moved to Tier 4 — nothing static left under this row
  specifically. Don't re-open it hunting for more; go straight to `T4.7`/
  `T4.8` if a live session becomes available, or skip to the next open
  Tier 1 item otherwise.
- **T1.2's own blocker note was stale** (still said "gated behind
  `FUN_1000_8aac`, do T1.1 first" from earlier the same day it was
  written) — its own later correction two paragraphs down already found
  the real remaining scope is unrelated (case 2's return-value semantics,
  a separate parked question with an existing behavioral substitute).
  Corrected in place.
- **T1.3 and T1.9's "shared blocker with T1.1" pointers updated** — T1.1
  itself is closed now, so anything still gated on that accessor family is
  transitively gated on `T4.7`/`T4.8`, not on a Tier 1 item. Restated so a
  future pass doesn't go looking for Tier 1 work under T1.1 that no longer
  exists.
- **T1.7 checked off** — its own final entry already concluded "nothing
  further to do... treating as closed for practical purposes" but left the
  checkbox unchecked; fixed for consistency with how T1.6/T1.10/T1.11 were
  closed.
- **New T1.14**, genuinely new: `DS:0x2f76`'s full 16-byte layout (decoded
  incidentally while closing `T4.1`) turned up two live, real, but
  undecoded columns (`+3` colony-founding neighbor score, `+4` a
  village/growth threshold term) with real call sites already found —
  self-contained, static-tractable, not attempted yet. See its own row.

- [x] **T1.1 — Resolve `FUN_1000_8aac` field-index accessor, fields
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
  **2026-08-21 — closed. Fields 0/4/5/6 also hit a live-only wall (not just
  case 3), nothing static left.** Field `0xc`: closed clean (generic,
  non-unit-touching). Field `5`: same jump-table target as field `0`, one
  case not two. Fields `0`/`4`/`5`/`6`: force-disassembling exactly at
  each one's cited jump-table address produces implausible byte streams
  that only resync into real code 1-2 bytes later — a genuine, reproducible
  local byte-alignment mismatch (different shape from case 3's far-
  displacement ambiguity below), confirmed against a fresh MD5-verified
  re-extraction of `VICEROY.EXE`, not a tooling artifact. Queued as
  **T4.8**. Between `T4.7` (case 3) and `T4.8` (cases 0/4/5/6), every
  remaining field of this accessor needs a live DOSBox-X session — T1.1
  itself has no more Tier 1 scope. See intro note above for what this
  means for **T1.2**/**T1.3**/**T1.9**.
  **2026-08-20 reassessment: split, don't stall the whole item on case 3.**
  Only field/case 3's own body hit the live-only wall — fields 4/5/6/0xc
  were never attempted yet and the hard part (calling convention, frame
  layout) is now cracked for all of them, so they're still genuinely
  static-tractable with the same method that closed field 2. Case 3's
  live-capture need is now tracked separately as **T4.7**; keep working
  T1.1 on fields 4/5/6/0xc, skip case 3. Caveat for **T1.2**/**T1.3**
  below: both cite the *whole* "field-3/4/5/6/0xc family" as their gate,
  not a specific field — until 4/5/6/0xc are actually resolved it's
  unknown whether their remaining OPEN pieces route through field 3
  specifically (in which case they inherit T4.7's live-capture block too)
  or only through the now-tractable fields. Re-check which once 4/5/6/0xc
  land, don't assume either way.
  **2026-08-20, later same day — fields 4/5/6/0xc attempted, mixed result,
  one prior-session finding retracted.** Field 0xc: closed, re-confirmed
  generic/non-unit-touching with a byte-exact force-disassembly (new tool,
  `tools/GhidraDisasmExact.java`). Field 5: same jump-table target as field
  0 (`0x2ce0`), literally the same case. Fields 0/4/5/6: hit a **new, real,
  reproducible static wall** — force-disassembling exactly at each field's
  cited jump-table address (not falling back to a nearby already-analyzed
  instruction, the mistake that let this file's own case-4 finding above
  go unchecked) produces implausible byte streams that only resync to
  real, sensible code 1-2 bytes later. This **retracts the case-4 finding
  above** (the `[0x92c0]/[0x92c2]/[0x372]` palette-cycle C is real but
  lives at a different function, `FUN_1a0a_0004`, overlay `1a0a:0004` —
  not reachable from the resident case-4 target at all) and reopens case
  6's "clean, register-source-untraced" filing (real bit-set/clear code
  found nearby, complete with its mask-build helper at `0x6ee0`, but the
  jump table's literal target lands 2 bytes into that code's own `SAR
  CX,0x3` instruction, not at a boundary). Full trace, including the raw
  byte evidence and why this isn't just a table-parsing bug on this pass's
  own end: `move_scoring_20e6_full.md`'s "2026-08-20, later same day" T1.1
  update. **Queued as `T4.8`** (Tier 4, new) — needs a live DOSBox-X trace,
  different shape from `T4.7`'s case-3 wall (far displacement vs. local
  byte-alignment ambiguity). Field 4 downgraded from "real, checkable
  hypothesis" back to fully open — its downstream `iStack_48 != 0`
  branches in `20e6` (lines ~1201/1205/1225) stay unresolved, not
  "probably always true" as previously floated.

- [x] **T1.2 — Deep `20e6` remaining field fidelity.** Full arms +
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
  tile, hunt, wagon, ship staging) untouched. Full `ctest` green after
  the change.
  ~~Both remaining pieces (explore-scan box redesign, `0x42`/`0x65` gate)
  are gated behind the `FUN_1000_8aac` accessor — do T1.1 first.~~
  **2026-08-21: that note was already stale when written** — see the
  correction immediately below (same day), which found the explore-scan
  redesign had already shipped and the real remaining gate is unrelated to
  `8aac`'s fields. T1.1 is now closed anyway (see its own row); nothing
  here was ever actually gated on it.
  **2026-08-20, later same day — stale note corrected, real scope
  narrower.** The "windowed best-tile-in-box explore-scan redesign" open
  item was already stale when written: `move_scoring_land.md`'s own
  2026-08-15 entries show it shipped that day
  (`ai_euro_land_explore_scan_target` in `ai_euro.c`, radius-5 box scan
  with the `−0x6b1a`/`−0x6a8e` friction term via the already-ported
  G-table tier) — confirmed still live/wired at `ai_euro.c:10281`/`10411`,
  thin (not byte-exact) but real, not a gap. **Real remaining scope is
  just the `0x42`/`0x65` found/contact gate.** Its stated blocker was also
  stale: T1.1's second 2026-08-20 pass fully cracked `FUN_0000_4fa8`'s
  calling convention and confirmed it's real code, not a false collision
  (case 2 = `0x46c7`, a real transport-chain node exchange). The actual
  open question is narrower and older: case 2's own **return-value
  semantics** (what "id < 2" means at the call sites), parked at a
  genuine "needs more context than cheaply available" wall in
  `move_scoring_20e6_full.md`'s "2026-08-15, fifth pass" (would need
  tracing a long stretch of `0a60`'s preceding code, not a quick static
  win). Not resumed this pass — flagging the correction rather than
  re-diving into an already-parked dig. The sixth-pass finding still
  stands too: the *behavioral* gap this gate would close already has a
  shipped substitute (`ai_euro_unit_act`'s H-block founding-tile bind /
  `ai_euro_scout_contact_ring_target`), so this is a byte-fidelity item,
  not a functional hole.
  **2026-08-21 — closed, nothing static left.** Re-verified the `0x42`/
  `0x65` gate's own `FUN_1000_8aac(nation,param_2,2)` calls directly
  (`move_scoring_20e6_full.md` lines ~2597/2609): `param_2` here is
  `20e6`'s own formal unit-id parameter, not a register-sourced value —
  no ambiguity to resolve, unlike the *different*, still-open `0a60`
  call the 2026-08-15 fifth pass conflated this with (a conflation
  already flagged and corrected in `euro_goal_orders_0a60_full.md`'s
  2026-08-18 "Third pass": `0a60`'s own `FUN_1000_89d0`-sourced field-2
  calls are fully resolved separately — colony-tile unit search — and
  don't bear on this gate at all). Case 2's return-value semantics
  (doubly-linked transport-chain splice against fixed slot 2, returning
  a chain-link value, `<2` reading as an idle/singleton-chain check) were
  already fully worked out in the 2026-08-15 fourth pass; the 2026-08-15
  sixth pass already concluded there's **no code to ship** regardless of
  further semantic certainty — the behavioral gap is already covered by
  the shipped substitute cited above, and DOS's own chain fields
  (`unit+0x315c`/`0x315e`) are never actively written for land units in
  this port, so wiring a check against always-empty state would be dead
  code. Nothing left to do here; this correction just confirms the wall
  the 2026-08-20 note flagged isn't real for this call site. The broader
  "full arms + combat-resolve field fidelity remain OPEN" framing in
  `ai_transcription.md` R5 Phase 1 is now stale for the same reason
  **T1.3** is stale: the only other field family this could refer to
  (fields 3/4/5/6/0xc of the same `FUN_1000_8aac` accessor, lines
  ~1388-1397) is the one **T1.1** fully exhausted statically and moved to
  **T4.7**/**T4.8** — Tier-4-gated, not Tier-1 loose ends.
  `ai_transcription.md` R5 Phase 1 line updated to match. `ctest` not
  re-run (doc-only).

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
  **2026-08-21: T1.1 closed — this matrix's gate is now genuinely Tier 4,
  not Tier 1.** T1.1 exhausted the static side of the `8aac` field family:
  field `0xc` resolved generic, but fields `0`/`3`/`4`/`5`/`6` (this
  matrix's own gating fields) all landed on a live-only wall (`T4.7`
  case 3, `T4.8` cases 0/4/5/6). Nothing to do here until one of those
  lands — don't re-attempt the field investigation, it's already been
  done as far as static tooling can take it.
  **2026-08-21, later same day — `T4.7`/`T4.8` landed, but don't treat this
  matrix as unblocked yet.** Field `3`'s real body turned out to be a
  cursor/keyboard-wait utility that never reads the unit-record pointer —
  not a per-unit numeric field at all. Before porting this matrix's
  `iStack_4a/48/16/46` reads against fields 3/4/5/6, first re-verify this
  matrix's own call site (`LAB_OVL14_L0000__003558`, `move_scoring_20e6_full.md`
  line ~1408) genuinely reaches the same case-3 dispatch with the same
  `[BP+8]` convention — a keyboard-wait firing mid-cargo-scoring would be
  broken, so either this call site doesn't really take that path at
  runtime or there's a second, different field-3 dispatch not yet found.
  Resolve that question before wiring anything, not just before trusting
  old field-3 semantics.
  **2026-08-21, same day, resolved — not a blocking wait, reachability
  worry closed.** Fully decoded case 3's `0A85:00DA` helper: it's a
  keyboard-buffer *flush* (drain already-queued keys), not a wait for a
  future one — returns immediately when nothing's queued, so no hang risk
  calling it from AI scoring. `iStack_4a` (field 3) just ends up holding a
  stale cached `DS:0x2DA4`/`0x2DA6` UI coordinate — noise, not a real AI
  signal, but harmless. **If this matrix is ever ported, treat field 3's
  term as inert/no-op rather than chasing fidelity for it** — nothing
  meaningful to preserve there. Fields `0`/`4`/`5`/`6` (the real gating
  logic, `T4.8`) are the ones that actually matter for this matrix; those
  are the genuine next-step scope if this item is resumed. Full trace:
  `move_scoring_20e6_full.md`'s 2026-08-21 addendum.

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

- [x] **T1.7 — `4528` deep raid body: tail case-dispatch semantics.**
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
  **2026-08-20, later same day — case 3's entry gate re-checked before
  porting, not yet safe.** `indian_settlement_4528.md`'s own "no blocker
  there" claim undersold it: two more fields feed case 3's own entry `if`
  (separate from the already-resolved tribe`+5` check). One resolved this
  pass on the spot (`DS:0x5398` = already-named `VICEROY_DS_FOCUS_NATION`,
  making the wealth-rank compare read as "acting nation poorer than focus
  nation"; the `&0x20` diplo bit was already documented elsewhere, just
  not cross-referenced here). Two remain genuinely open:
  `FUN_1000_84fc`'s own semantics and `DS:0x8d52`'s identity (a
  mismatched-looking `(nation_id, coord?)` argument pairing not safe to
  guess at) and a tribe-record field pair at `-0x77cc`/`-0x77ce`. **Not
  porting case 3 yet** — full trace in `indian_settlement_4528.md`'s
  2026-08-20 update. Cases 1/4/5/6/7/8/9 (no `84fc`/`8d52` dependency)
  are still the safer next slice if resumed, though the behavioral gap
  they'd close is already covered by `ai_euro_land_try_adjacent_village_seize`
  above (byte-fidelity item, not a functional hole).
  **2026-08-21 — both remaining unknowns resolved by direct disassembly,
  zero unnamed fields left in case 3.** `FUN_1000_84fc` force-decompiled
  (Ghidra headless, `1000:84fc`): it's an unpatched RTLink thunk whose
  real target (`FUN_0000_5ea0`) only reads 2 of the 3 pushed args — the
  "mode"/"dialog" argument every caller passes (including case 3's `0` vs.
  everyone else's `0x181f`) is dead, never read. Resolves the "mismatched
  pairing" worry directly: real shape is a flat `table[nation_b][a]` word
  read at `DS:0x5b1c` — a stored relation value, independently confirming
  `ai.c`'s own pre-existing "`FUN_281f_030c` relation get ->
  `ai_diplo_indian_relation`" comment from the opposite direction.
  `DS:0x8d52` (`VICEROY_DS_CUR_INDIAN_ALT`) confirmed as "current Indian
  nation" with no special case-3 meaning, once the dead-arg confusion is
  cleared. Bonus find: the tribe`+5` semantic caveat this row's
  2026-08-20 entries carried forward from `settlement_record_8d4a.md` is
  also closed — `+5` on a *tribe* record isn't the colony-record owner-
  nibble rule at all, it's `ColonizeCol1Tribe.mission`'s exact encoding
  (already implemented in `col1_save.h`), confirmed via `ai.c`'s own
  resolved-symbol header comment (present in-tree, just never cross-
  referenced into this function before) plus a structural proof (the
  comparison variable is provably a 0-3 Euro nation id, incompatible with
  the colony-record nibble convention). Full decoded formula and the
  `FUN_1000_8b24` housekeeping-call resolution (also disassembled, a
  moves-cache refresh with no Linux counterpart to port): see
  `indian_settlement_4528.md`'s 2026-08-21 update. **Still not wired** —
  what's left is a caller-integration question, not RE: `FUN_4d56_4528`
  fires from generic move-foreign/contact handling (any unit's move
  landing near/on a village), a different trigger than the deliberate
  attack decision `ai_euro_land_try_adjacent_village_seize` covers, so
  the "functional gap already closed" note above may not actually apply
  to case 3 specifically — needs checking where Linux's move-resolution
  handles a unit stepping onto/adjacent to a native village before
  writing any `src/` change, not assumed. Doc-only this pass, `ctest` not
  re-run (no `.c` touched).
  **2026-08-21, same pass — checked, and the gap is real, not covered.**
  `ai_contact_try_village_raid_warn` (`ai_contact.c:802`, the Linux home
  of `4528`'s human-warn head, already cited above) is called from exactly
  one place: `game_try_unit_move` (`game_loop.c:4581`), the **human**
  move-input handler — gated on nothing AI-reachable, and the function
  itself early-returns via `ai_contact_euro_is_human()`. AI unit moves go
  through `units_advance_goto_one_step`/`units_try_move` directly, which
  never call it. `ai_euro_land_try_adjacent_village_seize` is a distinct,
  deliberate "attack an adjacent undefended war-target village" AI
  action evaluated during move-scoring — it does not fire from a unit's
  *move landing on/near* a village tile the way `4528` does, and doesn't
  cover the peaceful/mixed-relation cases (1/4/5/6/8) at all. **Net: for
  AI-controlled units, `4528`'s whole mechanic — not just case 3 — is
  currently unmodeled**, a genuine functional gap, not the fidelity-only
  polish the 2026-08-20 note assumed. Real scope if resumed: decide
  whether an AI unit stepping onto/adjacent to a village should get an
  auto-decided equivalent of the human Attack/Leave choice (using this
  session's now-fully-decoded case-3 formula for the "mission-aware"
  outcome, cases 1/4/5/6/8/9 for the rest) — an architecture question
  (where in `ai_euro.c`/`units.c` AI move execution should hook this) as
  much as a formula-porting one. Not attempted — flagging precisely
  scoped, not guessed at.
  **2026-08-21, later same day — corrected: the row above was wrong about
  which side has the gap.** Dug into `4528`'s own `0x543f` polarity (the
  byte gating the mechanical `switch(unit_type)` block vs. the narration-
  only block) and found this doc's own long-standing framing had it
  backwards — real polarity is `0`=AI, `1`=human (cross-confirmed against
  the actual DOS per-nation turn dispatcher, `viceroy_unpacked.c:6395-
  6421`, plus `ai_king.c`/`ai_diplo.c`'s pre-existing agreeing comments).
  Once corrected: the mechanical Attack/Speak-to-Chief/Mission/Learn-
  Skill/Trade dispatch (all of cases 1/3/4/5/0xb/0xc, including this
  row's own fully-decoded case 3) is DOS's **human**-only path, and
  traced what the AI-controlled branch does instead (`FUN_1000_935a`'s
  non-blocking exit route, `FUN_6f74_248e`) — a plain window-redraw-and-
  dispose with no selection/scoring logic at all. **DOS's own AI gets no
  mechanical effect from this function either.** So Linux isn't missing
  anything DOS-faithful here: `ai_contact_try_village_raid_warn`/
  `ai_contact_try_village_meet` (human-only) are correctly scoped, and
  `ai_euro_land_try_adjacent_village_seize` (a deliberate Linux-side
  stand-in for "how AI ever attacks a village," not a literal port of
  this switch) is the right shape of answer, not a compromise. No `src/`
  change from this — full trace and the doc-prose fix (several passages
  literally said "if human Euro" for the AI-only branch):
  `indian_settlement_4528.md`'s 2026-08-21 section, added right after
  the file header so it can't be missed. T1.7 has nothing further to do;
  treating as closed for practical purposes even though the checkbox
  above stays unchecked (the case-3 formula and this correction are the
  full extent of what's decodable/needed here).

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
  8-neighbor score formula's toughness term stayed blocked on `0x2f76`
  (`T4.1`) at the time (resolved 2026-08-21, see this row's tail below).
  **2026-08-20, later same day — re-checked `units.c` directly, "big
  remaining lift" needs a real caveat, not a full retraction.**
  `units_flood_next_step`/`units_bfs_next_step` (`units.c`) already
  exist and are wired into `units_next_goto_step`'s near/far tiers —
  `units_flood_next_step` is a 16×16-window destination-outward cost
  flood (`UNITS_FLOOD_W==16`, matching `0015bc`'s own window size
  exactly), `units_bfs_next_step` is a whole-map uniform BFS (a
  deliberately *different*, arguably stronger substitute for `0015c1`'s
  18×18-window-plus-waypoint-retry scheme, not a literal port of it).
  **Important caveat**: per `euro_unit_act.md`'s own 2026-08-19 note,
  these two helpers were built citing a *different* DOS pair
  (`FUN_6662_0086`/`FUN_6662_00f2`, a separate, analogous 3-tier pathing
  routine in a different overlay) — same *shape* by independent
  convergent design, not a confirmed byte-level match to `0015bc`/
  `0015c1`'s own formulas specifically. **Net**: a working, tested,
  structurally-faithful substitute already ships; closing `0015bc`/
  `0015c1` for real byte-exactness is now optional fidelity polish, not
  the functional hole the "still untouched, big remaining lift" wording
  implied. `0009ae`/`000000` stay untouched — no evidence either is
  needed for anything currently unblocked. Lowering this row's priority
  accordingly; not resumed further this pass.
  **2026-08-21 — the terrain-table capture unblocked the toughness term,
  wired.** `terrain_yields.md`'s full `DS:0x2f76` decode (that day) covers
  offset +0, the exact byte `0f74`'s scored-fallback tail reads. Re-read
  `FUN_6662_0f74`'s tail directly (`viceroy_unpacked.c:104652-104720`,
  not just the prose summary above — real formula: `penalty +
  chebyshev(cand,goal)*4 + manhattan(cand,goal)*5`, `penalty` = 3 if the
  unit's max MP <2 else `terrain_cost[candidate]*3`; also picked up an
  AI-only non-worsening-move filter, `bVar27`/`DS:0x543f`, previously
  unmentioned in this row). Wired in `units_greedy_next_step`
  (`units.c`) via the already-existing `map_dos_terr_cost_byte()`
  accessor and the existing `g_units_combat_human_nation` module cache
  (no new parameter threaded through `units_next_goto_step`'s 16+ call
  sites). Full `ctest` 41/41 green. Full trace: `euro_unit_act.md`'s
  2026-08-21 update. Flood-fills (`0015bc`/`0015c1`) still untouched —
  unaffected by this, stays optional fidelity polish per the note above.
  **2026-08-21, later same day — `0015bc` freshly redecompiled clean, real
  structure now known, one tooling bug found+fixed, still not wired.**
  Destination-outward 16×16 flood-fill, matching `units_flood_next_step`'s
  own shape/window exactly. Its per-edge cost turns out to be the *same*
  `penalty` formula as `0f74`'s already-ported scored-fallback tail
  (`max_mp<2 ? 3 : terrain_cost*3`) — DOS reuses one formula across both
  tiers, not two independent ones, a real and low-risk target if this is
  resumed (the formula and its Linux implementation already exist).
  Along the way: **`FUN_281f_090c`'s `address_mapping.csv` row was wrong**
  (pointed at an unrelated village/nation-throttle function; the thunk's
  real second call resolves to `FUN_0000_48ca`, confirmed by direct
  disassembly as a small, correct max-MP accessor) — the "max MP" formula
  `units_greedy_next_step` already ships is right in substance but misses
  a ship-only `+3` conditional bonus (gated by an unidentified per-nation
  capability bit); confirmed practically inert (no real ship has base
  MP `<2`, so the bonus can never flip the branch) and **not wired**, just
  documented. Also named **`DS:0x5234`** = `ColonizeUnitType.movement`,
  completing the already-known `0x5236`/`0x5239`/`0x523d` stride-`0xe`
  unit-type-table family with its `+0` column. **Still not wired**: the
  domain/ownership gating half of `0015bc` (as opposed to just its cost
  formula) needs `0x1dd2`/`0x1dd4`/`0x1dd6` characterized well enough to
  be sure it matches `units_can_enter` before swapping
  `units_flood_next_step`'s edge cost for real — real next step if
  resumed. Full trace: `euro_unit_act.md`'s 2026-08-21 update; `units.c`'s
  fallback-tier comment updated with the `FUN_281f_090c` correction.
  `ctest` 41/41 green (comment-only `src/` change).

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
  **2026-08-20, later same day — two more pieces resolved, one genuine
  new wall found.** `func_0x00019c04` (a 6-arg call in this same block):
  resolved to the *same* `FUN_5fef_1b0e`/`combat_apply_1b0e_peels`
  `FUN_291f_0a14` already thunks to — invoked twice with different
  argument shapes, not two mysteries. `DS:0x53d2`: already named
  elsewhere in this project (`king_ref.md`'s `crown_nation_id`) — the
  gate reads "halve the score if the crown nation is slot 2 and two
  other flags are clear," mechanically clear now. **`DS:0x5239`/the
  `0x5235..0x523d` per-unit-type family hit a real new wall**: raw-byte-
  reading flat `0000:5235` onward (same method that resolved the
  `4fa8`/T1.1 family) returns unambiguous *code*, not a data table —
  meaning the "DS-relative address == flat resident offset" identity
  this session otherwise relied on successfully does not hold uniformly
  for this address range. Not guessed at; flagged precisely instead.
  Full trace: `quiet_brave_scoring.c`'s 2026-08-20 T1.9 update.
  **2026-08-21 — the "real new wall" resolved, no live session needed
  after all.** Same fix as `T4.1`'s `DS:0x2f76` dig: locate the table by
  byte-pattern content search across existing `dosbox-x-dumps/*` saves
  instead of computing a flat address (segment arithmetic off a save's
  `CPU`-record `DS` register is fragile — confirmed two saves with
  byte-identical static table content captured *different* `DS` values,
  `0x237d` vs. `0x2042`, matching `parse_0e52_dump.py`'s own "`DS` may be
  `A000` during IRQ" caveat). Found the unit-type table (Brave family's
  known `0x38` flags, 4-in-a-row at stride `0xe`) at the same file offset
  in 3 independent saves. Decoded and cross-checked against `NAMES.TXT`
  `@UNIT`/`ColonizeUnitType`, 23/23 exact for every field this formula
  actually reads: `DS:0x5236` (entry gate) = **`attack`**
  (`ColonizeUnitType.attack`), `DS:0x5239` (`cVar9` divisor) = **`cost`**
  (`ColonizeUnitType.cost`), `DS:0x523d` flags already known. For Braves
  specifically (this file's whole documented scope): gate open (attack
  `1`), divisor trivially `1` (cost `1`). **This one literal-address
  table family is fully closed** — but `iVar14`/`iVar18` (the two
  `FUN_281f_08bc()` calls also feeding `iStack_e8`'s formula) are a
  *different* piece, the generic field-index accessor shared with
  **T1.1**'s own blocker, not resolved by this. **T1.1 closed same day**
  (see its row) with those exact fields landing on `T4.7`/`T4.8` — so
  this piece is now Tier-4-gated too, not a Tier 1 loose end.
  Full trace: `quiet_brave_scoring.c`'s 2026-08-21 update.

- [x] **T1.10 — Resolve `DS:0x945a` / `FUN_1d1d_0ec6` division (profession/
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
  **2026-08-20, later same day — independently reconfirmed via a
  different route, still not closed.** Followed `address_mapping.csv`'s
  own `FUN_291f_0b26 → ram:19d16 → FUN_1000_9d16` row (a second,
  independent path from the earlier attempt) and force-decompiled
  `ram:19d16` directly (`GhidraDecompileAt.java` against the
  `ghidra_overlay_scratch`/`OverlayTest` project). Result:
  `FUN_1000_9d16` is itself another two-call RTLink loader shim
  (`FUN_1000_1e7b(); FUN_0000_c11c(); return;`) — `FUN_1000_1e7b`
  (canonical `FUN_210d_0dab`) is the already-documented loader thunk
  (`quiet_brave_scoring.c`'s comment), and its landing target
  `FUN_0000_c11c` (canonical `FUN_1c11_000c`) is
  `FUNCTION_CATALOG.md`'s own **"Decode 2-bit packed glyphs from font
  sheet into pitched buffer at xy"** — the *same* glyph/bitmap renderer
  this row already flagged as a false collision, reached by a different
  chain. Reconfirms the false-lead diagnosis rather than progressing
  past it. Tried `rtlink_decode VICEROY.EXE`'s own info-mode "Jump table
  list" (Exe-Offset → Segment-Index/Offset dump, the thing this row's
  "genuinely open" note points at) but couldn't yet connect `291f:0b26`
  (a Ghidra/canonical-project segment:offset) to that table's "Exe
  Offset" column (a flattened-output-file byte offset) — no layout-json
  entry keys on `0x291f` the way `OVLxx_Lxxxx` names do elsewhere;
  the index-to-loadSegment linkage this needs isn't obvious from
  `viceroy_v2_output_layout.json` alone. **Real next step, still
  unresolved**: find how `291f` (canonical Ghidra segment id) maps to
  one of the jump-table-list's numbered `Segment Index` entries (2–32)
  or a resident `-1` entry — likely needs reading `rtlink_decode.cpp`'s
  own segment-numbering logic rather than guessing from the JSON.
  `ai-5d04-structural-port` memory. `ctest` green (comment-only change).
  **2026-08-21 — resolved, item closed. The `291f:0b26`-to-segment-index
  puzzle above was the wrong question the whole time** — traced the call
  from `5d04`'s own body instead of from the `FUN_291f_0b26` symbol (that
  symbol was never actually what `5d04` calls; a coincidental collision
  every prior pass chased). Force-decompiling `5d04` fresh in the
  overlay-ground-truth project shows the real call is `FUN_1000_9d16`
  directly. `GhidraDisasmExact.java` at `0000:19d16` (**re-run twice this
  pass, deterministic — the earlier retraction's very different raw-byte
  citation for what it also called this same address was itself
  mistaken**, not a real second code path) shows `CALLF FUN_1000_1e7b;
  JMPF 0x0000:0718` — unpatched-placeholder segment `0000` (confirmed
  dead: no function can be forced there), but the **offset `0718`
  survives RTLink's patch untouched and matches `FUN_38fd_0718` exactly**
  (RTLink only ever rewrites a far pointer's segment half at load time,
  never its offset — a raw placeholder's offset half is real, trustworthy
  information even before patching). `FUN_38fd_0718` — "spawn a unit with
  a given profession" (`param_1` = `@JOB` code, e.g. `0x14`=Pioneer,
  `0x15`=Soldier, `0x16`=Scout, `0x18`=Missionary; writes the
  already-known `unit+0x314c`/`0x315b`/`0x3159` fields) — now confirmed
  three independent ways: the canonical decompile itself, a from-scratch
  redecompile at its own overlay address (`OVL05_L0040:b18`, byte-for-
  byte matching structure), and the raw offset match above. Net: `5d04`'s
  block here is a "crown grants a free specialist, RNG-picked from a
  3-slot menu in the crown record, after a treasury debit" event —
  sibling to **T1.11**'s already-resolved `FUN_38fd_5930` in the same
  `38fd` overlay. `T2.1`'s "empty by construction" delta-catalog verdict
  is unaffected (the hire-ladder tail's stub bodies are still what's
  live, this only resolves the *identity* the stubs would need if wired)
  — still Tier 3 scope to actually wire. Full trace: `ai_euro.c`'s stub
  header comment, `ai-5d04-structural-port` memory 2026-08-21 update.
  `ctest` 41/41 green (comment-only change).

- [x] **T1.11 — Name the diplomacy accessor bit `0x10` (`peace_bit_0x10`,
  same family as the already-resolved `AI_DIPLO_MET==0x40`).** New, split
  out of **T2.2**'s 2026-08-20 finding. This bit is the one live,
  non-stubbed term in the `153e` worthiness-score structural port — if
  it's ever set for a real nation pair the function already asserts
  `worthy=1` with a real score, so naming it (or explicitly deciding to
  zero it) is the real gate before that port is safe to wire live.
  **2026-08-20: mechanical role fully pinned, write-trigger still open.**
  It's a third, independent `worthy=1` trigger (distinct from the
  scripted crown-pressure event and the peace+wealth-disparity read
  already in the formula) and adds a flat `(difficulty+1)*500` straight
  into `combat_delta_sum` when set — both confirmed by reading the raw
  body, not guessed. What DOS condition ever *sets* the bit is still
  unresolved: the writer (`FUN_0000_5b62`) takes a raw byte, not a mask,
  so callers read-modify-write — a direct grep for a literal `|0x10` near
  the confirmed euro_relation address pattern (39 hits project-wide,
  none nearby) came back empty as expected, not a dead end but a scoped
  next step (XREF the writer's callers, or a live write-breakpoint on
  `ram:0x5b62`) different from grepping harder. Tier 3 gate unchanged:
  not safe to wire `153e` live until this resolves or the bit is
  explicitly zeroed. Full trace: `euro_diplo_153e_full.md`'s 2026-08-20
  T1.11 update; `ai_diplo.c`'s header comment updated in place. `ctest`
  green (comment-only change).
  **2026-08-20, later same day — write-trigger found, item closed.** New
  tool `tools/GhidraListXRefs.java` (Ghidra's own cross-reference index,
  not a text grep) found the writer's only 4 callers, all siblings in one
  small resident cluster — a symmetric OR-set helper and an AND-clear
  helper, each called from exactly one overlay-side wrapper
  (`switchD_2000:da9f::caseD_10` / `FUN_281f_0a10`). Grepped the whole
  canonical export for every literal-mask call into those two wrappers
  (~20 found, values `2/4/0x10/0x20/0x22/0x40/0x60/0xb/0xbb`) — exactly
  one uses `0x10`: `FUN_38fd_5930`, a periodic per-nation event (turn ×
  wealth-rank gated) that RNG-picks an already-met, economically-behind
  rival, grants free Veteran Soldiers + a treasury bump scaled to the
  development gap, and marks bit `0x10` on that (self, rival) pair —
  reads as a scripted "the crown arms us against a specific rival" event,
  coherent with the bit's own `worthy=1`-trigger role in `153e`.
  Structural/mechanical confidence solid, full semantic certainty not
  independently cross-confirmed (no live capture) — standard caveat.
  **Still not a Tier 3 candidate** — this answers "what sets the bit" (a
  real, occasional trigger, not a hypothetical), not "is `153e` safe to
  wire live," which stays `T3.2`'s call. Full trace:
  `euro_diplo_153e_full.md`'s 2026-08-20 "write-trigger found" update;
  `ai_diplo.c`'s header comment updated again. `ctest` green
  (comment-only change).

- [x] **T1.12 — `test_ai_king.c` fixture rewrite.** Not RE-blocked (the
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
  **2026-08-20, same day, done.** Rewrote both blocks against the real
  formula, all seeds=1 (`dos_rng_seed`), scores/deltas/picked cargos
  computed by hand from the real formula (small Python replica of
  `dos_rng_next`/`dos_rng_range`'s exact LCG — same algorithm, not
  guessed) rather than trial-and-error: off-interval no-op, `tax_rate>85`
  gate, and one deterministic scenario per ladder rung (cut / +1 / +2 /
  big-raise), plus a no-`ai_popups` auto-teaparty integration case and
  its below-threshold "hike just stands" counterpart. Block 2 rewritten
  for the real apply-then-optionally-revert shape: hike lands immediately
  inside `ai_king_nation_turn` (asserted directly, not deferred), the
  `KING_AUDIENCE` CHOICE payload carries the already-decided (delta,
  cargo) pair, Accept leaves it standing, Refuse reverts + boycotts +
  dumps stock via `ai_king_tax_teaparty`. Dropped the old "dump-goods
  CHOICE after Refuse" sub-test entirely — that two-step player-picks-a-
  cargo flow doesn't exist in the real design (`ai_king.c`'s own
  R6 "stale-claim correction": the cargo is roulette-picked once, before
  the single Accept/tea-party popup, not by a follow-up prompt). Kept the
  direct `ai_king_pick_dump_goods_cargo`-call block unchanged (never
  depended on turn/interval). `turn` reset to 1 and all touched state
  restored to baseline at the end of the rewritten region so nothing
  downstream in the 6000+-line file is disturbed — confirmed by running
  the whole file, not just the touched blocks. **Re-enabled in
  `CMakeLists.txt`** (was `DISABLED TRUE` since 2026-08-19). Full `ctest`
  41/41 green (the 4 golden AI suites remain separately Disabled per
  `T3.3`, unrelated, unaffected).

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
  exhausted — **2026-08-21 reassessment, list refreshed again: T1.1, T1.7,
  and T1.10 are now also done.** Still genuinely open Tier 1 items ahead
  of this one: **T1.8** (pathfinding flood-fills, deprioritized but not
  closed), **T1.9** (Indian quiet-scoring formula mapping), and the new
  **T1.14** (terrain-record `+3`/`+4` columns). This was just the last
  item a given session reached, not the end of the tier.

- [ ] **T1.14 — Decode `DS:0x2f76` record columns `+3` (colony-founding
  neighbor score) and `+4` (village/growth threshold term); identify what
  `+0xe` actually is.** New 2026-08-21, surfaced incidentally while closing
  `T4.1`. Both have real, already-located decompiled use sites, not
  hypothetical: `+3` is read in an accumulating colony-site desirability
  loop over a candidate site's 8 neighbor tiles, gated by a coast/river
  check (`viceroy_unpacked.c:85807/88865/91539`) — a **neighbor-tile**
  term distinct from `+1`'s already-wired own-tile founding score, so
  potentially relevant to **T1.3**'s `3558` matrix and any future
  refinement of the already-shipped `T1.5` landfall port. `+4` is
  subtracted from a running byte near Indian-village globals
  (`0x8542`/`0x8dd2`, `viceroy_unpacked.c:3629`), with a forest-class
  bonus gated on difficulty — reads as a terrain-scaled village/growth
  threshold or food-cost term, potentially relevant to **T1.9** and the
  already-ported `ai_indian_152e_village_growth`. `+0xe`'s own semantic
  role is unidentified (an internal type/graphic-index-shaped column with
  one confirmed anomaly at pedia index 15/Rain Forest — see
  `terrain_yields.md`). Self-contained: the raw bytes for all three
  columns are already captured (every `dosbox-x-dumps/*` save has them,
  same table `T4.1` closed), so this is pure formula-mapping + port, no
  live capture needed. Not attempted yet. Full data:
  [`terrain_yields.md`](../terrain_yields.md) "DS:0x2f76 terrain-class
  record" table.
  **2026-08-21 — both columns' raw bytes extracted and cross-checked
  (20/20 dump instances agree; `+0xe`'s formula independently re-derived
  from the same extraction as a sanity check, matches the doc exactly
  including its Rain-Forest anomaly), but neither is a quick port after
  all — re-scoped, not closed.** `+3`'s only 3 use sites are all inside
  `FUN_521d_20e6`'s own inline found/contact candidate-tile picker, the
  exact mechanism **T1.2** closed as "no code to ship" (DOS's own chain
  bookkeeping this gates on is dead for land units in this port; Linux
  already covers the behavior another way) — porting `+3` there would be
  dead code by the same reasoning, not attempted. `+4`'s one use site was
  first attributed to `FUN_129f_0008` (canonical `FUN_0000_29f8`) —
  **retracted same session, real function still unidentified.** A fresh
  Ghidra headless force-redecompile of `FUN_0000_29f8`'s real resident
  address (`0000:29f8`, `OverlayTest` project) turned out to be a
  completely unrelated RLE/byte-run graphics decoder — the flattened
  export's `FUN_129f_0008` span carries an unresolved-indirect-jump
  corruption warning (`viceroy_unpacked.c:3513-3525`) *before* the `+0x4`
  use site (line 3629), the same false-collision signature `4528`'s
  `a6e4` and `T1.10`'s glyph-renderer already hit. **Real next step**:
  find the `+0x4` read's true enclosing function via jump-table
  resolution from a verified-clean entry point
  (`tools/rtlink_overlay_extract.py`), not by trusting the flattened
  export's nearest preceding function header — a harder, more open RE
  problem than either the original row or the first correction assumed.
  `+0xe` still not semantically identified. Full trace:
  `terrain_yields.md`'s `+0x3`/`+0x4` rows. **Not checked off** — nothing
  wired; `+4`'s real remaining scope is now "find the real function," not
  "RE a known-uncatalogued one."

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
  Queued as **T1.11**, now closed (bit meaning and write-trigger both
  found — see that row). **Still not a Tier 3 candidate** — resolving
  what the bit means and what sets it answers the RE question, not the
  policy one; wiring `153e` live either way (as-is, or with
  `peace_bit_0x10` explicitly zeroed) is `T3.2`'s own user-confirm
  decision. Full `ctest` green (comment-only change, verified anyway).

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

- [x] **T4.1 — `DS:0x2f76..0x2f88`ish terrain-cost/toughness table.**
  **2026-08-20: stale, corrected — narrower than stated.** `map.c` already
  carries live captured values for offsets `+0` (`k_map_dos_terr_cost`,
  32 entries, move-cost/toughness term — `0f74`'s toughness deduction
  is this exact byte, already resolved not blocked) and `+1`
  (`k_map_dos_terr_found_score`, founding-site score, feeds `06ae`),
  both cited "from brave Memory dump" — a prior live capture never
  cross-referenced back to this row. **Real remaining gap: offsets `+2`**
  (Pioneer clear/plow/road work-turns threshold) **and `+8`** (case-8
  completion reward scale, Lumber-Mill-gated) — formula shape known
  (`table[terrain]+2`, halved for Hardy Pioneer), no real byte values
  captured for either. Detour-route scoring (if T1.8's flood-fill tiers
  ever get ported) would reuse the already-captured `+0` byte, so it's
  not actually blocked on this either. See `euro_unit_act.md` "highest-
  leverage single capture" note (also stale, same correction applies) and
  `map.c:1555` comment.
  **2026-08-20, same day — live DOSBox-X capture done and wired,
  item closed.** User captured `DS:0x2f72..0x3175` at runtime segment
  `237D` across two pastes; cross-checked against the already-known `+0`/
  `+1` columns (30 values, exact match) to confirm `DS` correctness before
  trusting the new bytes. All 29 real terrain classes (`0..28`, matching
  `PEDIA_TERRAIN_COUNT`) decoded for `+2` and `+8`; classes 29–31 (past
  the real terrain-type count) read noisy/inconsistent, not trusted.
  Added `k_map_dos_terr_pioneer_threshold[29]`/`k_map_dos_terr_lumber_reward[29]`
  + accessors (`map_dos_terr_pioneer_threshold_byte`/
  `map_dos_terr_lumber_reward_byte`) in `map.c`/`map.h`. Wired into
  `units.c`: `units_pioneer_work_needed` now reads the real `+2` byte
  (previously approximated with the `+0` move-cost byte — a different
  table column); the clear-forest lumber reward now uses the real
  mill-scaled `+8` formula (`scale*20<<hardy`, floor of `1` with no Lumber
  Mill — a floor, not a gate, corrected from an initial misreading of the
  raw decompile) instead of a flat `20`, still warehouse-capacity-clamped.
  **Also fixed three latent AI-dispatcher bugs this surfaced**: because
  every Pioneer job used to finish in a single tick under the old
  approximation, nothing had ever exercised a mid-job re-decide — with
  the real (usually multi-turn) threshold, three separate opportunistic
  idle-unit reassignment passes in `ai_euro.c` (`ai_euro_unit_act`'s early
  move-scoring gate, `ai_euro_0a60_goal_orders_structural`'s per-unit
  goal pick, and `ai_euro_colony_goals`'s "H: light bind" founder loop)
  would each yank a Pioneer off an in-progress `CLEAR_PLOW`/`BUILD_ROAD`
  order before it could finish, since none of them checked for those two
  order codes the way they already checked for "goto" orders. All three
  now skip a unit mid-improve-job. Five test scenarios across
  `test_units.c`/`test_ai_euro_expand.c` updated from single-tick
  assumptions to multi-turn loops (method note: "existing unit test
  usually encodes the old behavior" — exactly that here). Full `ctest`
  41/41 green (4 golden AI suites still Disabled per `T3.3`, unaffected).
  **2026-08-21 addendum — rest of the record decoded, no live session
  needed.** Turns out every `dosbox-x-dumps/*` save already carries this
  whole table (`HDR=0x88` for the `Memory` blob, not `parse_0e52_dump.py`'s
  `8` — different build/segment), byte-identical across 19 unrelated
  captures for terrain classes `0..28`. Full 16-byte layout, including the
  long-sought `DS:0x2f7b` field-yield base table (`+5..+13` of this same
  record — resolves `terrain_yields.md`'s "not present in the decompile"
  note) and two newly-real-but-undecoded columns (`+3`, `+4`), written up
  in `terrain_yields.md` under "DS:0x2f76 terrain-class record". Method
  note for next time: check the existing `dosbox-x-dumps/` saves for a
  byte-pattern match before asking the user for a fresh capture — full
  static-memory RAM images may already have what's needed, and are more
  reliable than a live paste for wide table ranges.

- [x] **T4.2 — `FUN_41f2_0294` (village growth-threshold "worth" cap)
  semantics.** Not a new-village-founding decision — villages are fixed
  at map-gen and never appear mid-game; this gates an *existing* capital's
  per-turn population growth (`population++`) vs. spawn-a-colonist
  (`FUN_281f_095c`) branch. "Founding worth" is the DOS decompiler's own
  naming for the underlying `+4 value` field (also used once, separately,
  at tribe *creation* — see `FUN_4d56_0038` — which is the only place the
  word "founding" actually applies); don't read it as new-village spawn
  logic.
  **2026-08-22 — resolved, static-only, no live DOSBox-X session needed
  after all (same pattern as `T4.1`/`T4.7`/`T4.8`).** The "decompiler
  corrupted" verdict was about Ghidra's *C pseudocode* reconstruction
  failing on a bad stack-frame guess, not about the underlying machine
  code — the raw `.asm` disassembly is completely intact and was byte-
  verified against a live `dosbox_dump.sav` capture (took at `152e`'s own
  entry, before the call): the entry fingerprint (`98 3B 46 A2 7E 30`) and
  all three terrain-class weight comparisons matched at the exact expected
  offsets, in both of their two copies in the file. Ghidra's own
  compile-time call-target labeling (`viceroy_unpacked.asm:136006`,
  `CALL CODE_112:FUN_41f2_0294`) confirms `152e`'s near call through the
  RTLink import-thunk table really does reach this function — earlier live
  tracing that landed elsewhere was chasing a stale/reused overlay-buffer
  segment, not this function.
  **Real function is much bigger than "a terrain-worth cap"** — it's the
  settlement's whole AI "worth" score, a **word**-sized sum (only the low
  byte survives at the `152e` call site, since that caller does
  `cmp al,[bx+4]` — worth values ≥256 wrap silently there, a real quirk
  to preserve if ported) of seven terms, computed once at
  `41f2:0294`/`viceroy_unpacked.asm:111541` and returned at `0b6a`/`112375`:
  1. **Terrain-neighbor weight bucket** (`[bp-0x6e]`) — the originally-
     targeted piece, confirmed exact: scans a neighborhood (bound driven
     by `DS:0x539e`/colony-pointer `+0x1f`), terrain class `0x1c` → **+2**,
     `0x19`/`0x1a`/`0x1b` → **+1**, else → **+4** per tile. Appears as two
     duplicate copies of the same scan in the compiled body.
  2. **Founding-fathers time-remaining bonus** (`[bp-0x64]`) —
     `(0x6f4 - x) << 1`, gated on `DS:0x5382` flag bit `0x08` and
     `x < 0x6f4`; `x` comes from an earlier, not-yet-traced sub-scan.
  3. **Nearby-unit count bonus** (`[bp-0x58]`) — `+5` per hit in a
     25-iteration scan (`FUN_281f_07b4(i, settlement_id)` nonzero), gated
     for *printing* on the debug flag but the addition itself is
     unconditional.
  4. **Founding-fathers/turns product** (`[bp-0x6c]`) —
     `nation.<+0x18> * (0xFFFF - DS:0x53a6)`, gated on `nation.<+0x18>`
     nonzero. `nation` = `*(int*)0x84fc`, the project's known nation-struct
     anchor pointer (see `[[king-audience-tax-delta-resolved]]`'s method
     notes) — **not indexed by a specific nation here**, just the raw base
     pointer, so this reads whichever nation currently occupies slot 0 of
     that pointer, not necessarily "this tribe's Euro contact."
  5. **Clamped percentage term** (`[bp-0x60]`) — `min(nation.<+0xc>, 100)`,
     gated on `DS:0x5382` bit `0x02` and `nation.<+0xc> >= 100`.
  6. **Flat global bonus** (`[bp-0x5a]`) — `DS:0x53d0` copied directly if
     nonzero.
  7. **Distance-ish term** (`[bp-0x2]`) —
     `FUN_1d1d_0ec6(nation.<+0x2a>, nation.<+0x2c>, 1000, 0)`, gated on
     `nation.<+0x2c> >= 0` and `nation.<+0x2a> < 1000` — the two operand
     fields are compared against map-coordinate-shaped bounds (0..999) in
     this call, which reads as x/y, **but** `difficulty.md` already names
     `nation+0x2a` as `gold` on the *`nation*0x13c` array* struct — open
     contradiction, not resolved this pass; don't assume either naming
     without checking whether `*(int*)0x84fc` (raw pointer, no
     nation-index multiply here) and the `nation*0x13c` array element are
     even the same struct.
  After summing, an optional rounding correction applies if a computed
  shift value is nonzero (`FUN_1d1d_0f60`, 32-bit divide-and-round-by-8).
  The rest of the ~880-line body (roughly half of it) is verbose debug-
  string-building and a debug info-box draw, all gated behind the
  `param_1`/`DS:0x5382` debug flags — confirmed non-gameplay, matches the
  doc's original guess exactly.
  **Not yet done:** identifying `nation+0xc`/`+0x18`/`+0x2a`/`+0x2c`
  against `col1_save.h`'s existing field names (several may already be
  named — same doc-sync pattern that resolved `sticky_trade_good`/
  `crown_nation_id` earlier), the sub-scan feeding term 2's `x`, and
  `FUN_281f_07b4`'s own 25-count target list. A real C port also still
  needs `DS:0x539e`/`0x539c` (scan bounds) and colony-pointer `+0x1f`
  named. `ai_indian_152e_worth_cap_stub` in `src/core/ai.c` left
  unchanged (still flat `15`) pending that follow-up — this pass resolved
  *semantics*, not the port itself.

- [x] **T4.3 — TURN2→3 Brave quiet-pulse movement/RNG divergence.**
  **2026-08-22 — root cause pinpointed, no live DOSBox-X session needed.**
  Wrote `tools/probe_missing.c` to simulate `TURN2.SAV`→`TURN3.SAV` in
  isolation and diff every native (nation 6) unit individually — every
  unit matches golden exactly except one: the Brave at `(40,20)` should
  move **W** to `(39,20)` in real DOS, but Linux's quiet move-scoring
  picks **NW** to `(39,19)` instead. The "missing Brave `type=19 nation=6
  xy=(39,20)`" symptom was never a missing unit — it's this same unit,
  one tile off, so the test's type/nation/xy match found nothing at the
  expected coordinate. Almost certainly also explains the
  `relation_by_indian` drift (wrong tile → different Euro/Indian contact
  or visibility check nearby; not independently re-verified this pass).
  **Same shape as the ~105 "quiet formula ≠ golden" holdouts
  `seed100_brave.md` already catalogs and fixes via one-line
  `k_mid_peels` entries** (e.g. the already-applied `(39,20)` peel a
  few tiles over, `ai.c:3223` — a different tile, coincidentally close).
  A peel entry for `(40,20)`/nation 6 would close this the same way.
  **Deliberately not added** — `ai_transcription.md`'s parking note
  explicitly says not to chase individual TURN-step diffs like this one
  until the AI planner itself reaches T3 1:1; adding a peel now would be
  exactly that. Leave for whoever resumes golden alignment post-parking.

- [x] **T4.4 — `2820` remaining unresolved DS fields.** **2026-08-20:
  T1.6 static pass confirmed exhausted — promoting to active Tier 4, not
  conditional any more.** `*(int*)0x8d4e+2` **resolved** (it's
  `ColonizeCol1Indian.tech`, already wired into
  `ai_contact_meet_economics_2154` — struck from this row's blocker list).
  **2026-08-22 — the two load-bearing values resolved, and *not* the way
  this row assumed.** `-0x7b44` was mis-framed as needing a live BP-
  relative stack trace (`iStack_c8 + param_4*0x10 + -0x7b44`) — re-reading
  the `.c` shows `iStack_c8` there is a small cargo-good index (0-15), not
  a stack address, so the whole expression is `(nation*0x10 + good_idx)
  - 0x7b44`, which on 8086 wraps mod 0x10000 to a **fixed table address**:
  `0x10000 - 0x7B44 = 0x84BC`. No live session needed — read straight out
  of a `dosbox_dump.sav` mid-negotiation capture:
  `DS:84BC..84FB` = `00 05 02 03 04 01 04 13 02 0a 0a 0e 09 02 01 02`,
  byte-identical across all 4 nation rows (`84BC`/`84CC`/`84DC`/`84EC`) in
  that capture — so despite the "per-nation" framing, this particular
  table didn't actually vary by nation in the observed game state; worth
  a second capture from a different save to confirm it's not just
  coincidentally unmodified. `0x8dc4` (the running scratch multiplier)
  was in the same capture: `50` (`0x32`) — matches the very first live
  `BPM` hit from this session, cross-confirming. Also worth noting:
  `0x8dc4` is **not** trade-specific — the `.asm` shows it's a shared
  scratch cell also used by unrelated colony-construction (`2f2b`) and
  King-audience (`38fd`) percentage math, so don't memory-watch it in
  isolation expecting only trade activity.
  Still open, lower priority: string/format IDs `0x15a9`/`0x2e0c`/`0x2e0e`
  (cosmetic only). 2026-08-20: confirmed the "Haggle (`2f96`)/hard-bargain
  (`306c`)" framing was a false lead (not separate functions, no RE needed
  there — see `indian_trade_2820.md`) — the actual port is now
  reading-and-transcribing already-recovered code, not a fresh RE hunt.

- [ ] **T4.5 — Incite (`417e`) Mode-2 trigger/caller.** Low value: Mode-1
  (human path) is fully ported and byte-faithful; whether an AI-vs-AI or
  AI-internal Mode-2 auto-incite is even a real mechanic is unconfirmed
  (the one capture that looked like it turned out to be a mis-decoded
  stack offset — see fulldraft memory passes 17–18). Optional; skip unless
  specifically requested.

- [ ] **T4.6 — `VR_B465X` hang dump.** Explicitly parked **by policy**
  (R0). Do not resume without a new, stated reason — this was a deliberate
  stop, not a stall.

- [x] **T4.7 — `FUN_0000_4fa8` case 3 (field 3 of the `FUN_1000_8aac`
  accessor), value at `[BP+DI+0x4c4]`.** New 2026-08-20, split out of
  **T1.1**. Case 3's 81-byte body is fully disassembled and the calling
  convention is confirmed (not guessed), but this one access lands 1220
  bytes past the function's own 6-byte stack frame — genuinely ambiguous
  statically (could be a legit DOS memory-model idiom reaching into an
  adjacent structure, or something else) and not safe to guess at. Needs
  a live DOSBox-X read of that displacement while `4fa8` case 3 is live.
  Doesn't block T1.1's fields 4/5/6/0xc — those stay Tier 1.
  **2026-08-21 — resolved, not from a live breakpoint but from the
  existing `dosbox-x-dumps/find_memory` savestate (same method as `T4.1`).**
  `[BP+DI+0x4c4]` never appears in the real body — that was a static-tool
  artifact. Bigger finding: case 3 doesn't touch the unit-record pointer
  at all; it's a cursor/keyboard-wait utility (validates a `DS:0x2DA4`/
  `0x2DA6` word pair, ends in an `INT 16h` keypress-wait), not a per-unit
  field query. **Re-verify before porting**: T1.3's `FUN_1000_8aac(...,3)`
  call site needs re-checking against this — a keyboard-wait mid-AI-scoring
  doesn't make sense, so either that call site doesn't really reach this
  case at runtime or there's a second dispatch not yet found. Full trace:
  `move_scoring_20e6_full.md`'s 2026-08-21 update.

- [x] **T4.8 — `FUN_0000_4fa8` cases 0/4/5/6 (fields 4/5/6 of the
  `FUN_1000_8aac` accessor; case 0 by extension since field 5 shares its
  target), literal jump-table byte alignment.** New 2026-08-20, split out
  of **T1.1**. Force-disassembling exactly at each case's cited jump-table
  address (`0xa100`/`0x2ce0`/`0x6ef7`) produces implausible instruction
  streams (out-of-range shift-count immediates, an unmatched `PUSH CS`
  ahead of a `LEAVE`/`RETF`) that only resync into real, sensible code 1-2
  bytes later — confirmed real, reproducible, and not a static tooling
  artifact (checked against a fresh re-extraction of the current, MD5-
  verified `VICEROY.EXE`). Different shape from `T4.7`'s case-3 wall (a
  plausible-but-unverifiable *far displacement* read) — this one's a *local
  byte-alignment* mismatch between where the jump table points and where
  the surrounding code's own control flow says the real instruction
  boundaries are. Needs a live DOSBox-X breakpoint on `FUN_0000_4fa8`'s
  entry, single-stepping through calls with `field∈{0,4,5,6}`, to see what
  the CPU actually fetches. Full trace: `move_scoring_20e6_full.md`'s
  "2026-08-20, later same day" T1.1 update.
  **2026-08-21 — closed from the same `find_memory` savestate, no live
  breakpoint needed.** Cases 4 and 6 resolved with strong confidence
  (case 4's +2-byte-corrected read reproduces the exact `[0x92C0]`/
  `[0x92C2]`/`[0x372]` addresses from a previously-*retracted* finding —
  that finding's content was real, just mis-attributed to the wrong
  function; case 6 turned out to need **no** adjustment at all — the
  "reproducible" misalignment was scanning forward from the wrong earlier
  byte, not a real problem with `0x6ef7`). Case 0/5 resolved with moderate
  confidence (+1-byte fix, internally consistent but no external
  cross-reference like case 4 had). Full trace:
  `move_scoring_20e6_full.md`'s 2026-08-21 update.

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
