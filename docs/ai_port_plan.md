# AI Port — Sequential Work Plan

A living, ordered queue for finishing the Euro/Indian/King AI port. This file
owns **sequencing** — which item to pick up next and why it's placed there.
It does not own status detail: that stays in
[ai_transcription.md](ai_transcription.md) (FUN_* inventory, per-cluster
fidelity claims) and the `original_sources_annotated/ai/*.md` thin maps.
Phase priority for the whole project stays in [roadmap.md](roadmap.md);
the non-AI / cross-cutting sibling queue is [port_plan.md](port_plan.md)
(this file remains the sole queue for AI transcription work).

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


> **2026-08-24 — track status: DEFERRED.** Project goal is now playability
> ([port_plan.md](port_plan.md) P1–P11); this queue is deferred phase D1/D2
> there. Work it only when a P-track needs a minimum-thin unblock, or the
> user asks for AI work explicitly.

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
- **Before asking the user for a fresh live DOSBox-X capture, byte-pattern-
  search the existing `dosbox-x-dumps/*` saves (a throwaway byte-pattern
  script — there is no checked-in `tools/find_memory`; `tools/brave_dump/
  parse_*_dump.py` show the dump layout) for the table/value you need.** As of 2026-08-22 this has
  closed 6 of 8 Tier 4 items that were originally filed as "needs a live
  session" (`T4.1`, `T4.2`, `T4.3`, `T4.4`, `T4.7`, `T4.8`) — every one of
  them turned out to already be sitting in one of the ~24 existing dumps,
  static data unchanged across saves. A full RAM image beats a live paste
  for wide table ranges and doesn't cost the user any time. Only file
  something as genuinely Tier 4 after checking this and coming up empty.
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

**Reassessed 2026-08-22.** A single session closed `T1.2` and pushed `T4.2`/
`T4.3`/`T4.4`/`T4.7`/`T4.8` — all five *without* a live DOSBox-X session,
via the `dosbox-x-dumps/*` static-search method (see the reinforced method
note above). Two of those closures leave real, now-unblocked Tier 1 work
that hadn't been queued yet — added as **T1.15** (`FUN_41f2_0294` village
worth-score port, semantics fully known since `T4.2`) and **T1.16** (`2820`
deep Haggle/hard-bargain port, blocking values resolved since `T4.4`). No
renumbering — appended at the end of Tier 1, same low-risk approach as
`T1.14`. `T1.13`'s open-items list refreshed again to include both.

**Reassessed 2026-08-22, later same day**, after a further session resumed
`T1.3`/`T1.8`/`T1.9`/`T1.14` (all newly reachable via `T4.7`/`T4.8`
closing) and closed three of them: **`T1.3` closed dead** — every one of
the `3558` matrix's 5 gating terms traced to a disguised constant, an
always-fixed value, or an uncontrolled write target; the shipped binary
computes no real per-unit signal here, so there's nothing to port ("no
code to ship," same verdict class as `T1.2`). **`T1.9` closed** — its full
formula (base value + capital-pull/village/crown/G-table-stance
multipliers) is now completely named against existing project accessors;
not wired only because no golden exercises `colony_count>0` for a Brave to
verify against (a real test-coverage gap, not an RE gap — correctly not
turned into a port item, matching `T1.15`/`T1.16`'s opposite case where RE
*was* the only remaining blocker). **`T1.14` closed** — `+3` reconfirmed
dead code (same mechanism as `T1.3`), `+4` traced to a real, previously
uncatalogued function (`FUN_15eb_28c8`, "colonist work-plot job scoring,"
254 lines, already in `FUNCTION_CATALOG.md` but never linked to a `.c`/
`.md` file) — its own full port is new, separate scope, added as **T1.17**.
`T1.8` gained a real refined finding (an asymmetric AI-vs-human
tribe/fort-zone pathing gate `units.c` doesn't implement) but stays open,
deliberately not closed — a working substitute already ships and nothing
currently depends on the fix. `T1.13` (KINGGALLEON2) re-attempted, still
PARKED — two prior leads ruled out, one new unexplored one (`38fd`
overlay) flagged but not swept (too large to blind-search). Fixed a stale
cross-reference in `T1.13`'s open-items list (was still citing `T1.3`/
`T1.9` as open after both closed this pass). Tier 1 now: 12 of 17 items
closed; genuinely open are only `T1.8`, `T1.13`, `T1.15`, `T1.16`, `T1.17`.

**Reassessed 2026-08-22, later still — `T1.16` closed** (full per-unit
rewrite, user-directed; its one remaining loose end, the AI refuse-gate
scale, moved to Tier 4 as **T4.9** — genuinely needs a live session, not
guessable from the established relation-polarity convention). Tier 1 now:
13 of 17 closed; genuinely open are `T1.8`, `T1.13`, `T1.15`, `T1.17`.

**Reassessed 2026-08-24.** T1.15: Ghidra's `152e`/`0038`→`41f2_0294`
label is a misresolve (real path `4c54→2a1f:0410→overlay:0`); candidate
`FUN_4d56_0000` matches `ai_tribe_initial_pop` but cannot explain capital
`growth_accum` in TURN1→2 — stub stays 15; item stays open pending thunk
overlay-id. T1.17: reference-only structural port already shipped last
session — checkbox closed. Tier 1 open: `T1.8`, `T1.13`, `T1.15`.

**Reassessed 2026-08-24, later same day — `T1.15` closed.** Ghidra
headless `GhidraDecompileAt.java` against the raw bytes (not the
flattened export) confirmed the thunk chain the `4c54`/`2a1f:0410` path
above was blocked on: it really is `FUN_4d56_0000` (as guessed), and the
"cannot explain capital growth" contradiction was this session's own
transcription bug (capital arm is `3*tech+4`, not `tech+1`) — see the
item's own 2026-08-24 entry for the full derivation. Ported, real
`ctest` (41/41, unrelated golden-AI cluster still intentionally
`DISABLED`). Tier 1 open: `T1.8`, `T1.13`.

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

- [x] **T1.3 — Full `LAB_521d_3558` cargo/colony sail matrix.** Was OPEN
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
  **2026-08-22 — resumed now that `T4.7`/`T4.8` are closed. Cross-checked
  `T4.8`'s single-dump resolution against all 24 `dosbox-x-dumps/*` saves**
  (byte-identical everywhere — rules out a resident-vs-overlay-swap-buffer
  worry this pass raised and then closed itself), **and fully traced field
  5's callee to a concrete return value: it's a disguised constant.** For
  any non-negative input (i.e. every real unit id this matrix ever passes),
  field 5 (`iStack_16`) always returns exactly `1` — not a per-unit read at
  all. Combined with field 3's already-known noise and `T1.2`'s finding that
  field 2's chain fields are dead for land units (making `iStack_a8` a fixed
  value too for this matrix's calls), **3 of the formula's 5 terms are now
  known to carry zero real per-call variance**, leaving only fields 4/6 as
  possible sources of actual signal — and both were already flagged
  (independently, `T4.8`) as reaching non-numeric side-effecting code
  (a palette-timer reset, a colony bitmap bit toggle), not plain value
  reads. **Not fully closed**: fields 4/6's own `AX` return paths weren't
  traced this pass the way field 5's was — that's the concrete next step,
  and if they turn out equally input-independent the honest conclusion is
  this whole gating formula is functionally constant in the shipped binary
  (which would finally explain why it's resisted a faithful port across
  this many passes — there may be no real design signal left to preserve).
  Don't guess at 4/6 without doing the same full trace.
  **Same pass, continued — field 4 traced too: also a disguised non-signal.**
  Both of its branches reconverge on `MOV AX,[BP+6]` right before return —
  it always echoes back the caller's own input unit id, unchanged, on top
  of its already-known palette-timer side effect. Plugged into the formula:
  with fields 2/3/5 now known fixed/noise and field 4 = raw unit id, the
  whole `iStack_82` term reduces to essentially "negative of the acting
  unit's own array index plus small constants" — no game-design signal,
  dominated by an arbitrary index value. **Recommend treating this matrix
  as very likely dead/non-functional in the shipped binary** (same shape as
  `T1.2`'s "no code to ship" precedent).
  **Same pass, continued, closed.** Traced field 6 the rest of the way and
  it settles the question: its jump-table target (`0x6ef7`) lands
  mid-instruction inside a real, correctly-formed mask-build helper whose
  actual entry is 23 bytes earlier (`0x6ee0`) — recovered `FUN_0000_4fa8`'s
  own prologue for the first time this pass (`0823:4fb0`) to check what's
  actually live in each register at the jump, and found `CX` (which the
  broken entry point ends up using as the write address via `MOV BX,CX;
  OR [BX],AL`) is leftover from an unrelated internal call
  (`FUN_0000_4272`, the convoy-head walker), not the colony-pointer/bit
  arithmetic the jump table's own location implies. So field 6 doesn't
  toggle a colony flag either — it writes to an address this call site
  doesn't control. **This corrects `T4.8`'s own case-6 entry** ("was never
  actually broken, no offset adjustment needed") — that check found the
  recognizable `OR`/`AND` fragment further into the byte stream but never
  verified the literal cited address was itself a real instruction
  boundary, the same mistake `T4.8`'s case-4 finding had already caught
  once for a different case. With all 5 terms now accounted for (field 2
  near-fixed, 3 UI noise, 4 a raw unit-id echo, 5 a disguised constant, 6
  broken/uncontrolled), **no term in this formula carries the per-unit
  signal its variable names imply** — closing on the same basis as
  `T1.2`'s "no code to ship": nothing here for a faithful port to
  preserve, because the shipped binary itself computes nothing meaningful
  at this call site. Full trace: `move_scoring_20e6_full.md`'s 2026-08-22
  update. `ctest` not run — nothing to port means nothing to test.

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
  **2026-08-27 retraction:** the polarity flip above (`0`=AI) was wrong —
  `0`=human (`col1_save.h` `control`, `turn.c:133`, the dispatcher's
  `==0` branch calls `FUN_281f_062c` Human Move/View Pieces). The
  `switch(unit_type)` block IS the AI-mover's automatic decision, and
  its 8 tail thunks go to 8 distinct OVL13 functions (Trade / Establish
  Mission / Denounce / Learn / Speak / **Incite `417e`** / tribute? /
  Attack), not one OVL11 utility. Full table + AI rule set in
  `indian_settlement_4528.md`'s 2026-08-27 section. Linux gap this
  reopens: AI Missionary Incite (T4.5) and AI Missionary Denounce; AI
  military Attack is covered by `ai_euro_land_try_adjacent_village_seize`.

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
  **2026-08-22 — answered: `units_can_enter` does not match it, it's
  simply absent.** Read `units_enter_probe` end to end: for a land unit
  stepping onto empty, unoccupied tribe territory or land near an enemy
  fort/colony (the exact case DOS's `+8` penalty targets), it falls
  straight through to `COLONIZE_ENTER_OK` — no ownership check at all.
  So this isn't "probably already covered," it's a genuine, previously
  unflagged gap. The four DOS accessors behind the penalty are already
  identified (not new RE): `continent_id`, `tile_tribe_owner`,
  `tile_tribe_or_presence` (Linux equivalents exist, though the tribe ones
  are currently `static`/private to `ai.c`'s Indian quiet-scoring gate),
  and "enemy Euro fort/colony owner vs nation" (no Linux equivalent yet).
  Still open: the exact relation-state condition that gates the `+8`
  itself, not traced from `0015bc`'s raw body this pass. Given a working,
  tested substitute already ships and nothing would catch a wrong wire
  (`T3.3`), staying optional fidelity polish — but now with a precise
  3-step next action (trace the condition, expose a non-static accessor,
  add the term) instead of an assumption to verify. Full trace:
  `euro_unit_act.md`'s 2026-08-22 update. `ctest` not run (doc-only).
  **2026-08-22, later same day — step 1 done, and it's not a flat `+8`
  after all.** Force-decompiled `0015bc` fresh (`OVL20_L0000:15bc`, clean):
  the real gate is **asymmetric by nation**. If the candidate tile is
  another tribe's territory, DOS **hard-rejects** it outright for any
  pathing nation. If it's within an enemy fort/colony's zone: an
  **AI-controlled nation also hard-rejects** it, but the **human**
  player's own pathing only takes the soft `+8` penalty — the AI polices
  its own routes strictly, the human just gets discouraged. A naive
  "always +8" port would be wrong both ways (too lenient for AI, too
  strict for the human). Separately, `0x1dd4` (the third scratch global)
  turns out to gate an unrelated terrain-cost-vs-flat-`1` branch (a
  hazard/cached-route heuristic), not part of the ownership question at
  all — resolved, but tangential. **Still not wired**: needs a
  nation-scoped tribe/fort accessor in `units.c` (none exposed yet) and
  the reject-vs-penalize branch keyed on AI-vs-human, not a single cost
  term — a small, well-scoped implementation task now, not an open RE
  question. Full trace: `euro_unit_act.md`'s 2026-08-22 later update.
  `ctest` not run (doc-only).
  **2026-08-22, later same day — tribe hard-reject wired; fort/colony `+8`
  term still not, new genuine unknown found instead of the expected
  quick wire.** Force-decompiled `FUN_1000_88c2`/`88d6` directly. `88c2`
  (tribe/unit occupant) matched its known `accessors.c` equivalent exactly
  — wired as `map_tile_tribe_or_presence` (`map.c`/`map.h`) + a hard
  `continue` in `units_flood_next_step`'s candidate loop when a foreign
  nation occupies the candidate tile (a real, previously-unmodeled gap:
  `units_can_enter`'s land path had no ownership check at all for a bare
  settlement tile). `88d6` (the fort/colony `+8` term) resolved the
  nation/relation half cleanly (Euro `0..3` only, gated on the already-
  named `euro_relation` MET bit `0x40`) but its own tile-flags gate reads
  `layer2_byte & 0x48` — bit `0x08` is the known road bit, bit `0x40` has
  **no established real-DOS-mask meaning** anywhere in this project (not
  the same thing as this project's own synthetic `MAP_LAYER2_FA_ROAD`
  reuse of that bit value). Not wired on a guess — real next step if
  resumed: XREF-sweep `FUN_137f_015e`'s callers (same tool that found
  `T1.11`'s `0x10` write-trigger) to find what sets DOS mask bit `0x40`.
  Full trace: `euro_unit_act.md`'s 2026-08-22 later-same-day update.
  `ctest` 41/41 green.
  **2026-08-22, later same day — the prescribed next step (literal-mask
  grep of `FUN_137f_015e`'s own call sites) is done, exhaustively, and
  `0x40` isn't set through this helper at all.** All 4 of `FUN_137f_015e`'s
  real callers (verified via raw disasm + a flattened-export grep, two
  independent methods agreeing) only ever pass masks `{1, 0x10}`; its
  `FUN_281f_068c` generic-wrapper alias's own fully-resolvable callers only
  ever pass `{1, 2, 4, 0x10}`. No `0x40`/`0x48` anywhere. Bonus, unrelated
  cross-confirmation picked up along the way: mask `0x10` = already-known
  `MAP_LAYER2_PURCHASED` (Indian land-purchase marker), not fort/colony.
  **Real next step, if resumed, is different from what this row said
  last time**: a raw `.asm` byte-pattern search for an inline `OR ,0x40`
  near colony/fort construction code — not another XREF/grep sweep of this
  helper family, that avenue is now closed. Not attempted (out of budget
  this pass). Fort/colony `+8` term stays unwired. Full trace:
  `euro_unit_act.md`'s 2026-08-22 later-same-day addendum. `ctest` not run
  (doc-only). **Deprioritizing this row further** — two full passes now
  spent on one gating bit with a working substitute already shipping;
  next session should default to T1.15/T1.16/T1.17 instead unless this
  specific `0x40` question is explicitly re-requested.
  **2026-08-24 — resumed at this row's own instruction (`0015bc` first),
  plus the other three still-open items (`0015c1`, `0009ae`, `000000`).
  Real, verified progress on all four; none fully byte-exact-ported.**
  (1) `0015bc`'s per-edge cost formula wired into `units_flood_next_step`
  (`units.c`) — re-confirmed from a fresh force-decompile that its real
  gate is `type->movement < 4` (the raw table field), not `0f74`'s own
  `max_mp<2` (a *computed* value) as an earlier pass's prose summary
  implied; the two tiers use similarly-shaped but genuinely different
  formulas, now corrected in the doc. (2) `0009ae` — previously untraced
  entirely — force-decompiles clean (411 bytes, no pcode error): a
  window-local "best immediate step toward goal" picker, structurally
  distinct from the already-confirmed `0015b7`. (3) `0015c1` — never
  actually read raw before (only inferred from its window size) — also
  force-decompiles clean: an 18×18 flood confirmed structurally close to
  `0015bc`'s own shape, gated by a first-stage call into `0009ae`.
  (4) `000000` — still hits the known pcode-error decompiler bug even
  fresh — hand-transcribed from raw disassembly (132 bytes), same method
  as `FUN_5fef_0000`/`OVL12_L0000`; both its callees turn out to be
  already-named accessors (`ocean_or_high_seas`, `continent_id`).
  **Not done**: byte-exact porting of `0015c1`/`0009ae`/`000000`
  themselves (two helper identities, `FUN_1000_8560`/`FUN_1000_856a`,
  and the exact register-argument roles, remain open) — `units_bfs_next_step`
  keeps shipping as the tested substitute. Full trace: `euro_unit_act.md`'s
  2026-08-24 update. Full `ctest` 41/41 green (one real `src/` change:
  `0015bc`'s edge-cost formula).
  **2026-08-24, later same day — `FUN_1000_8560`/`FUN_1000_856a` both
  resolved (same formula, already live); two NEW genuine blockers found
  instead of a port landing.** `8560`/`856a` both resolve to
  `FUN_124c_0040`/`ai_dos_dist` — already shipping as `units_octile`
  (`units.c:3927`) — so `0009ae`'s and `0015c1`'s own scoring/tie-break
  calls need no new logic. Along the way: **`0015b7`/`0015b2`/`0015c1`
  are 5-byte JMPF thunks into resident segment `1000`** (real bodies at
  `1000:a78c`/`a46e`/`a9c0`), not overlay-local bodies as assumed —
  doesn't invalidate prior findings (Ghidra auto-follows the tail call),
  just corrects address attribution. `1000:a46e` (`0015b2`'s target)
  newly identified: a Chebyshev-8-gated shortcut that, when the goal is
  close, calls **`0015bc` itself (already-ported `units_flood_next_step`)
  directly** rather than running the windowed dance — confirms Linux's
  own near/far tiering already mirrors this. `DS:0x1dd2` resolved
  (domain-flag relay between `1000:a46e` and `0015c1`). **Two new,
  previously-unflagged blockers, not restatements of old ones**: (1)
  `FUN_1000_1e7b`'s `address_mapping.csv` "exact" identity (the generic
  RTLink loader) is wrong for this call context — direct disasm shows a
  real critical-section/task-state primitive (`PUSHF/CLI/...gated on
  CS:0x39e1/0x39de.../POPF`), cross-confirmed against an unrelated
  citation in `euro_goal_orders_0a60_full.md`, but its actual
  return-value semantics are unresolved; (2) the window-local per-domain
  walkability grids `0009ae`/`0015c1` read are never populated anywhere
  in this call chain — the population function hasn't been found.
  Porting `0009ae`/`0015c1` now would mean inventing behavior for both —
  not done, per project convention. `units_bfs_next_step` re-verified as
  the far tier's sole caller (one call site) — kept, not retired. Full
  trace: `euro_unit_act.md`'s 2026-08-24 later-same-day update. `ctest`:
  rebuilt fresh (worktree needed the gitignored `COLONIZE/` assets
  symlinked in from the main checkout to run at all) — 40/41 green;
  the one failure (`unit_ai_king`) is a pre-existing baseline condition,
  not a regression (zero `src/` files touched this pass). **Row stays
  open** — real next step if resumed: `FUN_1000_1e7b`'s true semantics,
  and finding what populates the `-0x790a`/`-0x7a18` walkability grids
  (likely inside `0f74` itself, not chased this session).
  **2026-08-24, third pass — both blockers substantially resolved via
  static tooling (force-clear/XREF/custom operand-scan, no live capture
  needed); one correction to the prior pass's own finding; still no `src/`
  change.** (1) `FUN_1000_1e7b` hands back nothing real — confirmed `void`
  at its own address (a critical-section guard around `FUN_1000_40a2`/
  `FUN_1000_1d4f`, unrelated to pathfinding), and, **correcting** the prior
  pass: the "`1000:a46e` Chebyshev-8-gated shortcut... calls `0015bc(0)`"
  citation was a stale-decompile misattribution — force-redecompiling
  `1000:a46e` fresh shows a trivial `FUN_1000_1e7b();FUN_0000_084a();`
  relay, and `0015bc`'s real 4 XREF callers don't include it. The *real*
  Chebyshev-8 logic is genuine DOS code, just living elsewhere: an
  uncatalogued function inside `OVL20_L0000` itself (~offset `0x8f0`-`0x9ab`,
  one of `0015bc`'s real callers) whose own entry/caller aren't pinned down
  yet. (2) The "`-0x790a`/`-0x7a18` stack-relative" grids are actually
  fixed `DS:0x86f6`/`DS:0x85e8` absolute tables (Ghidra mis-rendered a
  `[BX+SI+disp]` indexed read as a bogus giant-negative BP-relative local).
  A full displacement-operand sweep of the resident block plus all 31
  overlay blocks found their one populator: `FUN_OVL21_L0040__0007ef`
  (DOS segment ≈`67f4`, not previously catalogued) — `memset`s both via
  the known `FUN_0000_df7e`, then a step-4 row/col/direction loop fills
  them (explains the previously-unplaced "×4-scaled quantities" note), and
  its tail also refreshes two unrelated per-nation aggregate tables —
  reads as a broader AI turn-start cache refresh, not pathfinding-only.
  Its own caller is not yet found (0 XREFs) — needed to know whether it
  runs once per AI turn or once per pathfind call before this could be
  safely modeled in Linux. **Row stays open** — both remaining sub-items
  (the shortcut function's own entry/caller; `0007ef`'s own caller) are
  static-analysis questions, not live-capture ones. No `src/` change (the
  `a46e` correction doesn't touch any shipped code). `ctest` not re-run
  (doc-only pass, zero `src/` touched). Full trace: `euro_unit_act.md`'s
  2026-08-24 third-pass update.
  **2026-08-24, fourth pass — both of the row's own two open leads
  resolved via static tooling (live overlay disasm + canonical flattened-
  export cross-reference, no live capture needed); no `src/` change (still
  nothing safe to wire).** (1) **Chebyshev shortcut's own entry+caller:
  found.** Real entry is `OVL20_L0000:0x906` (`ENTER 0xc,0x0` … `RETF
  0x6`, 0x906-0x9ab, reached by plain linear fallthrough after the
  previous function's `RETF`+one `NOP` pad byte at 0x905 — same
  "createFunction fails at the cited offset because the true entry is a
  few bytes earlier" pattern as `0007ef` below) — created as a real
  function in the `OverlayTest` project now (`FUN_OVL20_L0000__000906`).
  Cross-confirmed two independent ways: the live force-redecompile matches
  the already-known citation (`|x-goal|<8 && |y-goal|<8` gate, writes
  `DS:0x1dd2`/`0xa14e`/`0xa14c`/`0x1dd6`, calls `0015bc(0)` at its own
  `0x98f` — one of `0015bc`'s 4 known XREFs), and the canonical flattened
  export's `FUN_6662_0906` (`viceroy_unpacked.c:104150`) decompiles to the
  structurally identical body. **Caller, traced through the full RTLink
  relay chain**: `FUN_OVL20_L0000__0009ae`'s own body issues a near `CALL
  0x0015b2` (at both its "stay" and its 8-neighbor validation sites) →
  `0x15b2` is the already-known 5-byte `JMPF 1000:a46e` thunk →
  `1000:a46e`'s own real body (already known from third pass) is the
  2-instruction relay `CALLF 1000:1e7b; JMPF 0000:0906` → that `0000:0906`
  target is **not** a literal resident-space address (which would sit in
  IVT/BDA territory and can't be real code) — it's DOS's "whichever
  overlay is currently loaded in the shared window, offset 0x906"
  convention, and since this whole chain only ever runs from code that is
  itself part of `OVL20_L0000` (`0009ae`), that window necessarily already
  holds `OVL20_L0000` — i.e. it resolves to the real shortcut at
  `OVL20_L0000:0x906` found above. `FUN_1000_1e7b`'s own call inside the
  relay is unconditional and its return value unused before the tail
  `JMPF`, matching third pass's own "1e7b's return doesn't gate this path"
  finding — nothing about that earlier open question blocks this chain.
  (2) **`0007ef`'s own entry+caller: found, with a correction to the cited
  offset.** The true function entry is `OVL21_L0040:0x7d8`, not `0x7ef`
  (`0x7ef` was only the specific instruction offset the prior pass's
  operand-scan cited for the table *write*, not the function's own start —
  created now as `FUN_OVL21_L0040__0007d8`, 0x7d8-0x9e6, body confirmed
  identical to the earlier `0007ef`-cited description). Traced via the
  canonical flattened export (`viceroy_unpacked.c`, grep for the unique
  symbol name — exhaustive across the whole file, not a sampled search):
  its RTLink thunk (`FUN_2a1f_07ea`) has **exactly one call site project-
  wide**, inside `FUN_75c2_235c` — and that function is not a per-turn or
  per-pathfind routine at all, it's a **one-time new-game/scenario-setup
  state-reset** (zeroes ~15 `DS:` globals, seeds 16 RNG slots, resets
  per-nation relation tables). `FUN_75c2_235c` is itself reached (via its
  own thunk) from exactly 2 call sites, both inside `FUN_75c2_2778` — a
  dialog-driven new-game/load-game/scenario-selection wizard screen
  handler (catalog-id dialog calls, scenario-file-existence checks). This
  **corrects** the prior pass's "reads as a broader AI turn-start cache
  refresh" characterization — the evidence points to a one-time
  new-game/map-setup populator, not a per-turn one. **New, honestly-flagged
  open question this raises** (not resolved, not blocking): if the
  walkability grids are genuinely written only once at new-game setup and
  never again (confirmed: the exhaustive grep found no second call site),
  it's unclear whether DOS's runtime pathfinder reads meaningfully-live
  data from them mid-game or a map-gen-time-only snapshot — doesn't change
  the port decision either way (Linux has no equivalent one-time new-game
  hook regardless, so the existing `units_can_enter()`-based substitute
  stays the right call), so not filed as a blocking Tier-4 item, just
  flagged for whoever next touches this. **Bonus, tangential finding, not
  chased further**: while mapping `0009ae`'s own body, its *second*
  `CALL 0x0015b2`-chain sibling call turned out to sit inside a **third,
  previously uncatalogued function** at `OVL20_L0000:0xb4e` (reached the
  same way — linear fallthrough after `0009ae`'s own `RET`, not a `CALL`)
  whose body matches every structural detail this doc's earlier passes
  attributed to "`0015c1`, force-decompiled" (18×18 flood, first-stage
  call into `0009ae`, same popup-tail catalog id) — while `0x15c1` itself
  is independently confirmed (raw disasm, segment field literally `1000`
  in the JMPF bytes, not subject to the "current overlay window" trick
  above) to jump to genuine resident address `1000:a9c0`, which is a
  *different*, real function (also calls `FUN_1000_1e7b`, but its own
  decompile shows `unaff_BP`-relative locals — a probable bad-boundary
  artifact, same class as `684c_08c0`/`15eb_1d4c`, not chased this pass).
  Whether `0xb4e`'s body is `0015c1`'s real target via some further hop
  from `1000:a9c0`, or a fully separate sibling copy, is undetermined —
  flagging for a future pass, not this row's own two asks. **`src/`: no
  change** — `0906`'s only caller (`0009ae`) still isn't itself portable
  (still depends on the same walkability-grid semantics question above),
  so there's no safe place to wire the newly-identified shortcut into
  yet; nothing here was invented. **Housekeeping**: filled in 3
  previously-`gap` `address_mapping.csv` rows now that live evidence
  backs them (`FUN_2a1f_027e`→`FUN_1000_a46e`, `FUN_6662_0906`→
  `FUN_OVL20_L0000__000906`, `FUN_67f4_0088`→`FUN_OVL21_L0040__0007d8`,
  all `exact`); also created both new functions for real in the
  `OverlayTest` Ghidra project (persisted) so a future pass doesn't hit
  the same "createFunction fails at the cited offset" wall. `ctest` not
  re-run (doc-only pass, zero `src/` touched, confirmed via `git status`).
  Full trace: `euro_unit_act.md`'s 2026-08-24 fourth-pass update.
  **2026-08-26 — third negative pass on the parked fort/colony `0x40` bit,
  a different search method than the first two, still no candidate.**
  Tried the row's own suggested next step (a raw byte-pattern grep for an
  inline `OR ,0x40` near colony/fort code) via the flattened export instead
  of raw `.asm`: every literal `| 0x40` in `viceroy_unpacked.c` (55 sites)
  checked by hand. All either CRT/stdio internals (Watcom's `_flag` byte at
  struct offset `0x27bb` — file I/O, not game state; register-flag
  reconstruction idioms; a critical-section byte) or unrelated known
  fields (`0x5382`-`0x5387` king/independence bits, `0x3148`/unit-record
  bits, `0x8d4e+3` settlement flags already covered elsewhere) — **zero**
  land on the layer2 tile array. Cross-checked the other direction too:
  traced the layer2 array's own base/stride (`FUN_137f_012a`, `DS:0x160`
  + `DS:0x853a`-stride) and grepped all 118 uses of the stride constant
  for a bypass write straight into that array outside the already-ruled-
  out `FUN_137f_015e`/`FUN_281f_068c` wrapper family — none found either.
  **Net: three independent search strategies (XREF sweep of the setter
  wrapper, literal-mask text grep, array-base bypass check) have now all
  come back negative for this one bit.** Stays PARKED; a live DOSBox-X
  write-breakpoint on the layer2 array (`BPM` at `0x160`+offset while
  founding/besieging a fort) is the only remaining avenue that hasn't been
  tried. No `src/` change, `ctest` not run (doc-only, confirmed via
  `git status`).
  **2026-08-27 — closed statically, no `BPM` needed: layer2 `0x40` =
  plowed.** The DOS layer2 plane is the save's `mask` layer, so the bit
  was readable from existing saves: zero in every mapgen/early save,
  land-only, open-land classes 2–6 only, grows with Pioneer work across a
  game's autosaves — and `col1_bridge.c` had been importing that very bit
  as `MAP_IMPROVE_PLOWED` all along. `88d6`'s `& 0x48` = road|plowed
  ("improved tile"), not a fortification marker. Term wired in
  `units_flood_next_step` (AI reject / human +8, MET-gated, skipped
  without col1); `MAP_LAYER2_FA_ROAD` → `MAP_LAYER2_PLOWED`. `ctest`
  44/44. Method lesson: a runtime plane that is also a save layer can be
  decoded from save statistics — check that before hunting writers.

  **2026-08-27 — far tier ported (`units.c`).** Coarse 15×18 quarter-resolution walkability grids (DS:0x85e8 land / 0x86f6 sea, 8-bit direction masks) are rebuilt lazily per map from terrain (`units_coarse_build`; the populator `0007d8` body is not decompiled — cell = any domain tile in the 2×2 sample block, bit d = windowed domain BFS between sample points, documented as a consumer-side reconstruction). `0009ae` = `units_coarse_snap`, `0015c1` = `units_coarse_waypoint` (BFS from the goal cell, pick U's lowest-cost neighbour, octile tie-break, `000000` probe), `0f74`'s far arm = flood toward the waypoint, else toward U's probed centre, else greedy. `units_bfs_next_step` retired from the tier (kept compiled). Sea probe's continent==1 term dropped (Linux water has no continent id). `ctest` 46/46, goldens unchanged.
  **2026-08-27, latest — populator `FUN_OVL21_L0040__0007d8` decoded from `viceroy_overlays.asm` (OVL21_L0040:7d8-9e6) and the grid now follows it:** per domain, `memset(0x10e)`, cells at x=1..57/4, y=1..69/4 via the `000758` 2×2 probe (returns the body id; sea needs body 1), for d in 0..3 the neighbour cell must probe to the *same* body id and the `1000:a46e`/`0906` relay must answer 0<cost<8 (a 1..7-step path); bit d set here, bit (d+4)&7 on the neighbour. Tail recomputes `0x945e`/`0x85c8` per-continent tile tallies (unrelated). `units_coarse_build` mirrors all of this; only the `0906` cost is approximated by a windowed domain BFS.
- [x] **T1.9 — Indian mid-game quiet scoring (goods/missions/capital
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
  **2026-08-22 — worth re-checking, not re-blocked.** `T4.7` and `T4.8`
  both closed the same way `T1.1`'s other fields did (static, no live
  session). Before treating `iVar14`/`iVar18` as still stuck: find which
  specific field numbers those two `FUN_281f_08bc()` calls actually pass
  and check them against the now-resolved case table (field `0xc` generic,
  field `2`/`4`/`5` disguised constants per `T1.3`'s 2026-08-22 findings,
  field `3` a keyboard-flush no-op, field `6` still genuinely unresolved).
  Not done this pass — flagging that the door may already be open rather
  than leaving the stale "blocked" framing standing.
  **2026-08-22, later same day — done, and the formula collapses.** Pulled
  `iVar14`/`iVar18`'s real arguments straight from `viceroy_unpacked.asm`
  (`LAB_521d_52ca` onward — the flattened C export hides them, same
  zero-arg thunk-rendering issue already flagged for `FUN_291f_0a14`
  above): the three `FUN_281f_08bc()` calls here pass fields **2, 2, 0**.
  Field 0 is `T1.3`'s already-resolved disguised constant (always `1` for
  a non-negative unit id) — `iVar18 = 1`, certain. Field 2 is the known
  transport-chain node exchange; combined with `T1.2`'s "chain fields
  never actively maintained" finding, its raw return is realistically
  always `<1` in practice, which this block's own code already clamps to
  `1` regardless (`if (iVar14 < 1) iVar14 = 1;`) — so `iVar14 = 1` with
  high but not airtight confidence (open only on whether Braves' chain
  fields are as inert as land units', not independently re-checked).
  **With both resolved, the formula collapses to `iStack_e8 = 2*iVar20`**
  (divisor already `1` for Braves) — `iVar20` being the already-ported
  `combat_apply_1b0e_peels` strength value. Concrete and portable now, not
  an open field-mapping task. **Not wired**: still no golden exercises
  `colony_count>0` for a Brave to verify a port against, per this item's
  own original note — documenting per that note rather than shipping
  unverifiable code. Full trace: `quiet_brave_scoring.c`'s 2026-08-22
  update. `ctest` not run (doc-only, no `src/` touched).
  **2026-08-22, later same day — the rest of the title's own scope
  ("goods/missions/capital pull") traced too, all already-known
  ingredients.** After the base value above, the real formula multiplies
  by `3` if the candidate tile has a Euro settlement (the actual "capital
  pull" term), `<<1` if it has a native village, `>>1` if the mover is the
  crown nation with no settlement bonus and is already at its own home
  colony (`DS:0x8db8`, already named elsewhere), and `*3` again if the
  Brave-type flag is set and the candidate continent's G-table stance
  equals `4` (the already-ported `ai_euro_continent_stance_at`, same
  table `euro_g_table_0a60.md` fully resolved). A Missionary-only
  zero-out sits between these but is unreachable for the Brave type this
  file scopes to. **Every term is now named and reuses existing project
  accessors** (`euro_settlement_owner`/`indian_settlement_owner`/
  `continent_id`, all already catalogued/known) — nothing left to RE for
  this item's own stated scope. Still not wired (same no-golden reason).
  Full trace: `quiet_brave_scoring.c`'s 2026-08-22 later update. `ctest`
  not run (doc-only).

  **2026-08-27 — wired (`ai.c` `ai_native_foreign_euro_pull`).** The `LAB_521d_52aa` arm replaces the picker's blanket foreign-owner reject for Euro-owned tiles: gate = owner<4, Indian side not at PEACE with it (`indian[].euro_diplo & 0x40`) or a Privateer on either side, and (!WoI or owner is the human). Formula per the trace (`1b0e` strength via `combat_land_engage`/open-field base, stack cost/military terms, colony ×3, village <<1, crown-at-home >>1, 1000 clamp, `-999` under 12 else `+e8*4`). Stance-4 term inert for Indian nations (Euro-only table). `golden_ai_turns` unchanged (no such tile in TURN1→2).
  **2026-08-27, latest — two corrections from the goldens.** (1) The `52aa` pull is Euro-mover only: TURN2→3 (Aztec Brave at (47,15) beside an unmet Dutch unit) shows DOS does not pull a quiet Brave; gated to `nation<4`. (2) Real bug found while chasing the 3 parked Braves: `ai_mask_fa_flags` read layer2 bit 0x08 (Linux rumour-cleared stand-in) as the DOS road bit, so the river/road `+1` pair term fired spuriously. Now reads `map_tile_has_road`. **`golden_ai_turns` TURN1→2 passes**; TURN2→3 fails on one Euro Soldier (nation 1 at (49,37), DOS steps SE along its (56,42) goto, Linux re-plans an AI_MOVE to (49,36)) — a Euro land-arm goal question, not Brave scoring.
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

- [x] **T1.13 — KINGGALLEON2 (non-Cortes royal-galleon share) re-attempt.**
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
  exhausted — **2026-08-22 reassessment, list refreshed a third time:
  T1.3, T1.9, and T1.14 are now also done** (all closed same day, see the
  top-of-Tier-1 note). Still genuinely open Tier 1 items: **T1.8**
  (pathfinding flood-fills / tribe-zone gate, deprioritized but not
  closed), **T1.15** (`41f2_0294` worth-score port), **T1.16** (`2820`
  Haggle/hard-bargain port), and the new **T1.17** (`FUN_15eb_28c8`
  work-plot job-scoring port). This was just the last item a given session
  reached, not the end of the tier.
  **2026-08-22 — re-attempted, still PARKED, but the search space
  narrowed.** Both of the 2026-08-20 pass's suggested leads ruled out:
  the `48d3`/`europe_finish_bridge.md` neighborhood is docks/landfall
  bookkeeping, unrelated; the cited `FUN_1000_8842` CHOICE-dispatch
  function has no traceable evidence trail anywhere in this project
  (zero hits in either flattened export) — flagged as unreliable, not
  assumed correct going forward. **New, unexplored lead**: this is a
  Crown-initiated offer, the same *kind* of mechanic as audience/
  congress/mercenary-hire (all in the `38fd` overlay per `king_ref.md`),
  not `48d3` — nobody has searched `38fd` for this specifically. Not
  searched this pass (60+ un-attributed "gap" functions in that overlay,
  too large to blind-sweep without a narrower hint). Full trace:
  `euro_unit_act.md`'s 2026-08-22 update.
  **2026-08-24 — the `38fd` CHOICE(3 STRING, 0 NUMBER) sweep this row asked
  for was, in fact, already run later the same day as the note above (this
  row itself was just never updated with the result) — found via
  `src/core/founding_fathers.c`'s own `founding_fathers_cortes_free_king_
  galleon` comment ("Phase 3: 38fd CHOICE 3 STRING / 0 NUMBER negative...
  Phase 5: 38fd overlay sweep... no match"), cross-confirmed by matching
  Phase-5-negative status lines added the same day to `roadmap.md`,
  `manual_gap.md`, and `ai_transcription.md`. That result predates this
  row's own text by a few hours and was never propagated here — pure
  doc-sync gap, not unfinished research. Rather than trust the comment at
  face value (this item's own history of unverifiable citations, e.g. the
  retracted `FUN_1000_8842`, earns that skepticism), independently re-ran
  the same search this session: mapped all 81 `38fd` functions
  (`viceroy_unpacked.c:58695-68762`) and every CHOICE-adjacent call site in
  that span (`FUN_291f_0182` CHOICE-read, `FUN_1d1d_07e4` string-template
  format, `thunk_FUN_291f_0ae0` notify) to its owning function, including
  the overlay's two giant outliers (`FUN_38fd_3694`, 2886 lines, and
  `FUN_38fd_4f6e`, 2205 lines — both flagged `WARNING:` corrupted in spots,
  the likeliest place a signature-based text scan could have missed
  something). Neither reads as Treasure/Galleon: `3694` matches
  `FUNCTION_CATALOG.md`'s existing "dock immigrant info / embark bark
  dialog" label on inspection (its many `unit+0x3146` type checks span
  ship types `0xd-0x12` and a few land types, never Treasure's `'\n'`/
  `0x0a`); `4f6e` reads as warehouse/goods-overflow accounting (`nation+
  0x92`/`0x94`/`0x95`/`0xb6`/`0x9a` fields), not "Europe keyboard/hotkey
  dispatcher" as currently inferred-labeled, but not Galleon-shaped either
  way. **Confirms the negative independently — `38fd` is genuinely
  exhausted, both by this session and the prior one.**
  **Real, new correction found in the process**: this row's own "audience/
  congress/mercenary-hire... all in the `38fd` overlay per `king_ref.md`"
  premise is half wrong — re-reading `king_ref.md`, only the tax-audience
  pair (`38fd_5be8`/`38fd_3dc8`, plus `38fd_5e52`) is actually `38fd`; the
  declare-gate/congress citation (`2564`/`1a26`) and the mercenary-hire
  pair (`2022`/`2244`) that doc cites live in **`43f7`** (the doc's own
  title: "King / REF / independence (`43f7`)") — a different, much smaller
  overlay (21 functions vs `38fd`'s 81). Since KINGGALLEON2 is structurally
  closer to those Crown-proposal mechanics than to the audience/tax-teaparty
  pair, `43f7` is arguably the better-motivated candidate `king_ref.md`
  actually points to. Checked it fully this pass: 18/21 of `43f7`'s
  functions were already attributed in `king_ref.md`; the 3 that weren't
  (`FUN_43f7_0082`, `_0108`, `_0188`) are now read and ruled out too —
  `0082` is a small difficulty-scaled weight/priority-constant table (no
  dialog at all), `0108` is per-nation elimination/reset bookkeeping
  (kills that nation's units, sets `nation+0x543f=2`), `0188` disposes a
  losing/eliminated nation's ships with a single-string status message (one
  `STRING` arg, no CHOICE, no second option) — none is a 3-STRING/0-NUMBER
  CHOICE, and `43f7` has no corruption warnings anywhere in its 21
  functions, so nothing here needs live Ghidra re-verification. **`43f7`
  is now also fully exhausted (21/21 functions accounted for), not just
  `38fd`.** Net: both King-turn overlays plausibly reachable from
  `king_ref.md`'s own citations are now genuinely searched and negative,
  not merely asserted negative. No new candidate overlay identified this
  pass. **Stays PARKED** — a future re-attempt should not repeat either the
  `38fd` or `43f7` sweep; if picked up again, needs either a genuinely new
  overlay hypothesis or a live DOSBox-X capture (e.g. a hang-dump trapped
  at the moment a non-Cortes player accumulates Treasure with no Galleon,
  the same method that pinned `417e`'s real caller args). Doc-only pass,
  no `src/` touched, `ctest` not run.

  **2026-08-27 — FOUND and ported, static-only.** Every text search
  missed it because the tag is assembled at runtime: `FUN_5fef_1908`
  (catalogued as "Treasure ransom/loot", and explicitly ruled out on
  2026-08-20 on that basis) does `strcpy(buf, DS:0x1bed "KINGGALLEON");
  strcat(buf, has_FF(10 Cortes) ? DS:0x1bf9 "3" : DS:0x1bfb "2")`, runs the
  CHOICE, and returns untouched on decline. Share: `pct = tax; if !Cortes:
  pct = max((difficulty+10)*5, 2*tax); cap 90` → `royal_money(+0x22) +=
  value*pct/100`, `gold += rest`, `nation+0x26 += rest` (so `unknown24_pad`
  is *not* dead — write-only accumulator), then `@LOOTCASH`; WoI declared →
  no King, full value, `@CASHTREASURE`. Trigger is `FUN_465b_0000`
  (`viceroy_unpacked.c:75800`): **human** nation only (`0x543f==0`),
  Treasure moved onto own colony with flags bit `0x40` (coastal), skipped
  when the per-nation unit-type count table (`-0x6db4`, stride 0x13)
  says the nation owns a Galleon (type `0xf`) and it lacks Cortes. The
  key unlock was general: the "unrecoverable numeric popup ids" are **DS
  addresses of GAME.TXT tag-name strings** (`0x1866`="INDIANCITY",
  `0x1340`="MERCENARIES", …; EXE offset = 121248 + addr) — see
  `popup_string_resolver.md` + `docs/popup_tag_ids.md`. Port:
  `units_king_galleon_share_pct` / `_offer_coastal_treasures` /
  `_apply_popup` (`units.c`), tag `AI_POPUP_TAG_KING_GALLEON`, wired at
  human turn end (`turn.c`, replaces the Cortes-only auto-cash there) and
  applied in `game_loop.c`; AI callers of
  `units_cortes_cash_coastal_treasures` untouched (DOS never runs this
  for AI). Test in `test_units.c`. `ctest` 44/44.
- [x] **T1.14 — Decode `DS:0x2f76` record columns `+3` (colony-founding
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
  [`terrain_yields.md`](terrain_yields.md) "DS:0x2f76 terrain-class
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
  **2026-08-22 — real function found, via the `.asm`'s own XREF rather than
  another force-redecompile guess.** Grepped `viceroy_unpacked.asm` directly
  for the `[BX+0x2f7a]` instruction bytes (skips the flattened C export
  entirely) — found it tagged `caseD_10`, with a genuine `XREF[1]:
  129f:00b7(j)` cross-reference and physically living in the **`15eb`**
  overlay module (same one `FUN_15eb_18ec`/`FUN_15eb_17fa`, already-
  documented field-composition/special-resource functions, belong to).
  Walking back to the nearest real function label lands on **`FUN_15eb_28c8`**
  — already in `FUNCTION_CATALOG.md` ("Score/assign best work-plot job for
  colonist," 254 lines, calls `FUN_15eb_18ec`) but never linked to a `.c`/
  `.md` file. So `+0x4` isn't an Indian-village growth term after all — it's
  a labor/travel penalty subtracted from a colonist's candidate work-plot
  score (worse for higher-effort terrain), with an extra low-difficulty
  forest penalty when no settlement is adjacent. **`+3`'s dead-code verdict
  above independently reconfirmed** by this session's `T1.3` closure (same
  `T1.2` "no code to ship" mechanism). **`+4` identified but not wired** —
  `FUN_15eb_28c8` itself has no Linux port; porting the whole 254-line
  work-plot-assignment function is a new, separate, unscoped item, not
  something this narrow identification question required. `+0xe` still
  unidentified — not attempted this pass. Checking this item off on that
  basis: both of T1.14's original questions (`+3`, `+4`) are now answered,
  even though `+0xe` and the full `FUN_15eb_28c8` port remain open
  elsewhere. Full trace: `terrain_yields.md`'s `+0x4` row. `ctest` not run
  (doc-only, no `src/` touched).

- [x] **T1.15 — Port `FUN_41f2_0294`'s village worth-score formula.** New
  2026-08-22, split out of `T4.2` now that its semantics are fully known
  (7-term word-sum, see that row) and only field-naming + porting remain —
  no live capture needed for any of it. Replaces the flat-`15`
  `ai_indian_152e_worth_cap_stub` in `src/core/ai.c`, which currently gates
  the same `152e` capital-growth branch this session's `T4.3` root-cause
  investigation traced a real (if unrelated) golden divergence into.
  Remaining static sub-tasks per `T4.2`'s own list: name `nation+0xc`/
  `+0x18`/`+0x2a`/`+0x2c` against `col1_save.h` (cross-check
  `[[king-audience-tax-delta-resolved]]`'s already-resolved nation-struct
  fields first — several of these may already have names), trace term 2's
  not-yet-found sub-scan for its `x` input, identify `FUN_281f_07b4`'s
  25-count target list, and name `DS:0x539e`/`0x539c` (scan bounds) +
  colony-pointer `+0x1f`. Preserve the real quirk `T4.2` found: only the
  low byte of the word-sized total survives at the `152e` call site
  (`cmp al,[bx+4]`) — values ≥256 wrap silently, byte-truncate on port,
  don't widen it "for correctness." Also resolve the open
  `nation+0x2a`/`+0x2c` vs. `difficulty.md`'s existing `gold` naming
  contradiction (`T4.2` flagged, didn't resolve) before trusting either.
  **2026-08-22 — real progress, not closed.** Resolved via `offsetof` +
  reading the real asm at the term-7 call site directly (not the flattened
  export's botched empty-arg rendering): `nation+0xc` = `liberty_bells_total`,
  `nation+0x18` = `villages_burned`, `DS:0x539e`/`0x539c` were already-named
  (`VICEROY_DS_COLONY_COUNT`/`_UNIT_COUNT`, just never cross-referenced into
  this function before). The `+0x2a`/`+0x2c` "contradiction" is fully
  resolved, not merely restated: they're the low/high halves of one 32-bit
  read, `sizeof(ColonizeCol1Nation)==0x13c` matches `difficulty.md`'s own
  citation exactly, `col1_save.h`'s existing `gold` field IS this field —
  term 7 = `gold>=1000 ? gold/1000 : 0` (real gate read off the raw
  `CMP`/`JGE`/`JG`/`JNC` chain), not a distance/coordinate read as
  previously guessed. Full trace: `ai.c`'s `ai_indian_152e_worth_cap_stub`
  header comment, 2026-08-22 update. **Still open, not attempted**: term
  2's `x` sub-scan, `FUN_281f_07b4`'s 25-count target list, and the
  neighbor-scan's real bound field (the doc's own "colony-pointer `+0x1f`"
  framing doesn't match the actual asm — that literal byte pattern doesn't
  occur in the function body; not re-derived this pass). Not a same-session
  finish — real port still needs those two remaining pieces. `ctest` not
  run (doc/comment-only change).
  **2026-08-22, continued same day — bigger structural finding, changes
  confidence for the whole function.** `FUN_41f2_0294` turns out to be one
  of at least 4 external entry points into ONE shared code block
  (`_0266`/`_0280`/`_026e`-ish/`_0294`, each with its own `4d56:XXXX` XREF,
  none with its own `ENTER` — same DOS-compiler frame-sharing trick already
  seen for `394e`'s callers). Entry `0266` initializes `[BP-0x5e]` before
  falling through — the only init anywhere in the block — but the two call
  sites this item actually needs (`4d56:0086`/`4d56:1547`) `CALL` straight
  into `0294`, skipping that init, and both callers' own `ENTER` sizes
  (`0x6`/`0x18` bytes) are far smaller than the `-0x5e..-0x7e` offsets read
  as "locals." **Term 2's `x` (`[BP-0x6a]`) is confirmed never written
  anywhere in the shared block and unreachable from either real caller's
  declared frame** — same "no real design signal" verdict class as `T1.3`'s
  five dead terms, not a formula to port. Term 3's callee resolved (force-
  decompiled `FUN_0000_9810` directly): it's `bool(euro_nation 0..3,
  bit_index)`, a per-nation FF/feature bitmap test — confirms the existing
  `ai_indian_152e_ff_bit_stub` comment, and corrects this row's own
  "nearby-unit count" framing (the 25-iteration loop only ever produces a
  hit for `i=0..3`, not a spatial scan). Term 1's own scan-bound field
  still not re-derived — it gates on the same uninitialized-for-these-
  callers slot family, inheriting the same caveat rather than needing
  separate RE. **Net: terms 4/5/6/7 (nation-struct/global reads) stay
  reliable; terms 1/2/3 are now suspect as modeling deterministic-but-
  meaningless stack garbage rather than designed behavior** — a live trace
  would settle it, but porting them as literally decoded risks shipping
  noise as if it were a real formula. Full trace: `ai.c`'s
  `ai_indian_152e_worth_cap_stub` header comment, 2026-08-22 continued
  update. Real port stays future work — not a same-session task. `ctest`
  not run (doc/comment-only).
  **2026-08-22, later session — the "7-term word-sum" framing itself is
  an undersell.** Read `FUN_41f2_0294`'s real body directly
  (`viceroy_unpacked.c:72085`): both real callers pass a clean 1-argument
  call (the earlier "zero-arg" confusion is specific to `0294`'s own
  declared signature, not its callers). The body is a ~200-line loop over
  every colony that **interleaves the numeric score with conditional
  report-text-line construction** (string-builder calls paired with a
  repeating line-height bump) — shaped like a scrollable Colonial
  Report/prestige screen, not a plain arithmetic formula. Confirms term 7
  again and finds an uncounted 8th term (`nation+0xc` liberty bells,
  `>99` gate, `/100` capped at 100). **Not a same-session port** — bigger
  and structurally different from this row's own scope, needs a fresh
  dedicated investigation (same caution class as `T1.17`). Not resumed
  further this pass — flagging precisely rather than guessing at the
  report-vs-formula split. Full trace: `ai.c`'s
  `ai_indian_152e_worth_cap_stub` header, 2026-08-22 later-session update.
  `ctest` not run (comment-only).
  **2026-08-24 — Ghidra call-target misresolve; real callee unknown;
  stub kept.** Both `4d56:0038` and `4d56:152e` near-call through
  `4c54 → JMPF 2a1f:0410` (loader + `JMPF` offset `0`), **not** into
  `FUN_41f2_0294`. Neighboring thunks in that table land in overlay
  `4d56`, so offset `0` is plausibly `FUN_4d56_0000` (OVL13 start):
  `return (tribe+3 & 4) ? tech+1 : 2*tech+3` with `tech =
  indian[nation-4].tech` (`DS:0x5AD6` stride `0x4e`, +2). Non-capital
  arm == existing `ai_tribe_initial_pop` (TURN1 satellites match
  `pop == 2*tech+3` exactly). **Cannot be the live 152e worth compare:**
  seed-100 capitals have `pop == 2*tech+3` yet still do
  `growth_accum += pop` each turn, which needs `pop < worth`; both arms
  of `4d56:0000` give `worth <= pop` on those rows. So either thunk
  `0410` loads a different overlay than `4d56` at offset 0, or another
  mechanism sets `local_16`. Flat-15 stub retained (matches early
  capital accrual). `FUN_41f2_0092`/`0294` nation-score/report body
  still real — just not this call site. Next: RTLink jump-list / live
  overlay-load for `2a1f:0410`. Cited `ai_tribe_initial_pop` /
  stub comments in `ai.c`. `ctest` not required (comment-only `src/`).
  **2026-08-24, later same day — resolved and ported; CLOSED.** Used
  this project's own established recipe for a `CALLF <loader>; JMPF`
  RTLink placeholder (Ghidra headless `analyzeHeadless ... -postScript
  GhidraDecompileAt.java <space>:<offsetHex>` against the canonical
  `decompiled-colonize`/`VICEROY_OUT_2.EXE` project — same tool used
  earlier in this project for the `0a60`/`153e` thunk tables, see
  `docs/rtlink_decode_v2_gap.md` step 3b) instead of the flattened
  export's `FUN_41f2_0294` misresolve:
  - `4d56:0086`'s near `CALL` raw bytes `E8 CB 4B` decode to target
    `0x0089+0x4BCB = 0x4C54` (Ghidra's own rendered `0x4000:21b4` display
    for this instruction is wrong — a segmented-space rendering artifact,
    confirmed by manually decoding the `rel16` operand).
  - `4d56:4c54` raw bytes `EA 10 04 1F 2A` = `JMPF 2A1F:0410` (again,
    Ghidra's rendered `0x2000:a600` is the same display artifact).
  - Raw bytes at `2a1f:0410`, read directly (not via a stale/cached
    function boundary from a prior probe in the same run — first attempt
    at this address produced a mismatched result from exactly that
    trap): `CALLF 210D:0DAB` (`FUN_210d_0dab`, the RTLink overlay loader)
    + `JMPF 4D56:0000`, a clean 12-byte thunk-table entry. Confirms the
    **already-guessed** target, `FUN_4d56_0000`. (Table entries also
    resolved in passing: `041c`->`4d56:39ea`, `0428`->`4d56:3646`,
    `0434`->`4d56:2154`.)
  The 2026-08-24 "cannot be the live compare" contradiction above turned
  out to be a **transcription bug in that same session**, not a wrong
  callee: decompiling `4d56:0000` fresh gives capital arm `3*tech+4`
  (`ADD CX,AX; INC CX` onto the already-computed `2*tech+3` base), not
  `tech+1` as read then (the base add was dropped in transcription).
  `3*tech+4 > 2*tech+3` for every `tech >= 0`, so `pop < worth` holds for
  seed-100 capitals and `growth_accum += pop` fires every turn — matches
  golden TURN1->2 exactly, contradiction resolved. Ported: stub renamed
  `ai_indian_152e_worth_cap_stub` -> `ai_indian_152e_worth_cap` (no
  longer a stub), reads `ind->tech` and `t->state.capital` directly
  (already-wired fields) instead of re-deriving them through
  `FUN_4d56_0000`'s own redundant `DS:0x54ec`-stride-`0x12` lookup table
  (a separate per-tribe worklist mirroring the same two fields — porting
  it as a second table would just be duplicate state). Result truncated
  through `uint8_t` before return, matching DOS's own `byte bVar5 = ...`
  before the compare (same "byte-truncate on port" convention as the
  `T4.2`/`417e` case). Full diff + derivation: `ai.c`'s
  `ai_indian_152e_worth_cap` header comment. `ctest`: full suite green,
  41/41 (4 golden-AI-cluster tests remain `DISABLED` by pre-existing
  2026-08-19 policy, unrelated to this fix — see `CMakeLists.txt`'s own
  comment there). Informational-only spot check (not part of the gating
  suite): running the disabled `golden_ai_turns` binary directly now
  reports `TURN1->2 ok`; TURN2->3 still diverges on unrelated,
  already-documented gaps (unit-order/goal mismatches, two
  `relation_by_indian` off-by-ones) — expected per that cluster's own
  "guaranteed future diff until AI transcription is further along"
  framing, not a regression from this change. **T1.15 closed.**

- [x] **T1.16 — Port `2820`'s deep Haggle/hard-bargain resume-loop.** New
  2026-08-22, split out of `T4.4` now that both blocking values are
  resolved (per-(nation,cargo) throttle table at `DS:0x84BC..0x84FB`,
  scratch multiplier `0x8dc4`=`0x32` — see that row). `T1.6` already
  established this is reading-and-transcribing already-recovered code, not
  fresh RE (the `iStack_5e==2`/`3` resume-loop branches inside `2820`'s
  own already-clean 595-line body). One caveat to carry into the port,
  not a blocker: `T4.4`'s captured throttle table was byte-identical
  across all 4 nation rows in a single mid-negotiation save despite being
  documented as per-nation — flagged there as "worth a second capture to
  confirm it's not just coincidentally unmodified," still outstanding.
  Port with the captured values, comment the single-capture caveat
  in-line, don't block on a second capture unless the port's own behavior
  turns out to hinge on real nation-to-nation variance.
  **2026-08-22 — attempted, found a real mislabeling in already-shipped
  code, corrected it, did NOT ship the resume-loop itself.** Read the full
  595-line body end to end. Real finding #1: for the AI-controlled path,
  the "resume loop" never actually resumes — `iStack_5e` is recomputed
  fresh each iteration from a value fixed before the loop starts, and only
  the haggle branch (`iStack_5e==2`) sets the continue-flag; AI never
  produces `2`. So the AI side is a **single deterministic accept-or-refuse
  decision**, not a real loop — simpler than the row's own framing assumed.
  Real finding #2, bigger: force-decompiling `LAB_002bbc`'s own callees
  (`FUN_1000_8cdc`/`FUN_1000_8f48` → canonical `FUN_0000_902c`/`_8f68`)
  proved `LAB_002bbc`'s accept branch **removes cargo from the Euro unit
  and credits the Euro nation's gold** — the opposite direction from what
  `ai_contact_auto_trade`/`ai_contact_2820_ai_buy_price` already implement
  and attribute to it. The shipped code's actual gold/cargo direction
  matches `LAB_002e92`'s accept branch instead (verified: explicit
  `gold -= price` there, paired with a real `FUN_1000_8f48` cargo-add).
  **Not a bug in shipped behavior** — the direction it implements is
  internally consistent and already tested — **just a wrong DOS-branch
  citation**, now corrected in `ai_contact.c`'s comments (3 call sites).
  The real `LAB_002bbc` mechanic ("Euro nation sells cargo to natives, gets
  paid") is a genuinely separate, currently-unported behavior — its own
  dispatch condition (when DOS picks sell-to-tribe vs buy-from-tribe,
  traced to somewhere in `2820`'s lines ~189-283, not yet fully mapped)
  needs nailing down before it's safe to wire, so not attempted this pass
  rather than guessed at. Full trace: `indian_trade_2820.md`'s 2026-08-22
  addendum. `ctest` run after the comment-only `ai_contact.c` edits —
  green, no behavior changed (verified via `colonize_core` rebuild +
  full suite).
  **2026-08-22, continued — dispatch condition traced (turned out simple),
  a real gold-direction bug found and fixed with user confirmation.**
  `iStack_c8<0` (unit carries nothing) → `LAB_002e92` (tribe sells TO the
  unit); `iStack_c8>=0 && iStack_8==0` (unit has cargo, AI-controlled) →
  `LAB_002bbc` (unit sells TO the tribe) — human-with-cargo falls through
  to neither, real destination not traced. Force-decompiled `LAB_002bbc`'s
  real callees (`FUN_1000_8cdc`/`FUN_1000_8f48` → canonical
  `FUN_0000_902c`/`_8f68`): confirms `LAB_002bbc` removes cargo from the
  unit and **credits** Euro gold. `ai_contact_auto_trade`'s stock-drain
  has always structurally matched this (unit cargo leaving hand); its gold
  **debit** — added in an earlier session citing this same branch — was
  backwards. Confirmed independently by the human CHOICE's own UI text
  ("The %s offer N gold for your trade goods"), which already described a
  credit while the code debited. **Fixed with explicit user sign-off**
  (asked before touching live, tested economic code): `ai_contact.c`'s
  `nat->gold -=` → `+=` in both `ai_contact_auto_trade` branches, function
  renamed `ai_contact_2820_ai_buy_price` → `_sell_price` to match, all
  header comments corrected (including the ones this pass itself wrote
  earlier the same day — they'd assumed the debit was correct). New
  regression test added (`test_ai_contact.c`): asserts gold increases by
  exactly the locked price on trade accept — passes, locks the fix in.
  `ai_transcription.md`'s `2820` row corrected to match (was repeating the
  same mislabeling). Full `ctest` 41/41 green after rebuild. **Not done**:
  the AI-silent refuse decision (`LAB_002bbc`: relation-shaped value
  `aiStack_d6[0] > 0x31` → refuse-with-penalty instead of always
  succeeding) — plausibly already covered by the existing
  `alarm_by_player[e] >= 50` gate at this function's own top (suspiciously
  close threshold, 49 vs 50), not confirmed byte-exact, not added as a
  possibly-redundant/wrong-polarity second gate. The human-with-cargo
  dispatch destination and the deep Haggle/hard-bargain resume-loops
  (mostly moot now that AI never actually loops — see the earlier note)
  remain open if this is resumed.
  **2026-08-22, later same day — full rewrite per user decision, item
  closed as far as it safely goes.** User's call on the hybrid-mechanic
  question above: port the real per-unit shape structurally rather than
  keep the Linux-convenient colony-warehouse approximation, even though
  it costs a larger diff. Shipped: `ai_contact_auto_trade` now keys on the
  ONE contacting Euro unit (`ai_contact_find_adjacent_euro`'s own find),
  moving TRADE_GOODS literally into/out of that unit's cargo hold --
  matches `LAB_002bbc`'s real `FUN_0000_902c` cargo-removal effect exactly.
  The colony-warehouse/nearby-ship radius search (no DOS counterpart) is
  gone. Also retired the invented mid-alarm "hard bargain" 2x-drain peel --
  no basis in `LAB_002bbc`'s real body (single deterministic AI decision,
  already established above). `LAB_002e92` reassessed and confirmed
  genuinely out of this port's TRADE_GOODS-only scope, not merely
  direction-flipped: it sells the tribe's OWN production goods (furs/ore/
  silver/tobacco/cotton/sugar via the `acStack_7c`/`98` candidate-slot
  arrays), a cargo-type universe TRADE_GOODS was never part of -- real RE
  gap (candidate-list construction unresolved), correctly still PARKED,
  not invented to plug it. The human CHOICE is now honestly documented as
  a Linux-only agency layer over `002bbc` (DOS's own human-with-cargo
  dispatch reaches neither label) rather than miscited as a `002e92` port.
  **Still not wired, on purpose**: the AI refuse gate -- `aiStack_d6[0]`
  (`FUN_1000_84fc`) is elsewhere equated to `ai_diplo_indian_relation`,
  but this project's established polarity for that accessor ("higher =
  friendlier": peace baseline 96, refuse-talk <40, trade bumps +2, war
  deltas negative) makes a literal `relation>49` refuse fire on FRIENDLY
  natives -- doesn't hold up without a live/independent scale check, not
  guessed at either way. Real next step if resumed: a live capture around
  `0x84fc`'s call in an active trade with known relation, or another doc
  independently pinning this specific call's scale. Rewrote
  `test_ai_contact.c`'s whole trade section (~15 assertions) to spawn
  cargo directly on the contacting unit; full `ctest` 41/41 green. Full
  trace: `indian_trade_2820.md`'s 2026-08-22 "user decision" addendum.

- [x] **T1.17 — Port `FUN_15eb_28c8`: colonist work-plot job scoring.** New
  2026-08-22, split out of `T1.14`'s `+4` identification. 254 lines,
  already in `FUNCTION_CATALOG.md` ("Score/assign best work-plot job for
  colonist," calls the already-ported `FUN_15eb_18ec`) but never linked to
  any Linux `.c` or `original_sources_annotated` `.md` file — check first
  whether an equivalent already exists under a different name (this
  project's colony-tile/job-assignment code) before assuming it's a clean
  gap. Includes the terrain-record `+4` term `T1.14` decoded (a labor/
  travel penalty subtracted from a candidate work-plot's score, worse for
  higher-effort terrain, plus a low-difficulty forest penalty when no
  settlement is adjacent) — that term's own formula is already known and
  ready to drop in once the enclosing function has a Linux home. Scope not
  otherwise assessed this pass (no read of the other ~250 lines yet).
  **2026-08-22 — scoped, not attempted. Genuinely large, same class as
  `T1.8`'s flood-fills, not a same-session port.** Checked first (per this
  row's own instruction): `ai_euro.c` has a large, mature, already-tested
  hand-tuned colonist-job-assignment heuristic (name-based profession
  matching — Pioneer/Farmer/Carpenter/Lumberjack/Free Colonist — plus
  per-shortage bind logic), not a port of this function and not obviously
  replaceable by one without real regression risk. Read `FUN_15eb_28c8`'s
  full body directly (`viceroy_unpacked.c:12908-13161`): scores up to 9
  jobs per candidate work-plot in a nested loop, AI-vs-human branch, and
  calls at least 8 still-unresolved helpers (`FUN_15eb_0a50`, `_06d2`,
  `_1068`, `_0e18`, `_1f72`, `_1376`, `_0470`, `FUN_13e4_003a`) on top of
  the already-known `FUN_15eb_18ec`. Porting this faithfully would need
  resolving that whole helper family first — a multi-pass RE effort in its
  own right, comparable in scope to `euro_g_table_0a60.md`'s own dig, not
  attempted this pass. Real next step if resumed: same method as
  everywhere else this session — resolve the helper identities one at a
  time via force-decompile, don't guess at the scoring shape from names
  alone.
  **2026-08-22, later session — 3 of 8 helpers identified, real progress,
  not closed.** Read each directly (flattened export, no headless needed
  this pass): **`FUN_15eb_0470`** = colony's workable-plot-tier count —
  `min(census(job_type=10), 2) + 2`, indexes a small fixed table at
  `+0x329` for how many tiles are in scope this pass (matches Colonization's
  colony-radius-grows-with-size convention). **`FUN_15eb_039e`** (its own
  callee, and called directly at two more sites in `28c8` itself with
  `job_type=0x28`/`3`) = census helper: walks a stride-`0xc` linked list at
  `-0x707a` counting nodes where `FUN_15eb_038e(job_type)` is true — a
  building/work-slot chain, not yet itself resolved (`FUN_15eb_038e`).
  **`FUN_15eb_0a50`** = colony population cap: `colony+0x95` (a level
  byte) `==0 -> 100`, else `(level+1)*100`; paired with a per-job-type
  count array at `colony+0x9a` (9 entries, one per job index 0-8) to
  compute open headroom in `28c8`'s own scoring loop (`local_6 -
  colony[0x9a+job*2]`, floored at 1). Still unresolved: `FUN_15eb_06d2`
  (called with a tile pair + packed byte — looks like a "select candidate
  tile" cursor/lock, called again with `0xffff` as a clear/release at loop
  end, not yet confirmed), `_1068` (job assign/query, `param_2` in `0..0xd`
  range including the `0xd`="none" sentinel), `_0e18` (returns current job
  for a colonist), `_1f72` (void, no params — a refresh/cache-prime call),
  `_1376` (returns a count/flag for `param_1` — called with `0xd` in
  `28c8`), `FUN_13e4_003a` (tile-pair -> terrain-class index into the
  already-resolved `DS:0x2f76` family — `local_32*0x10+0x2f7a` matches
  that table's own `+4` yield-column pattern from `T1.14`/`ds237d-terrain-
  record-full-layout`, likely just a coordinate->terrain-class lookup, high
  confidence but not independently confirmed this pass), `FUN_137f_02a0`
  (colony coordinates -> some scan-bound int, feeds `local_e`). Also
  spotted, not previously counted: `FUN_15eb_15c6` (a small per-job-index
  lookup, `local_34+0x2b6`) and `FUN_15dc_00e0` (`param_1`=another
  colonist's job index, `param_2`=own nation — NOT the already-known
  `FUN_15dc_00a2` quartile bucketer, a different function at a nearby
  offset; don't conflate the two). **Net**: real per-helper progress, but
  the count of genuinely unresolved pieces (`038e`'s own linked-list
  semantics included) is still ~9, unchanged in scale from last pass —
  not a same-session port. Real next step if resumed: resolve `_1068`/
  `_0e18` next (they gate the AI-vs-human branch and the "current job"
  baseline `iVar4`, both load-bearing for the loop's own control flow, not
  just scoring weight), then `_06d2`'s tile-lock semantics. `ctest` not
  run (doc-only, no `src/` touched).
  **2026-08-22, same session, continued — `_0e18`/`_0e52` resolved (2 more
  of the 8), confirms the existing Linux duality this row already flagged.**
  `FUN_15eb_0e18(param_1)` = colonist-slot's assigned **work-plot job**:
  `colony+param_1+0x20` (a per-colonist-slot array) when `param_1 <
  colony+0x1f` (colonist count, already known), else an out-of-colony
  fallback via `FUN_15eb_0924`/`_0902` (not resolved). `FUN_15eb_0e52` is
  the sibling **profession-type** accessor, same in-range array at
  `colony+param_1+0x40`, same out-of-range fallback reading straight
  through to `unit+0x315b` (already-named `profession` field, `col1_save.h`)
  — independent confirmation this DOS function models the exact same
  "work-plot job slot" vs. "colonist profession" duality `ai_euro.c`'s own
  existing hand-tuned heuristic already keys on (per this row's original
  Linux-equivalent check). Still unresolved: `FUN_15eb_0924`/`_0902` (the
  out-of-colony fallback pair both `_0e18`/`_0e52` share), `_06d2`, `_1f72`,
  `_1376`, `FUN_13e4_003a`, `FUN_137f_02a0`, `FUN_15eb_038e` (the census
  predicate `_039e` calls). Not a same-session port — pausing the helper
  dig here; real next step if resumed is `_0924`/`_0902` (shared by both
  just-resolved accessors, likely high-leverage) or `_06d2`'s tile-lock
  role. `ctest` not run (doc-only).
  **2026-08-22, same session, continued further -- `_06d2` resolved, and it
  surfaces a real, previously unflagged mechanic: working a tile for the
  first time can discover a hidden bonus resource.** `FUN_15eb_06d2(x, y,
  job_or_0xff)` writes `job_or_0xff` into the work-plot's occupant byte
  (`colony+plot_idx+0x70`, the same field `28c8`'s own loop reads); when
  writing a REAL job (not the `0xff` vacate sentinel) it also registers/
  reveals the tile with the map layer (`FUN_137f_0314`/`_03e4`/`_0228`),
  and -- only the FIRST time this specific tile is worked
  (`*(char*)(y+x*5-0x7262) != -1` gate, then set to `0xff` after) -- rolls a
  bonus-resource discovery (`FUN_281f_0d78`, a treasure/yield roll scaled
  against a difficulty-seeded threshold from `FUN_15eb_0544`, crediting
  `indian_state+5` -- or, on a miss, granting the colony a flat production
  bonus scaled by difficulty and continent stance via `FUN_281f_0d6c`).
  **Reads like Colonization's "found a hidden resource while working this
  tile" mechanic** (a real, named Colonization feature) -- not confirmed
  against `NAMES.TXT`/fandom docs this pass, flagging as a strong
  hypothesis, not stated as fact. In `28c8`'s own calling pattern, `06d2`
  is called once before scoring a candidate tile's 9 jobs (a real,
  provisional assignment, not a no-op probe) and once after with the
  `0xff` sentinel to undo it -- so scoring genuinely mutates colony state
  transiently, a real semantic subtlety a port must preserve. **Growing
  scope, not shrinking**: this dig pulled in 6 more unresolved callees of
  its own (`FUN_137f_0314`/`_03e4`/`_0228`/`_04b0`, `FUN_281f_0d78`/
  `_0d6c`, `FUN_15eb_0544`/`_0596`/`_0668`) on top of the ones already
  open.
  **2026-08-22, same session, correction -- the "growing scope" framing
  above was premature; a proper cross-reference sweep (the method note
  this very file's header repeats) resolves nearly all of it without any
  fresh disassembly.** Checking each new symbol against
  `original_sources_annotated/*.md` before assuming it needs RE (the
  discipline skipped in the rush above) found: **`FUN_15eb_1f72` is not a
  gap at all -- it's the already-documented, partially-ported colony
  crosses/bells composer**
  ([`nation_crosses_bells_1f72.md`](../original_sources_annotated/turn/nation_crosses_bells_1f72.md),
  same address, `viceroy_unpacked.c:12474`) -- `28c8` calls it as a bare
  side-effecting refresh before scoring. **`FUN_13e4_003a` is
  `terrain_yields.md`'s own already-decoded `+0x4` labor/travel-penalty
  term** -- that doc's 2026-08-21/22 entry already walked the raw `.asm`
  XREF back to `FUN_15eb_28c8` as the real owner independently of this
  investigation, table fully decoded (`DS:0x2f76+4`, 29 values). **`_0544`**
  = per-nation treasury accessor (`indian_trade_2820.md`). **`_0668`** =
  "mark tile purchased" (`MAP_LAYER2_PURCHASED`, nailed down in
  `euro_unit_act.md`'s own T1.8 XREF sweep, same address). **`_02a0`** =
  plain `continent_id(x,y)` (matches this project's already-named
  accessor, `T1.8`'s own citation -- don't confuse with the different
  4-arg `FUN_15eb_0142` that also uses `02a0` as a sub-step,
  `move_scoring_land.md`). **`_0314`/`_03e4`/`_0228`** are the same
  generic tile-record find/create family `terrain_yields.md`'s river-bit
  dig already profiled. **`_1376(job)`** is trivial once `_0e18` is known
  (census loop, no new accessor). **`_0924`/`_0902`** (the out-of-colony
  fallback pair `_0e18`/`_0e52` share) walk the nation's own unit list via
  the same iterator `28c8` already uses at its own top, filtered by the
  already-named `DS:0x30e` profession-slot gate from `T1.10`. **Net: every
  symbol in `28c8`'s call graph now has a real identity** -- RE-wise this
  is close to done, not a fresh multi-session dig; the "growing subsystem"
  read above came from not checking existing docs before assuming new RE
  was needed, exactly the mistake this file's own header method notes warn
  against. Genuinely still open and worth treating as a deliberately
  separate/PARKED slice rather than blocking the core port: `06d2`'s
  first-work hidden-resource **discovery roll itself** (`FUN_281f_0d78`'s
  exact odds/payout, `FUN_15eb_0596`'s apply, `FUN_281f_0d6c`'s miss-case
  bonus) -- self-contained, portable later without touching the scoring
  loop.
  **2026-08-22, same session, final -- one real architecture question
  found, genuinely blocks a full-fidelity port, not RE-gated.** Cross-
  checked the DOS formula's inputs against `colony.h`'s own
  `ColonizeColony` struct before writing any port code (per this file's
  own "check for a Linux equivalent first" discipline): `has_building[]`
  already covers the building-test terms (`FUN_15eb_038e`) directly, no
  raw-bitmask porting needed there at all -- good news. But DOS's own
  workable-plot count is **not fixed at 8**: `FUN_15eb_0470` returns a
  colony-size-scaled tier (`2..4`) indexing a small table at `DS:0x329`
  for the real tile count that tier unlocks (classic Colonization
  radius-grows-with-population), while `colony.h`'s own
  `COLONIZE_COLONY_FIELD_TILES` is a hardcoded **8** (the immediate ring
  only) -- no outer-ring field-tile storage exists in the Linux struct at
  all. Tried to pin the `0x329` table's real values (byte-dump against
  both Ghidra projects) to see whether tier 2 (the common/default case)
  actually equals 8 -- inconclusive this pass, tool/addressing mismatch,
  not a dead end just not finished. **This is the one piece that
  actually blocks a full-fidelity port** (not more helper RE): a real
  8-tile-only vs. full-radius scope decision, touching `ColonizeColony`'s
  own layout. Recommend as next step: confirm the `0x329` table's values
  (via existing `dosbox-x-dumps/*`, same method as `DS:0x2f76`) to learn
  whether tier-2/default colonies are 8-tile-equivalent (safe to port as
  base-ring-only, matching current data model) or genuinely need the
  outer ring (a struct-layout change, `colony.h`/save-format-adjacent,
  worth a user check-in before touching).
  **2026-08-22, same session, resolved cleanly — user chose to find the
  table; found, and it's a full match, not a compromise.** Byte-searched
  `dosbox-x-dumps/find_memory` (the same save already calibrated for
  `DS:0x2f76`, `HDR=0x88`, `DS=0x237d` — re-verified against the known
  cost-column bytes before trusting the new read, matched exactly).
  `DS:0x329..0x338` = `{0,4,8,12,20,2,0,1,0,3,0,1,0,0,2,2}`; `FUN_15eb_0470`
  indexes this array directly by its own tier value (2/3/4), giving
  **table[2]=8, table[3]=12, table[4]=20**. Tier 2 fires when
  `FUN_15eb_039e(10)==0` — a census of building index **10 in `@BUILDING`
  (NAMES.TXT row 176: "Town Hall", the 2nd of 3 Town Hall tiers)** — i.e.
  tier 2 is the **default, Town-Hall-level-1 case**. **Tier-2's own tile
  count is exactly 8** — the identical number `colony.h`'s
  `COLONIZE_COLONY_FIELD_TILES` already hardcodes. So the 8-tile Linux
  model isn't an approximation of DOS's common case, it **is** DOS's
  common case, byte-for-byte; only colonies that have built Town Hall
  level 2 (12 tiles) or level 3 (20 tiles) — a real but comparatively rare
  mid/late-game upgrade — would need storage this project's struct doesn't
  have yet. **Net: the core 9-job/8-tile scoring loop is now fully
  portable within the existing data model, no architecture change needed
  for the common case.** Town-Hall-level-2/3 outer-ring support stays a
  clean, separately-scoped future item (needs `colony.h` layout work,
  genuinely deferred, not blocking). `ctest` not run (doc-only; the
  byte-search touched no `src/`).
  **2026-08-22, session close-out -- consolidated into a proper deep-map
  doc, matching this project's own convention** (every other resolved
  accessor family in this file lives in a `.md` under
  `original_sources_annotated/`, not scattered across plan-row updates):
  [`colonist_work_plot_28c8.md`](../original_sources_annotated/turn/colonist_work_plot_28c8.md).
  Full accessor table, structure walkthrough, the fidelity finding, and
  the parked discovery-roll side effect all live there now. **RE is
  complete; no C port written this pass** -- a real port needs a golden
  fixture to verify the 9-job weighted formula against (none currently
  exercises colonist auto-assignment) and is separate, risk-bearing work
  matching this project's "document even if untestable, don't ship
  unverifiable code" precedent (`T1.9`). Real next step if resumed: either
  build a small golden fixture for colonist auto-assignment, or write the
  port as a reference-only structural function (matching `T2.1`/`T2.2`'s
  own "port exists, not wired" pattern) and accept it stays unverified
  until a fixture exists. `ctest` 41/41 green (doc-only this whole item).
  **2026-08-22, resumed — reference-only structural port written, the
  close-out's own recommended path.** `ai_euro_28c8_colonist_job_score_structural`
  (`ai_euro.c`, next to the other colonist job-assign helpers) scores all
  8 field jobs across the 8 tier-2 tiles using the real resolved terms
  (field yield via `colony_yield_for_tile`, a new
  `map_dos_terr_labor_penalty_byte` accessor for the `+0x4` term — added
  to `map.c`/`map.h` this pass, the table was already decoded in
  `terrain_yields.md` but not yet in code — `warehouse_level`-as-
  population-cap headroom clamp, current-job sticky doubling) and leaves
  the still-open weight terms (distance term, continent×nation danger
  term, RNG/wealth-rank boost, per-job throttle table, senior-tier/
  unhappy-colony gate, discovery roll) unimplemented rather than guessed,
  per its own header comment. **Not wired** — address-taken only in
  `ai_euro_colony_goals`, same convention as
  `ai_euro_5d04_nation_planning_structural`; still no golden fixture to
  verify the weighted formula against. Doesn't cover building-job
  assignment (DOS job `>=0xd`) or the human single-job-probe/early-
  shortcut gate — AI full-search field-job loop only. Full `ctest`
  41/41 green.

Bigger in scope than Tier 1 items but not blocked on anything static tooling
can't reach. Pick up after Tier 1 thins out, or interleave if a Tier 1 item
is genuinely stuck mid-session.

- [x] **T2.1 — Verify `ai_euro_5d04_nation_planning_structural` against
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
  after **T1.10**. **2026-08-26: T1.10 is now closed** (its own last
  entry: "T2.1's 'empty by construction' delta-catalog verdict is
  unaffected") — the blocking condition this row named is satisfied and
  the verdict didn't change, so this row's own Tier 2 task (verify +
  catalog deltas) is complete; checking it off. Whether to actually wire
  `5d04` live stays a separate Tier 3 user-confirm decision (`T3.1`),
  unaffected by closing this row.

- [x] **T2.2 — Verify `ai_diplo_153e_worthiness_score_structural`
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
  **2026-08-26:** this row's own Tier 2 task (verify + catalog deltas) is
  complete per its own text above — checking it off. `T3.2` (the actual
  wire-live decision) stays a separate, unaffected Tier 3 user-confirm
  item.

---

- [x] **T1.18 — `FUN_521d_20e6` land arms, structural port (2026-08-27).**
  Closed same session. Prior passes kept re-deriving the `FUN_1000_8aac`
  wall and closing sub-items as "no code to ship"; the bulk of the land
  path never depended on it. Ported from the clean raw C
  (`move_scoring_20e6_full.md`): prologue locals, `iStack_6a` explorer flag
  (all clauses), LAB_277a SCOUT/PATROL, LAB_2912→2a59 explore-ring scoring
  (colony pull, village penalty bands, explorer bonus, site reservation),
  LAB_4d2e→5183 8-direction wander scorer, epilogue commit. `DS:0x523d`
  resolved as the NAMES @UNIT bit-string (MSB-first; Brave `0x38`
  cross-check). Test `unit_ai_euro_20e6`; full `ctest` green.
  **Left thin, each marked in code:** LAB_52aa attack-odds core (raw is
  register-garbage → `combat_strength` ratio, war-only), explore-plane low
  nibble (seen bit), `−0x6168` rival strength (0), `0x4c` village arms,
  colonist labor loop (LAB_2c..), ship band. None of these need a live
  session; the odds core would need a clean re-disassembly of LAB_52aa
  (~raw line 2100) if byte-fidelity is ever wanted.

- [x] **T2.3 — Consolidate Indian relation onto the DOS alarm store.** New
  2026-08-27 from T4.9's finding, done same day. `ai_diplo_indian_relation*`
  were backed by `nation.relation_by_indian` (a `0x60` flag byte in every DOS
  save, never a scalar) with inverted polarity; DOS reads/writes
  `indian.alarm_by_player`. Added `ai_diplo_indian_alarm[_delta]` (DOS
  semantics, clamp 0..100, tension-tier clamp on negative alarm delta) and
  made the relation API a `100−alarm` view; switched DOS-transcribed sites
  (152e, 1816 — direction was inverted, 2820/417e prices, MADAT/0x4b gates)
  to the native calls; retired the invented drift/feeler/tick-±1 and two
  double-pushes; first contact clamps alarm ≤20 (`:96624`). ~230 test
  assertions rewritten (`test_ai_diplo.c`, `test_ai_contact.c`). `ctest`
  44/44. See `docs/indians.md` "single alarm store".
  **Second pass same day — at-war rule made DOS-shaped.** `ai_diplo_indian_at_war`
  = met ∧ (`euro_diplo & 0x02` ∨ alarm > 0x4a) per `153e`; new
  `COL1_INDIAN_WAR_BIT`; `13b0` resolved via the tag table as the paid
  "smite" war declaration (`@SMITEINDIANS`/`@SMITEEUROPE`/`@UNFORTUNATE`/
  `@MERCENARY`) — the only DOS setter of that bit; `4cc6_00f2` clears it
  below 75 (mirrored). Sticky bands re-scaled (at-war <26, very-low <16),
  ~30 more test fixtures re-banded. `ctest` 46/46, `golden_ai_turns` still
  the same 3 Brave diffs. **New RE lead (not chased): Linux's Euro×Euro
  bit map (`AI_DIPLO_WAR 0x01 / PEACE 0x02 / MET 0x40`) looks wrong** —
  `13b0` clears `0x40` then sets `0x02` on the (payer, target) Euro pair
  before `@MERCENARY` "declare war", and `43f7_1a26` (independence) sets
  `0x22` toward the REF nation and clears `0x40`; i.e. DOS `0x02` = WAR,
  `0x40` = PEACE, `0x20` = MET on `euro_relation`, same encoding as
  `euro_diplo`. Filed as **T1.19**.

- [x] **T1.19 — Re-verify the Euro×Euro `euro_relation` bit map.** Evidence
  above (`13b0`, `43f7_1a26`) says `0x02`=WAR, `0x40`=PEACE, `0x20`=MET;
  Linux `ai_diplo.h` says WAR `0x01`, PEACE `0x02`, MET `0x40`. Check every
  literal-mask call into the OR-set/AND-clear wrappers (T1.11's list:
  `2/4/0x10/0x20/0x22/0x40/0x60/0xb/0xbb`) against real saves'
  `euro_relation` bytes (a nation at declared war vs. at peace), then fix
  the defines + the `88d6` flood term's MET test. Save-format-visible, so
  worth doing before more diplo work.
  **2026-08-27 — done, same session.** Writers pin it: MET `0x20` (`5bfb_022e`
  first contact, `5bfb_3180`, `43f7_0108` `0x60` toward human+REF), PEACE
  `0x40` (`5bfb_0182`, `13b0` peace branch `@…188d`, `3844_0442` post-
  independence; cleared at every attack site and by `465b`), WAR `0x02`
  (`465b_0000` when attacking a human at peace, `5fef_1b0e`/`684c_08c0`/
  `6cb2_24b8` sneak-attack sites, `13b0` paid smite; cleared by `43f7_0108`
  `0xb`/`3844` `0xbb`), `0x01` planner war-intent (`6d8e`: 1-in-4 while bit
  `0x08` up and cooldown 0; `465b` clears after the attack), `0x04` on
  Indian pairs = "attack this village?" confirmed once (`465b` CHOICE
  `0x13ad`; `4cc6_00f2` clears on cooling), `0x10` crown-arms. Real saves:
  `00/20/22/60/a0/e0/e2/e8`, directional. `ai_diplo.h` defines swapped to
  DOS values (+ `AI_DIPLO_WAR_INTENT 0x01`), `88d6` gate re-pointed to
  PEACE; no raw masks anywhere in `src/`/tests so nothing else moved.
  `ctest` 46/46, goldens (which diff `euro_relation`) unchanged.
  **Attribution fix (later same day):** the paid `@SMITEINDIANS`/
  `@SMITEEUROPE` branch is in `FUN_5bfb_153e` (its two-line header fooled
  the function-finder), inside the human-initiated FA negotiation
  (`FUN_291f_016a` dialog pick → target; `@NOCONTACT` if unmet,
  `@ALREADYSMITE` if already at war; price `clamp(gold/50 ×
  (field_combat[target]+land_strength[target]) / 50, 10, 200) × 50`,
  halved for Franklin; pay → gold human→AI, AI clears PEACE / sets WAR
  with target, `@MERCENARY`; `@UNFORTUNATE` if the AI can't afford). FA
  UI is PARKED policy, so not ported. `FUN_5bfb_13b0` itself is the
  AI-initiated **treaty sign/cancel** — see **T1.20**.

- [x] **T1.20 — Replace the invented "alliance offer" with `FUN_5bfb_13b0`
  treaty sign/cancel.** `13b0(a,b)` (85 lines, `viceroy_unpacked.c:97260`):
  skip if WoI; cadence `(a+turn+b)%3==0` unless the pair is unmet; skip if
  WAR bit either way. `eligible = FUN_5bfb_10ec(a,b) || 10ec(b,a)`. If
  not eligible and no PEACE: `@SIGNTREATY`, set PEACE both ways,
  `FUN_5bfb_12d0` (clear armed-unit gotos near colonies) both ways,
  diplomatic cooldown (`nation+0x40+peer`) = 1 both ways. Else if PEACE or
  unmet: `@DECLAREWAR` (`0x18a5`) if no PEACE, `@CANCELTREATY` otherwise;
  cooldowns = 0; clear PEACE (no WAR bit — war starts when someone
  attacks). `10ec(a,b)` "war-worthy": turn > 39, focus nation ≠ `0xa153`,
  either nation's `-0x6bf4` byte > 7, neither independent, and for each
  of a/b: not (met-unpeaced with the focus nation) or (focus nation's
  `land_combat_strength` ≤ theirs and `-0x6bf0` ≤); then count met-
  unpeaced rivals (+1 if a,b themselves aren't at peace), scan continents
  1..14 with the G-table tallies (`-0x6b1a`/`-0x6b5a`/`-0x6a8e`, mapped in
  `euro_g_table_0a60.md`): return 0 if a's presence is dwarfed on any
  continent; accumulate `local_8` (a's `-0x6a8e`) and `local_6`
  (b's `(−0x6b5a + −0x6a8e)/2`); worthy if
  `count − (−0x6a9a)[a*3] + 4 <= 4·local_8/local_6` or no shared
  continent. Linux today: 1-in-40 "offers an alliance" CHOICE +
  `AI_DIPLO_ALLY` machinery (form/break/timers/prize) — all invented. The
  port replaces the offer with treaty sign/cancel (SIGNTREATY is a notice,
  not a CHOICE) and needs `10ec` on the already-ported continent tallies;
  the ALLY machinery can then be retired in a second pass.
  **2026-08-27 — first pass done.** `ai_euro_10ec_war_worthy` (`ai_euro.c`,
  on the 20e6 continent accessors + a land-unit-per-continent helper;
  `−0x6a9a` reads 0 — it's `unknown34_pad`, another "confirmed dead" table
  DOS does read; `0xa153` gate skipped) and `ai_diplo_13b0_treaty_tick`
  (`ai_diplo.c`), spliced over the 1-in-40 alliance offer in
  `ai_diplo_euro_balance`. Caller found: `13b0` is `153e`'s own entry
  branch for AI selves (`:97406`: `if (self > 3 || 0x543f[self]) 13b0(self,
  peer)`), so it runs wherever `153e` does — per peer per balance. Linux
  choices: unmet pairs get the bit effects (DOS signs PEACE on unmet pairs;
  real saves carry `0xa0`) but no notice; the `unit_units` target links
  `ai_diplo.c` without `ai_euro.c`, so `10ec` has a weak never-worthy
  fallback there. ALLY machinery (form/break/timers/prize/embargo lift)
  left in place for now — nothing in `euro_balance` creates ALLY any more,
  direct API callers/tests still work; retiring it is the second pass.
  `ctest` 46/46, `golden_ai_turns` unchanged.

- [x] **T1.21 — `FUN_465b_0000` `@WHACKINDIANS` attack-village/Brave confirm
  (2026-08-27).** Human land combat unit moving onto a native unit's tile,
  tribe alarm toward the human `< 0x4b`, pair `euro_diplo` bit `0x04`
  clear → "Shall we attack the {tribe}, Your Excellency?" Yes/No before
  the move; Yes sets bit `0x04` (`COL1_INDIAN_ATTACK_CONFIRMED_BIT`, asked
  once) and resumes the move, No aborts. `FUN_4cc6_00f2` clears bit `0x04`
  on any cooling delta (and `0x02` below 75) — mirrored in
  `ai_diplo_indian_alarm_delta`. `ai_contact_try_whack_confirm` +
  `AI_POPUP_TAG_CONTACT_WHACK`, hooked in `game_try_unit_move` ahead of
  `units_try_move`; village tiles keep the separate `4528` Attack/Leave
  warn. Test in `test_ai_contact.c`. `ctest` 46/46.

- [ ] **T2.4 — Retire the Linux-only Euro alliance machinery (cleanup).**
  Dead since T1.20: no AI path sets `AI_DIPLO_ALLY` any more (DOS has no
  Euro×Euro alliances; `13b0` is treaty sign/cancel). Still present:
  `ai_diplo_form/break_alliance[_ctx]`, ally treaty timers, ally aid/prize,
  the `DIPLO_ALLIANCE` CHOICE apply, ~64 refs in `ai_diplo.c` and ~130
  assertions in `test_ai_diplo.c`, plus `ai_king_ai_peacetime_gift`'s
  "self or ally" beneficiary test (now effectively "self"). Zero gameplay
  effect either way — do it as one deliberate deletion pass when the diplo
  file is next touched, not piecemeal.

- [x] **T1.22 — `FUN_4d56_2820` `LAB_002e92`: the tribe sells its own goods
  (2026-08-27).** Candidates = the 16 cargo slots sorted by the `2154` bid
  table (`FUN_1000_a0c0` = `FUN_1cf8_000a` insertion sort on `DS:0x9e78`),
  top three excluding muskets/food/tools/trade goods. AI picks the highest
  `DS:0x84BC` throttle byte (`k_2820_throttle`, from the 2026-08-22 capture);
  human gets `@BUYWHICH` → `@BUY0` Accept/"Never mind" (Haggle arm still
  PARKED), `@NOTENOUGH` when broke. Price byte-exact per the doc formula.
  Accept: gold−, `tribe.last_sold`, `indian.tons[cargo] −= qty`, unit hold
  `+= qty`, alarm `+= price/25+1`. **Flagged:** qty is `DS:0x8dc4`, a
  shared scratch never assigned inside `2820` (captured 50 mid-negotiation);
  ported as 100 with the literal `>>2` for ships. `ai_contact_auto_buy_2e92`
  (AI, from `ai_contact_auto_trade`'s empty-hold branch),
  `ai_contact_enqueue_buywhich`/`apply_buywhich`/`apply_buy0` (human, tags
  43/44). Test in `test_ai_contact.c`. `ctest` 46/46.
  **Same day — Haggle arm added** (`iStack_5e == 2`): `ai_contact_2e92_haggle`
  — roll `RNG(0, bid/25+8)`; `price < 11` or roll `<= difficulty+1` →
  refuse (alarm +2, `sticky_trade_good = 0xfe`, `@BADHAGGLE2`); else
  `price -= price>>2` (floor 10), 1-in-(8−difficulty) alarm +1, re-ask with
  `@BUY1` (tag built at runtime as `"BUY"+digit`, like DOS's
  `FUN_0000_d9b4`+`'0'+iStack_88`).
  **Sell-side hard-bargain (`LAB_002bbc` human loop, the old "`306c`")
  also done, same day:** `@TRADE0` is now Accept / "A fairer price would be
  N" / gift; `ai_contact_2820_sell_price_ex` keeps DOS's loop scaffolding
  (`c4 = RNG(0,1) + ((ask − tier + 4) >> 2)`, `fair = (ask+1)·4 + price`);
  haggle = `ai_contact_2820_sell_haggle` (`RNG(1, c4<<3) > difficulty` →
  `c4−−`, `price += RNG(ask/2+1, ask·2+1)·qty/100`, `fair = max(fair,
  price+10)`, re-ask with `@TRADE1`; else `sticky_trade_good = cargo`,
  alarm `+tier/2+1`, `@BADHAGGLE0`); gift (round 0) hands the goods over
  (`last_bought`, friction/attacks relief, alarm `−4·(c4+1)`); accept now
  applies DOS's `−2·c4` alarm instead of a flat −2. Nothing of `2820`
  remains PARKED except the `0x8dc4` quantity source.

- [x] **T1.23 — `@VIOLATE` trigger (`FUN_4720_049e`) — closed as a dead
  tag (2026-08-27).** No `VIOLAT` string exists in `VICEROY.EXE`'s DS, so
  DOS never shows `@VIOLATE`. `4720_049e` pushes `@HAVETREATY`/`@SNEAK`/
  `@CANCELPEACE`/`@DECLAREWAR` — the encounter → war-declare flow already
  covered by `DIPLO_WAR`. Doc-only.

- [x] **T1.24 — `20e6` `LAB_52aa` attack-odds core, byte-exact.** Shape
  recovered from the asm 2026-08-27 (`move_scoring_20e6_full.md` tail):
  `odds = ((4fa8(foe,0)+1) / max(4fa8(foe,2),1)) × 5fef_1b0e(unit,x,y,0,0)
  / max(@UNIT col+6, 1)` then the known modifiers. Blockers: `5fef_1b0e`'s
  probe-mode return value and `4fa8` cases 0/2/0xb on a unit record (the
  "case 2 = transport-chain splice" reading conflicts with its use as a
  divisor here — re-verify). Static work, medium size.
  **2026-08-27 — done, same session.** Both blockers fell statically:
  `8aac` is `FUN_281f_08bc → FUN_1427_0d38` (the stack query dispatcher),
  *not* `4fa8` — the "case 2 = chain splice" verdict belonged to the wrong
  function. Its jump table (`CS:0xd78`) decoded from the raw bytes:
  case 0 = Σ `@UNIT` col9 (`0x5239`) over the stack, case 2 = count of
  military land types `{1,4,6,7,8,9}`, case 0xb = Σ `157e_004a(unit, 1)`
  over stack units whose ship-ness matches the tile, case 3 = Pioneers,
  case 0xa = vehicles (`0x5236 > 1`), case 0xd = Σ holds (`0x5237`).
  `5fef_1b0e` probe mode (`param_4=param_5=0`) returns
  `(local_92 << 3) / (local_a8 + 1)` = attacker strength (with the `0x8d04`
  stash + peels) over defender engagement strength — exactly
  `combat_land_engage`/`combat_naval_engage`. `@UNIT` record layout
  corrected from the shipped rows: `0x5230` icon(w), `0x5232` move, `0x5233`
  attack, `0x5234` defense, **`0x5235` holds** (not `0x5237`), `0x5236`
  c6 (0 natives / 1 land / 99 vehicles), `0x5237..0x523a` c7–c10, `0x523d`
  bits. Col9 is 0 for every land type ⇒ on land `odds = (1/max(mil,1))·base`,
  i.e. the AI never attacks a stack with ≥2 military units. Wired in
  `ai_euro_20e6_attack_term` (`ai_euro_20e6_unit_col9`); still substituted:
  the `REF nation == 2` halving and the case-0xb adjacent-Spanish skip.
  `ctest` 46/46, `golden_ai_turns` unchanged.

## Tier 3 — Confirm with the user before flipping

The engineering/verification for these can happen autonomously (do that
part under Tier 2 above); the actual switch-flip is a deliberate,
user-visible behavior or policy change — confirm before doing it, per
CLAUDE.md's "hard to reverse" guidance.

- [x] **T3.1 — Wire `5d04` structural port live** (replace/extend
  `ai_euro_nation_planning`), once T2.1's delta catalog exists. This
  changes default AI economic/hire behavior.
  **2026-08-27 — wired.** `ai_euro_nation_planning` now calls
  `ai_euro_5d04_nation_planning_structural` (treasury bump + flag cascade +
  ship-buy ladder + hire-ladder tail) instead of the treasury bump alone.
  Zero observable delta as T2.1 predicted: full `ctest` incl. the
  re-enabled `golden_ai_mid01`/`golden_ai_late01` green, `golden_ai_turns`
  unchanged (same 3 parked Brave diffs). The thin hire matrix below it is
  still the live behavior until the tail's two iterator stubs go real.

  **2026-08-27, later — hire-ladder tail callees made real (`ai_euro_5d04_cb_*`).** Europe stack iteration = the nation's units at Europe coords; type/profession reads and equip writes by name (fixtures use synthetic pools), expert gate, `03d0` urgency, `38fd_0718` recruit spawn, market ask/bid from the EuropeScreen (col1 `euro_price` fallback), hold-0 unload/sell (boycott-aware, tax-adjusted), cargo buy+load, Artillery spawn, ship departure = board the dock stack (the dispatcher's own 48d3_048e teleport follows), `0x5238` = new `ColonizeUnitType.space`, `0xa0db/0xa0da` = per-turn own-colony muskets/tools-need counts (6d8e prelude). Misports fixed: RNG rolls that were really market prices, `found_flags` vs pioneer count. Two loop guards (DOS drops departed ships from its list; Linux does not). Still thin: `0a2e`/`0c14` market volume only for the human market; recruit-pool refill is a plain RNG pick (`38fd_46d4` remap table unported); `DS:0x945a` = 0. `ctest` 46/46, mid01/late01 green.
- [ ] **T3.2 — Wire `153e` worthiness-score port live**, once T2.2's delta
  catalog exists. Changes default war-declare eligibility scoring.
  **2026-08-27 — deliberately NOT flipped.** Every term is a neutral stub
  except diplo bit `0x10`, and no Linux code ever sets that bit (its DOS
  writer `FUN_38fd_5930`, the crown-arms-a-rival event, is unported) — a
  live wire would be a trigger that can never fire. Prerequisites before
  this is worth flipping: port `38fd_5930` (sets the bit) and un-stub at
  least the G-table/relation terms; then re-run the T2.2 catalog.
  **2026-08-27, later — `38fd_5930` ported (`ai_king_new_war_event`,
  `@KINGNEWWAR`), and it settles the question the other way:** the event
  is gated `0x543f[nation]==0` (human only) and runs from the `38fd`
  Europe-EOT king slot when the tax event stays quiet, so bit `0x10` only
  ever lands on (human, peer) pairs — while `153e` early-exits for
  `self == human`. The bit is therefore never a live term for an AI self:
  wiring `153e` today would be all-neutral-stubs by construction. Stays
  unflipped; real prerequisite is un-stubbing the score terms themselves.
  Port detail: Franklin (FF 19) suppresses; gate `(difficulty+2)*turn > 799`;
  every met non-REF, non-independent peer must be at PEACE (raw byte —
  DOS reads `-0x77c4` directly, unmet = 0) and Σ`land_combat_strength`
  (`-0x6be4`) of met-unpeaced peers ≤ own; roll `RNG(0,(4−peace_n)·20) ≤
  difficulty`; random peace peer; `gold=(diff+1)·100`, `count=1`, +
  `field_combat_totals` gap (`-0x6bd4`): `count=(gap>>3)+1`, `gold+=gap·25`;
  caps `6−diff` / `(5−diff)·500`; Veteran Soldiers (type 1, profession
  `0x15`) spawned in Europe → Linux pushes them on the docks; clear PEACE
  both ways, set `AI_DIPLO_CROWN_ARMED 0x10`; DOS also stamps
  `DS:0x53c8[peer]=turn` (`head.nation_relation`). Test in `test_ai_king.c`;
  `ctest` 46/46.
  **Follow-ups done same session:** (a) `ai_diplo_read` no longer
  synthesizes `PEACE|MET` for never-written pairs — it returns the raw
  byte like DOS (`-0x77c4`, unmet = 0); one fixture that relied on the
  default (alliance near-parity) now marks the pair met+peace explicitly.
  (b) `head.nation_relation` is now DOS's Crown-war stamp: the WAR/ALLY
  mirror is gone (no readers existed), `ai_diplo_declare_war` zeroes both
  slots like the attack sites, `ai_king_new_war_event` writes the turn.

  **2026-08-27, latest — every term real; exported as `ai_diplo_153e_worthiness_score`.** `-0x6d68` = `colony_counts`, `-0x6a4e`/`-0x6ada` computed per continent (exposed combat / colonist-class count), `0x53c8` = `head.nation_relation` (16-turn cooldown forces the talk + clears CROWN_ARMED; stamp refreshed), `0xa153` = top of the `FUN_5bfb_00f8` rank table (`ai_diplo_00f8_top_ranked_nation`), `FUN_5bfb_0000` = per-colony border probe (stack opcodes 0xa/0xb), Franklin, self-treasury clamp, `uStack_ae` bit 0x02 = WAR (not PEACE). Not a live trigger by construction: DOS only reaches phase 1 for a human self via `FUN_5bfb_3180` (encounter dialog, phases 2–4 unported). Bonus: `10ec` gained its asm `[0xa153]==human` gate, and `13b0` now requires a real adjacent-unit/colony encounter (was every balance) — that removed the 12 `euro_relation 0x40` diffs from `golden_ai_turns`.
  **2026-08-27, latest — phases 2–4 ported: `ai_diplo_153e_encounter` (ai_diplo.c) + `AI_POPUP_TAG_DIPLO_TALK`.** Trigger = the human's land-unit move next to an AI Euro unit (`game_loop.c`, the 3180 Euro×Euro branch, WoI clear, once per pair per turn). Flow as a popup state machine: HELLO{FIRST|AHOY|MEEK|MANLY} → APOSTATES/HEATHEN third-party demand → PIRACY (privateers to Europe) → SIEGES (withdraw adjacent military) → TRIBUTE → PROVOKE/war or the WORTHY partition treaty → GIVECASH → (OLD)PEACE{MEEK|MANLY} 4-way menu: withdraw (NOTHINGWITHDRAW/WITHDRAW/NOTWITHDRAW/MAYBEWITHDRAW + threat roll), tribute demand (THREATS/PROVOKE/GIFTS), alliance pick (NOCONTACT/ALREADYSMITE/SMITEEUROPE/SMITEINDIANS/UNFORTUNATE/MERCENARY); exit latch 0x08 + treaty cooldown. Tags are the DOS strcpy/strcat products resolved from VICEROY.EXE (121248+id). Not modeled: WANTSTUFF goods demand, USA variants, encounter-direction stamps. Smoke test in `test_ai_diplo.c`.
- [ ] **T3.3 — Re-enable `golden_ai_turns`/`golden_ai_joint`.** **2026-08-27
  partial:** `golden_ai_mid01` and `golden_ai_late01` pass again and are
  re-enabled as gates (`CMakeLists.txt`). `golden_ai_turns` still fails on
  exactly 3 TURN1→2 Braves (parked seed-100 quiet-pulse divergence,
  `seed100_brave.md`), so it and `golden_ai_joint` (depends on it) stay
  disabled. Explicitly
  parked 2026-08-19 (`ai_transcription.md` top status note, `roadmap.md`
  phase 3) until the AI planner reaches T3 1:1 for in-scope planners —
  re-enabling early just chases "porting incomplete" symptoms. Don't flip
  this back on piecemeal; do it once Tier 1/2 above has meaningfully closed
  the gap, and confirm with the user first since it changes what `ctest`
  gates on by default.

---

## Tier 4 — Needs the user's live DOSBox-X session

Confirmed genuinely blocked without a live capture (register/stack trace,
memory dump, or hang-dump). **As of 2026-08-22, 6 of the 8 items ever filed
here closed without any live session at all** — ad-hoc
byte-pattern search against the existing `dosbox-x-dumps/*` saves found
every one of them already sitting in static data. Before working (or
asking the user to work) any `[ ]` item below, search the existing dumps
first — see the method note above. As of 2026-08-27 every item here except `T4.6` (parked by
policy, not by blocker) is closed — `T4.9` too, statically. Don't resume
speculatively, but don't assume "Tier 4" still means "needs the user" the
way it did when this file was first written.

  **2026-08-27, latest:** `golden_ai_turns` is down to exactly the 3 parked TURN1→2 Brave diffs (the 12 relation diffs were the 13b0 cadence bug, fixed). Still DISABLED — flip is the user's call.
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

- [x] **T4.5 — Incite (`417e`) Mode-2 trigger/caller.** **Caller found
  2026-08-27, static-only** (`indian_incite_417e.md` "Caller: FOUND"):
  `FUN_4d56_4528` tail switch `case 7`, reached through OVL13's local
  thunk `4c36` → resident `FUN_1000_a5b8` (`CALLF ensure-loaded; JMPF
  0000:417e`) — the stub earlier written off as a false lead. Mode 2 is a
  real mechanic: AI Missionary at a village, gate = tribe relation to
  human <75 ∧ human MET ∧ AI wealth rank < human's ∧ AI gold ≥1500 ∧
  (RNG(0,4)≠0 ∨ no mission) → incite the village against the human,
  else establish mission / denounce heresy. **Port of the AI auto-incite
  itself still open** (new item, low-medium value): hook at the AI
  Missionary CONTACT-goal arrival in `ai_euro.c`, reuse
  `ai_contact_incite_price(..., is_missionary=1)` with `target=human`,
  needs a chrome-free `ai_contact_apply_incite` variant and
  `turn_rank_euro_nations` for the rank compare.
  **2026-08-27 — ported.** `ai_contact_ai_incite_human` (`ai_contact.c`),
  hooked at the top of the AI Missionary branch of
  `ai_contact_missionary_convert` (the `4528` non-human `case 3 → 7`
  slot): alarm(tribe→human) `< 0x4b`, human MET the tribe,
  `euro_power_rank[ai] < euro_power_rank[human]` (literal DOS compare on
  the `5bfb_00f8` table — whether that means "poorer" is not
  independently confirmed), AI gold ≥ 1500, `RNG(0,4) != 0` ∨ no mission;
  then `417e` Mode 2: AI×human MET gate, pay `ai_contact_incite_price`,
  alarm(tribe→human) `+10` (same effect the human Mode-1 apply uses).
  Chrome-free (DOS shows no dialog for Mode 2). Test in
  `test_ai_contact.c`. `ctest` 46/46, `golden_ai_turns` unchanged.

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

- [x] **T4.9 — `2820`'s AI refuse-gate scale for `FUN_1000_84fc`
  (`aiStack_d6[0] > 0x31` → refuse-with-penalty).** New 2026-08-22, split
  out of **T1.16** now that its own full rewrite landed. `aiStack_d6[0]` is
  `FUN_1000_84fc`'s return, elsewhere equated to `ai_diplo_indian_relation`
  — but this project's own established polarity for that accessor is
  "higher = friendlier" (peace baseline 96, refuse-talk < 40, trade-accept
  bumps +2, war deltas negative). A literal `relation > 0x31`-scaled port
  of this gate would then refuse trade with FRIENDLY natives, which
  contradicts every other established use of the same accessor family —
  either this one call's `dialog`/context argument selects a genuinely
  different (inverted or differently-scaled) native quantity, or this
  project's own polarity convention is wrong for this specific site.
  Static tooling can't distinguish the two — needs a live DOSBox-X read of
  `aiStack_d6[0]`'s actual value at `2820`'s `FUN_1000_84fc` call, cross-
  checked against the same save's known in-game relation, while an AI
  peer contacts a tribe. Not wired on a guess; `ai_contact_auto_trade`
  currently always succeeds once the pre-existing outer
  `alarm_by_player>=50` gate (untouched, unrelated) is clear. Full trace:
  `indian_trade_2820.md`'s 2026-08-22 "user decision" addendum.
  **2026-08-27 — resolved, static-only, no live session needed.** The
  "contradiction" was a storage mis-mapping, not a polarity mystery.
  `FUN_1000_84fc` → `FUN_15dc_00e0` = `word DS:[0x5b1c + (a*0x27+b)*2]`;
  stride `0x27` words = 78 bytes = `sizeof(ColonizeCol1Indian)`, and
  `FUN_15dc_0006` (`viceroy_unpacked.c:9229`) sets
  `*0x8d4e = idx*0x4e + 0x5ad6`, so `0x5b1c = 0x5ad6 + 70` =
  **`indian[idx].alarm_by_player[euro]`** — *not* `nation.relation_by_indian`
  (DOS never touches `-0x77c0`; that byte array is Linux-only). Polarity
  from the writers: map-gen (`:107764`) seeds it `RNG(0,14)` (+2×difficulty
  for AI nations), first contact (`:96624`) clamps it ≤20, and
  `FUN_4cc6_00f2` (the "relation_delta") clamps to 0..100 and on a
  *negative* delta calls `281f_0a10` (= clear-bit `15b3_00d0`) to drop
  bit 4 and, below 75, the war bit 2. So **high = hostile**, and the "peace
  baseline 96 / refuse-talk <40" convention belongs to Linux's separate
  `relation_by_indian` field, not this accessor. `aiStack_d6[0] > 0x31`
  therefore = `alarm_by_player[e] > 49` — **already wired**: the
  `ai_contact_auto_trade` outer gate `alarm_by_player[e] >= 50` that the
  earlier note called "unrelated" is exactly this DOS gate. Side finding
  (new Tier 2 candidate, not done): Linux `ai_diplo_indian_relation`/
  `_delta` (~40 call sites) store on `nation.relation_by_indian` with
  inverted polarity where DOS stores on `alarm_by_player`; the two Linux
  fields drift independently. Also DOS's post-trade delta is
  `alarm += -2*c4` on accept / `-4*(c4+1)` on refuse with
  `c4 = RNG(0,1) + ((bid - 2*tier + 4) >> 2)`, where Linux does a flat
  `alarm--` — refinement, not a blocker.

---

## Tier 5 — Polish / chrome (last)

Deliberately last per `roadmap.md` phase 5 and `project_goals.md`'s
visual-polish-last rule. Some of these need the user's judgement call on
visual fidelity, not just RE.

- [ ] **T5.1 — VGA-identical dialog chrome** (meet/diplo/king wood frames,
  FA `3f41` full widget body, chief portrait `FUN_281f_04ac`).
- [ ] **T5.2 — Letter cinematic** (`43f7` `160a`).
- [ ] **T5.3 — Congress UI / F3 portrait grid.**
- ~~**Not planned:** `COLDIG.BIN` digital SFX — no reachable DOS trigger
  found~~ **Retracted 2026-08-27.** The triggers exist; they are event ids
  passed in `AX`, which Ghidra's decompile drops. Playback is wired and the
  remaining push sites are tracked as `port_plan.md` P3.7 — not an AI-queue
  item either way.

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
