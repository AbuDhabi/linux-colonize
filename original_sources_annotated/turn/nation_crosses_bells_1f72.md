# `FUN_15eb_1f72` — colony crosses/bells composer (partial peel)

Deep annotation per `original_sources_annotated/README.md` Layer D. Found
2026-08-15 while verifying `FUN_15eb_038e` building-index arguments for the
`FUN_15eb_1d4c` deep peel ([`manufacturing_worker_calc_1d4c.md`](manufacturing_worker_calc_1d4c.md))
— `FUN_15eb_1f72` is the clean function immediately following `1d4c`'s
`RETF` (same one confirmed clean/undamaged in that doc's corruption
writeup). Call chain: `FUN_15eb_3956` (thunk `281f_0c22`, "Compose yields"
per [`colony_eot_production.md`](colony_eot_production.md)) → `FUN_15eb_394c`
→ `FUN_15eb_09c0` + `FUN_15eb_1f72`. Source: `viceroy_unpacked_2.c:11170-11387`.

**This is a partial peel** — one self-contained sub-block (crosses) is fully
traced and already used to fix the port; the rest (bells, and an unrelated
early block) is read but not fully resolved. Do not treat unconfirmed parts
below as ported guidance.

## Structure

1. **Lines 11198-11298: unrelated early block(s).** Computes `byte 0xa891`
   (some per-nation/difficulty classification, `local_26` from
   `FUN_13e4_003a`) and a 5×5 nested loop (`local_1c`/`local_1a` 0-4) calling
   `FUN_15eb_18ec` (the field-yield composer — see
   [`terrain_yields.md`](../../docs/terrain_yields.md)) with `param_4=1`
   ("clear pedia" mode) into `byte 0xa895`/`0xa896`. **Not resolved this
   pass** — plausibly a happiness/famine-risk or AI-scoring metric sharing
   this function for compose-pipeline convenience, not obviously related to
   crosses/bells. The 5×5 range (vs. the colony's normal 3×3/8-surround
   catchment) is itself unexplained.
2. **Lines 11299-11305: the `FUN_15eb_1d4c` per-worker loop** (see the other
   doc) — fills a 20-slot totals array (`DS:-0x7238`, indexed by
   `param_2` from each `1d4c` call, i.e. profession 9-17) with each craft/
   Carpenter/Preacher/Statesman worker's output.

   **Open question CLOSED (2026-09-24, bugs.md #937).** `DS:-0x7238` is just
   the signed spelling of `DS:0x8dc8` (`-0x7238 & 0xffff == 0x8dc8`) — the
   same 20-slot totals array, zeroed slot-by-slot immediately before the
   loops (`viceroy_unpacked.c:12580-12584`, `for (i=0;i<0x14;i++)
   *(int*)(i*2 - 0x7238) = 0;`) and then written through both the absolute
   (`0x8dc8`, `0x8dea`, `0x8dec`) and the indexed (`slot*2 - 0x7238`)
   spellings in the same body. Slot `0x12` is `0x8dc8 + 0x24 == 0x8dec` —
   the **bells** word; slot `0x11` is `0x8dea` (crosses). So the combine
   does **not** happen elsewhere: it happens inside `1f72` itself, right
   after the `1d4c` per-worker loop, because the crosses/bells sequence
   below is `+=` onto the very slots that loop just filled.
3. **Lines 11306-11314: crosses — fully traced, confirmed, and now used to
   fix the port** (`colony_prod_church_passive_crosses` — see
   `building_production.md`'s 2026-08-15 fix row):
   ```
   crosses = 1                                    ; unconditional
   if FUN_15eb_038e(0x25 /* Church, NAMES.TXT row 37 */):    crosses += 1
   if FUN_15eb_038e(0x26 /* Cathedral, NAMES.TXT row 38 */): crosses += 1
   ```
   Church and Cathedral are worth the **same** +1 (both checks are
   independent `if`s, not `else if` — a colony can in practice only have
   one, so this is moot in normal play). This directly contradicts the
   port's previous +2 (Church) / +3 (Cathedral), which was manual/wiki-
   sourced, not decomp-derived — likely mixing up the *per-worker* Preacher
   rates (3 Church / 6 Cathedral, which **are** correctly 2× apart, and stay
   as-is) with the colony-wide passive.
4. **Bells — full literal sequence** (`viceroy_unpacked.c:12603-12648`;
   `viceroy_unpacked_2.c` raw 11299-11343 in the older numbering). Every
   term below is a `+=` onto `*(int*)0x8dec`, i.e. slot `0x12` of the totals
   array the `1d4c` worker loop has just summed into — the workers' bells
   are already in the word when line 1 runs:
   ```
   ; (the 1d4c worker loop above has already added every Statesman's output
   ;  into slot 0x12 == 0x8dec)
   bells += 1                                          ; base — presumably Town Hall passive
   if FUN_15eb_3960(nation, 0xf /* = 15 = FF_THOMAS_JEFFERSON, exact index match */):
     bells += bells >> 1                              ; ×1.5 — matches Jefferson's +50%
   if FUN_15eb_3960(nation, 0x11 /* = 17 = FF_THOMAS_PAINE, exact index match */):
     bells += byte[nation*0x13c - 0x77f7] * bells / 100   ; matches Paine's "+tax_rate%"
   if FUN_15eb_3960(nation, 0x12 /* = 18 = FF_SIMON_BOLIVAR? */)
      AND (nation >= 4 OR table[nation*0x34 + 0x543f] != 0):   ; the same "AI/non-human"-
                                                                ; shaped gate seen in
                                                                ; FUN_15eb_18ec's SoL zero-out
                                                                ; (terrain_yields.md)
     bells += (pop + 3) / 5
   bells += byte[0xa892]                              ; unexplained
   if FUN_15eb_038e(0x14 /* Newspaper, row 20 */): bells <<= 1        ; ×2, matches +100%
   else if FUN_15eb_038e(0x13 /* Printing Press, row 19 */): bells += bells >> 1  ; ×1.5, matches +50%
   ```
   Line-by-line: workers sum (the `1d4c` loop); `+= 1` unconditional;
   Jefferson `+= v>>1`; Paine `+= (tax * v & 0xffff) / 100`; the `0x12`
   flag term `+= (pop + 3) / 5`; `byte 0xa892` zeroed on the line before
   and then added; Newspaper `<<= 1` else Printing Press `+= v>>1`.
   The **Jefferson/Paine/Printing-Press/Newspaper pieces line up exactly**
   with what the port already implements (`colony_prod_colony_bells_ff`) —
   good independent confirmation those are right. But two things are
   **not** ported and not fully understood, so nothing here was changed:
   - **2026-08-25: fixed, empirically.** This applies Jefferson's ×1.5 (and
     Paine, and Printing Press/Newspaper) to the **whole passive bells
     total** (base + Paine + this mystery term), *before* combining with
     the Town Hall statesman workers' totals (per item 2, wherever that
     combine happens) — the port used to apply Jefferson only to each
     statesman worker's own contribution, never to the base +1, and ran
     Paine *after* Press/Newspaper instead of before. Re-checking
     `manufacturing_worker_calc_1d4c.md`'s own Statesman-body trace
     (`15eb:1f18`) confirms Jefferson can't belong in the per-worker body
     at all — it's exactly `v = base; if (skill) v <<= 1`, nothing else.
     Generalized this function's literal order from "just the passive" to
     "passive + workers combined, same relative order" (Jefferson → Paine
     → Press/Newspaper) and checked against 7 player-reported colonies
     (`dutch-reports.SAV`, Jefferson+Paine(35% tax) both owned, mixed
     Newspaper/Press/none, 1-3 workers each): **exact match on all 7**.
     `colony_prod_colony_bells_ff` (building_production.md 2026-08-25 row)
     now applies Jefferson once colony-wide instead of per-worker, and
     moved Paine before Press/Newspaper. `byte[0xa892]` is still not
     identified — none of the 7 fits needed it, but a configuration this
     set didn't cover could still expose it; the combine point (item 2)
     also remains formally unresolved even though this specific
     "passive+workers combined, same order" model now fits everything
     tested.
   - **The `0x12` flag term — confirmed real and ported, 2026-08-15.**
     Player-observed on Viceroy difficulty: an AI colony's free-colonist
     Statesman nets 5 colony bells vs. 3 for a human colony in the same
     setup (excluding the passive Town Hall +1), with no visible FF/media
     bonus on either side. This rules out "dead code"/decompiler noise and
     confirms the term is real and worth porting — the arithmetic itself
     (`(pop+3)/5`) was already asm-certain, only its realness and purpose
     were in doubt. The `0x12 == 18 == FF_SIMON_BOLIVAR` numeric match is
     still almost certainly coincidental reuse of the shared flag-test
     primitive for an unrelated AI-difficulty bit — Bolivar's real effect is
     SoL +20% (`founding_fathers_bolivar_sol_bonus`), not a bells production
     term, and nothing here reads Bolivar ownership; it reads the same
     AI/non-human table gate `colony_prod_sol_bonus_field` already uses.
     Ported: `colony_prod_colony_bells_ff` gained a `bool nation_is_ai`
     parameter, adding `(pop+3)/5` to the Town Hall passive bells right
     after the base +1 (matching the decomp's structural position, before
     Paine/Press/Newspaper) — the exact pop value wasn't re-derived from the
     single observation (that would be underdetermined from one data point);
     the formula was taken directly from the decompiled bytes, only gated on
     AI-ness. `byte 0xa892`'s meaning is still unknown, not ported.

## Open questions (next layer, not a blocker)

- The early 5×5/`0xa891`/`0xa895`/`0xa896` block (item 1) — purpose unknown.
- Where the `1d4c` per-worker totals (item 2) combine with this function's
  crosses/bells (items 3-4) — not found in this function's body.
- **Whether Jefferson/Printing-Press/Newspaper apply to the Town-Hall-passive
  bell — closed 2026-08-15, structurally provably moot, not just untested.**
  First pass (human colony): Jefferson alone 1 bell; +Printing Press 1 bell;
  +Newspaper 2 bells — matched both hypotheses (port's per-worker-only
  Jefferson, and DOS's literal base-first order) identically, since
  `bells=1; bells+=bells>>1` truncates to `1` either way (`1>>1=0`).
  Proposed as a next step: re-test on an **AI-controlled** colony, reasoning
  the AI bells subsidy (item 4, `+(pop+3)/5`) could push the passive base
  above 1 before Jefferson's truncation could hide anything. **That
  reasoning was wrong** — re-reading this function's own literal decomp
  order (`bells=1` → Jefferson → Paine → AI-subsidy → `byte 0xa892` →
  media), Jefferson is the *second* step, evaluated immediately after
  `bells=1`, strictly *before* Paine, the AI subsidy, or the mystery byte
  can add anything. So `bells` is provably still exactly `1` at the moment
  Jefferson's check runs, for *every* colony configuration this function can
  ever compose — human or AI, any population, any tax rate, Bolivar-gate or
  not. `1 >> 1 = 0` is not a coincidence of small test cases; it's forced by
  the order itself. Player re-test on an AI colony, empty Town Hall,
  Jefferson: still **1 bell**, exactly as this proof predicts. Conclusion:
  whichever way DOS's bytes literally read here, Jefferson's *observable*
  effect on the passive Town-Hall bell is always zero — porting this
  specific piece would be a no-op by construction, not merely unconfirmed.
  Closed; no code change, and no further test of this specific question is
  needed (its answer can't vary). This does *not* touch whether
  Jefferson/Press/Newspaper apply to the *per-worker* Statesman
  contributions — that part is already correctly wired
  (`colony_prod_bells_worker`, `colony_prod_colony_bells_ff`) and unrelated
  to this passive-only question.
- `byte 0xa892` — closed 2026-09-24 (bugs.md #938a/#925b), provably +0. All
  known writers across the five decompiled exports and their raw `.asm`
  (`viceroy_unpacked[.c/.asm]`, `viceroy_unpacked_2[.c/.asm]`,
  `viceroy_overlays[.c/.asm]`) are `mov byte [0xa892],0x0` at the three sites
  found by grep (`viceroy_unpacked.c:4294/5369/12619` and mirrors) — every
  one zeroes the byte, then the very next few statements read it back
  (`viceroy_unpacked.c:4313/5388/12638`, `+= (uint)byte[0xa892]`) with no
  intervening writer, including no call into `FUN_15eb_3960` (checked: that
  function only reads a per-nation FF bitmask, `viceroy_unpacked.c
  raw 13832-13844`, and never touches `0xa892`). No `mov [0xa892], reg`
  (a store of a computed value) exists anywhere in any `.asm` export — only
  the `mov byte [0xa892],0x0` immediate stores and `mov al,[0xa892]` reads.
  So the term is a dead zero at every one of the 6 read sites listed in the
  bug row (raw 12634-12641's `1f72` composer plus the four callers at raw
  48038/48064/50303/65529/65679); not ported, and no further chase needed.
- `FUN_15eb_09c0` (`1f72`'s sibling in the `394c` compose call) — read;
  ruled out as the combine point. It's an unrelated per-nation colony-count
  tally (`byte 0x8d72`/`0x8d74`/`0x8d76`, capped at 50), nothing to do with
  crosses/bells.
- `FUN_4345_0a22` (`colony_eot_production.md`'s cited "Bells + FF" callee,
  `291f_09f8 → 4345_0a22`) — read; also ruled out. It's pure FF-election
  bookkeeping (accumulate an already-computed bells *amount* passed in as
  `param_2`, check the election threshold, call `FUN_4345_0342` to elect).
  It does not compute or combine anything — its caller must already have the
  final per-colony bells number by the time it calls this. That caller is
  presumably inside `FUN_364b_0688` itself (the main EOT dispatcher,
  ~800 lines, see `colony_eot_production.md` Phase A) — not chased into;
  diminishing returns for what's now a small-magnitude question (whether
  Jefferson/press/paper apply to the +1 Town-Hall-passive bell specifically,
  worst case ≈0.5 bell/turn/colony).
