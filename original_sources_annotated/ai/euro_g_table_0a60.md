# Deep `−0x6790` G-table — ported (`FUN_521d_0a60`, 2026-08-14)

Section map for the **actual G-table (nation×continent stance) write loop**
inside `FUN_521d_0a60` (Euro colony/unit goal writer — already confirmed
clean, 853 lines, fifth-pass sweep; no disassembly-fault warning). Decomp
`viceroy_unpacked.c:88054-88151`. This is the deep body `ai_transcription.md`
unpark #4 has listed as **OPEN** since the earliest passes ("full −0x6790 G
table / explore-ring matrix still PARKED"; "thin prio ladder own≥2/3/4→6/7/8
Done" was always an approximation, not this).

**Status: Done (real formula ported), same day.** All 8 data-table
dependencies traced and confirmed (see "What's newly resolved" below), then
wired into `ai_euro_refresh_continent_stance` in `ai_euro.c` — the formula
itself (presence-vs-threshold baseline, expand/military pressure vs each
rival/tribe by defense-value comparison, zero-presence override) is now
real, not heuristic. Two approximations kept, both documented in-code:
the DOS diplomacy-flag gate on which rivals count toward pressure (two
still-unidentified bits) is skipped — every rival/tribe with presence is
always counted, a defensible superset since DOS's gate only ever narrows
that set; and the existing Linux-only at-war/Indian-hostility-sticky
overrides are kept layered on top (protect tested behavior with no direct
DOS-table backing). Full `ctest` 42/43 (same pre-existing unrelated
`unit_ai_euro_expand` baseline failure, confirmed via `git stash` — no
regression), including all T2 golden gates (`golden_ai_turns`/`_mid01`/
`_late01`/`_joint`) unchanged.

Parent: [`move_scoring.md`](move_scoring.md). Linux:
`ai_euro_refresh_continent_stance` / `s_euro_continent_stance` in `ai_euro.c`.

## Real DOS formula (transcribed, not yet ported)

```c
for (continent = 0; continent < 0x10; continent++) {
  int expand_pressure = 0;   /* local_22 */
  int military_pressure = 0; /* local_38 */
  int dev_accum = 0;         /* local_156 — accumulated but not read again in
                                 this excerpt; likely feeds a caller-side use
                                 not traced here */
  int presence_sum = 0;      /* local_1e2 */

  /* Pass 1: compare against the other 3 Euro nations. */
  for (rival = 0; rival < 4; rival++) {
    dev_accum    += table_ADA[rival][continent];        /* DS −0x6ada, new, unnamed */
    presence_sum += table_B1A[rival][continent];         /* DS −0x6b1a, summed over ALL 4 (self included) */
    if (rival == own_nation) continue;
    if (table_B1A[rival][continent] == 0 && table_B5A[rival][continent] == 0)
      continue;                                          /* rival has no presence here at all */
    /* diplomacy gate: skip unless (not-ally-ish) or a specific flag combo */
    byte d = ai_diplo_flags(own_nation, rival);           /* FUN_281f_0a38 */
    if ((d & 0x60) != 0x20) {
      byte d2 = ai_diplo_flags(own_nation, rival);
      if ((d2 & 0x48) == 0x40) continue;
    }
    /* who's more developed here? */
    if (table_E74[continent][rival] < table_E74[continent][own_nation]
        || table_B1A[own_nation][continent] == 0)
      expand_pressure++;      /* rival weaker here, or I have zero presence → room to expand */
    else
      military_pressure++;    /* rival stronger here → defend/contest */
  }

  /* Pass 2: same shape against the 8 Indian nations (id 4..11). */
  for (indian = 4; indian < 0xc; indian++) {
    bind_indian(indian - 4);                               /* FUN_281f_0a42 */
    dev_accum += indian_local_accum[indian][continent] * 2; /* local array,
                                 zeroed at function entry — turn-scratch, not
                                 a DS global */
    if (table_E34[continent][indian] == 0 && indian_local_accum == 0) continue;
    int relation = ai_diplo_indian_relation(indian, own_nation); /* FUN_281f_030c */
    if (relation < 0x4b) {                                 /* hostile-ish */
      byte d3 = ai_diplo_flags(own_nation, indian);
      if ((d3 & 2) == 0) continue;
    }
    if (table_E34[continent][indian] < table_E74[continent][own_nation]
        || table_B1A[own_nation][continent] == 0)
      expand_pressure++;
    else
      military_pressure++;
  }

  /* Baseline tier: room to grow vs. continent already full. */
  int scaled = (table_B1A[own_nation][continent] + presence_sum) * 20;
  int cap = continent_tally_b[continent];        /* DS −0x7a38 == 0x85c8,
                                                       ALREADY-NAMED, ALREADY
                                                       LIVE in Linux */
  G_table[own_nation][continent] = (scaled <= cap) ? 6 /* develop */ : 0 /* none */;
  if (expand_pressure)   G_table[own_nation][continent] = 4; /* pressure to expand → "military"-numbered tier, see note */
  if (military_pressure) G_table[own_nation][continent] = 3; /* rival stronger → "expand"-numbered tier, see note */
  if (table_B5A[own_nation][continent] == 0 && table_B1A[own_nation][continent] == 0)
    G_table[own_nation][continent] = 4; /* own nation has zero presence at all → force tier 4 */

  /* Separately (same continent loop, after the G write): −0x6168 max-tracks
     the largest +0x1f field (colony "level") among all FOREIGN colonies on
     this continent, then folds in a capped sum of −0x6b5a across the other
     3 nations. See move_scoring_land.md's "0x8db8 identified" section for
     where 20e6 reads −0x6168 back out (`local_12` explore-bonus term). */
}
```

**Naming caveat on tier values 3/4, checked at port time:** the numeric
G-table values `{0,3,4,6}` are the same set Linux's
`ai_euro_refresh_continent_stance` already produced, but this
transcription's `expand_pressure→4` / `military_pressure→3` assignment is
copied straight from the decompile's own literal writes — it does **not**
match the old thin heuristic's `3 expand`/`4 military` comment convention
(which was never itself DOS-derived, so has no authority to override this).
Checked both hardcoded `stance==3`/`stance==4` consumer sites in `ai_euro.c`
at port time: `stance==3` (soft-cap military priority + bump FOUND) reads
as a defensible "contested continent, don't pick a losing fight, grab a
founding spot instead" strategy under the new mapping, not obviously wrong;
the peacetime `stance==4` sticky-gate is unaffected either way since it's
forced by its own explicit override, independent of the pressure tier. Kept
the DOS-literal values rather than swapping to match the old convention —
full confidence would need tracing DOS's own `20e6`/`5d04` consumers of
this exact table, not attempted.

## What's newly resolved vs. what's still open

**Resolved this pass:**
- `−0x7a38` **= `continent_tally_b[16]`** (`0x10000 − 0x7a38 = 0x85c8`,
  `save_format_map.md` row 305, already `mapped`, already live at
  `col1->post_map.continent_tally_b[]`). The G-table's baseline develop/none
  split is gated by the *same* field Linux's current thin stance already
  uses (`target = continent_tally_b[cid] / 12`) — real cross-validation that
  the thin version's instinct (compare against this field) was right, just
  scaled/shaped differently from the real formula.
- `−0x6b1a` **confirmed, not just characterized** (second pass same day):
  its own writer, `FUN_4962_0018` (the "reset + recompute per-nation AI
  stats" function, called once per nation, also the already-cited row-251
  writer for `unit_type_counts`), increments `[continent + nation*0x10]`
  once for every colony that nation owns on that continent — a plain colony
  count, not "friction." `save_format_map.md`'s `0x94e6` row updated to
  `colony_counts_by_continent`.
- `−0x6b5a` **confirmed** the same way: same `FUN_4962_0018` loop over
  units, increments `[continent + nation*0x10]` once per own **land** unit
  (type outside the `[0xd,0x12]` ship range) found on that continent — a
  land-unit-count table, not a bare presence flag. `save_format_map.md`'s
  `0x94a6` row updated to `land_unit_counts_by_continent`.
- `−0x6a0e` **confirmed**: same function, a per-continent (not per-nation)
  bitmask — bit `1` any Indian tribe present, `2` a foreign Euro unit
  present, `4` a foreign colony present, `8` this nation's own exposed
  combat-capable land force. `save_format_map.md`'s `0x95f2` row updated.
  Closes out `move_scoring_ship.md`'s `−0x6b1a`/`−0x6b5a`/`−0x6a0e` trio —
  all three now have confirmed writers.

**Resolved, second pass same day — the `FUN_4962_0006` register blocker
turned out tractable after all.** The decompiler couldn't show what
`FUN_4962_0006`'s implicit `AX`/`BX` args were at each call site, but the
raw `.asm` (`viceroy_unpacked.asm`, `FUN_4962_0018` body from offset
`0x44`) shows every `MOV AX,.../MOV BX,...` immediately before each `CALL`
in plain sight — no register-flow tooling needed, just reading the
instructions the decompiler had already thrown away. All four:

- `−0x6ada` **= skilled-unit count per nation×continent.** Incremented by 1
  whenever `FUN_281f_0b78(unit) ≥ 0` — the same "is this unit skilled" query
  that already feeds the known nation-total `census_pop_proxy` (`0x9410`,
  save row 243, "+1 skilled unit"); this is that signal's per-continent
  breakdown.
- `−0x6e74` **= sum of `FUN_281f_09c8(unit, mode=0)` per nation×continent.**
  `FUN_281f_09c8` thunks to `FUN_157e_004a`
  (`FUNCTION_CATALOG.md`: "unit base combat×8 + vet/Drake/damage", already
  partially ported per `move_scoring.md`/`euro_unit_act.md`'s vet/Drake +50%
  peels) — mode 0 is the base/unbonused value. Also feeds the nation-total
  `0x9180`. This is the G-table's Euro-side "development level" comparison
  operand.
- `−0x6a8e` **= sum of `FUN_281f_09c8(unit, mode=1)` per nation×continent**
  (combat-adjusted/bonused value) — also feeds the already-known
  `land_combat_strength[4]` nation-total (save row 247, confirming its
  "Σ combat mode 1" description was right). This is `FUN_521d_20e6`'s
  subtraction/"discount" term.
- `−0x6a4e` **= same combat-value sum, restricted to exposed units**
  (not fortified, orders `≠'A'(0x41)` `≠'G'(0x47)`, plus a difficulty/
  building-class gate at `0x543f`) — per-continent twin of the already-known
  `field_combat_totals[4]` nation-total (save row 250, same write site,
  confirms its existing "land not in colony / not A\|G" description).

All four are now `mapped`-quality in `save_format_map.md` (rows 254-259
fully rewritten). **The whole six-table block (`0x94a6`/`0x94e6`/`0x9526`/
`0x918c`/`0x9572`/`0x95b2`) is now a coherent, fully-named "per-nation-per-
continent AI stats" scoreboard**: colonies, land units, skilled units, raw
unit value, combat value, and exposed combat value — each summed by
`FUN_4962_0018` once per nation per turn. This — combined with the already-
resolved `continent_tally_a`/`continent_tally_b` thresholds — is
**everything the G-table formula's own body needs**, except reconciling
`FUN_281f_0a38`'s bitmasks (below) and the `−0x6e34`/Indian-side family.
Structurally ready for a real port attempt next time, not just documentation.

- `−0x6e34` **resolved, third pass same day** — traced exactly as predicted:
  `FUN_4962_06b6`'s own `.asm` body sums `FUN_281f_09c8(brave, mode=1)`
  (combat value) into `[continent + tribe_type*0x10]`, the Indian-side twin
  of `−0x6e74`. `save_format_map.md`'s `0x91cc` row rewritten — was a never-
  verified placeholder name (`tribe_dwellings_91cc`), now confirmed to be
  something else entirely (Brave combat-value sum, not building/dwelling
  data). **The G-table formula's entire data-table dependency list is now
  fully named** — `−0x6790` (the table itself), `−0x6b1a`/`−0x6b5a`/
  `−0x6ada`/`−0x6e74`/`−0x6a8e`/`−0x6a4e` (Euro six-table block),
  `−0x6e34` (Indian side), `−0x7a38`=`continent_tally_b` (threshold). Only
  `FUN_281f_0a38`'s bitmask reconciliation (below) remains before this
  formula is honestly portable.
- **Bonus, same trace**: `FUN_4962_06b6` also fully resolved
  `indian_incite_417e.md`'s *second* unnamed price term, `−0x6e7c`
  (sum of Brave combat values per tribe type — the per-nation-total sibling
  of `−0x6e34`'s per-continent breakdown). Combined with the earlier
  `−0x69d6` (village count) finding, both of `417e`'s previously-unnamed
  price terms were identified — and, in a later pass the same day, actually
  **wired into `ai_contact_incite_price`** (recomputes both live sums
  directly instead of the old `ind->tech` stand-in; also fixed a real
  tribe-type comparison bug found in the process). See
  `indian_incite_417e.md`'s "Price formula wired for real" section.
- `0x947e` (`village_counts_by_continent`, cross-tribe-type, save row 252)
  also fell out of the same trace — was `unknown_ds_947e`, now confirmed.
- `FUN_281f_0a38` — **already identified**, checked this pass:
  `euro_diplo.md` names it as the `FUN_15b3_0004` thunk, "Read peer byte,"
  which already branches Euro (`nation<4`, `euro_relation[]`, Linux
  `ai_diplo_read` — ported) vs Indian (`nation≥4`, `nation*0x4e+23000`
  matrix, flagged there as "full matrix **PORT DEBT** on Linux" — explains
  why this G-table formula's Indian-side pass reads real DOS bytes Linux
  doesn't fully model yet). The bitmasks used here (`0x60`/`0x48`/`0x20`/`0x2`)
  don't match `euro_relation[]`'s documented `WAR 0x01/PEACE 0x02/ALLY
  0x04/MET 0x40` convention on their face — **not reconciled this pass**,
  worth checking whether `0a38`'s return is a repacked/translated byte
  (matching `euro_diplo.md`'s own note that the raw peer byte and the
  Linux-side bit convention aren't a direct 1:1) before assuming either
  side is wrong.

**Ported, same day (fourth pass).** All data tables above resolved via raw
`.asm` register tracing; the `FUN_281f_0a38` bitmask gate was the one
remaining open item and was **deliberately approximated, not blocking** —
see the "Status: Done" note at the top of this file for what shipped and
what's still simplified.
