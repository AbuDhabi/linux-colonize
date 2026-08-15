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
   Carpenter/Preacher/Statesman worker's output. **Where this combines with
   the crosses/bells computed below is not found in this function** — no use
   of the `-0x7238` array appears after line 11305 in this body. Either the
   combine happens in the (unread) rest of the caller, or in `FUN_15eb_09c0`
   (`1f72`'s sibling call from `394c`), or the totals array is read by
   something else entirely later in the EOT sequence. Open question.
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
4. **Lines 11315-11344: bells — read, NOT fully resolved, NOT ported.**
   ```
   bells = 1                                          ; base — presumably Town Hall passive
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
   The **Jefferson/Paine/Printing-Press/Newspaper pieces line up exactly**
   with what the port already implements (`colony_prod_colony_bells_ff`) —
   good independent confirmation those are right. But two things are
   **not** ported and not fully understood, so nothing here was changed:
   - This applies Jefferson's ×1.5 (and Printing Press/Newspaper) to the
     **whole passive bells total** (base + Paine + this mystery term),
     *before* combining with the Town Hall statesman workers' totals (per
     item 2, wherever that combine happens) — the port instead applies
     Jefferson only to each statesman worker's own contribution inside
     `colony_prod_colony_bells_ff`'s per-worker loop, never to the base +1.
     If DOS really does apply it to the base too, the port under-counts by
     a small amount whenever Jefferson is owned. Not fixed — the combine
     point (item 2) needs resolving first to know how big a change this is.
   - The `0x12` flag term (`(pop+3)/5`, gated on the AI/non-human-shaped
     condition) doesn't match Bolivar's known effect (SoL +20%, a display-
     time bonus in `founding_fathers_bolivar_sol_bonus`, not a bells
     production term) even though `0x12 == 18 == FF_SIMON_BOLIVAR` lines up
     numerically. Possibly a DOS AI-difficulty bells subsidy unrelated to
     any Founding Father, using the same per-nation flag-test primitive for
     an unrelated bit — not resolved, not ported, `byte 0xa892`'s meaning is
     also unknown.

## Open questions (next layer, not a blocker)

- The early 5×5/`0xa891`/`0xa895`/`0xa896` block (item 1) — purpose unknown.
- Where the `1d4c` per-worker totals (item 2) combine with this function's
  crosses/bells (items 3-4) — not found in this function's body.
- Whether Jefferson/Printing-Press/Newspaper apply to the Town-Hall-passive
  bell too, not just per-statesman-worker contributions (would need the
  combine point resolved first).
- What flag `0x12` and `byte 0xa892` actually are, given they don't match
  Bolivar's documented effect.
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
