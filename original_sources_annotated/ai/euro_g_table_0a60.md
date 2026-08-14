# Deep `−0x6790` G-table — real formula found (`FUN_521d_0a60`, 2026-08-14)

Section map for the **actual G-table (nation×continent stance) write loop**
inside `FUN_521d_0a60` (Euro colony/unit goal writer — already confirmed
clean, 853 lines, fifth-pass sweep; no disassembly-fault warning). Decomp
`viceroy_unpacked.c:88054-88151`. This is the deep body `ai_transcription.md`
unpark #4 has listed as **OPEN** since the earliest passes ("full −0x6790 G
table / explore-ring matrix still PARKED"; "thin prio ladder own≥2/3/4→6/7/8
Done" was always an approximation, not this).

Parent: [`move_scoring.md`](move_scoring.md). Linux thin stand-in:
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

**Naming caveat on tier values 3/4:** the numeric G-table values `{0,3,4,6}`
are the same set Linux's `ai_euro_refresh_continent_stance` already produces,
but this transcription's `expand_pressure→4` / `military_pressure→3`
assignment is copied straight from the decompile's own literal writes — it
is **not** yet cross-checked against what `s_euro_continent_stance`'s
existing 3-vs-4 comment convention (`3 expand`/`4 military`) means for
consumers reading the table elsewhere in the DOS code. Don't assume the
labels transfer without checking a consumer site first.

## What's newly resolved vs. what's still open

**Resolved this pass:**
- `−0x7a38` **= `continent_tally_b[16]`** (`0x10000 − 0x7a38 = 0x85c8`,
  `save_format_map.md` row 305, already `mapped`, already live at
  `col1->post_map.continent_tally_b[]`). The G-table's baseline develop/none
  split is gated by the *same* field Linux's current thin stance already
  uses (`target = continent_tally_b[cid] / 12`) — real cross-validation that
  the thin version's instinct (compare against this field) was right, just
  scaled/shaped differently from the real formula.
- `−0x6b1a`'s role is now much better characterized than the earlier
  "friction"-shaped guess from `move_scoring_land.md`: here it's clearly a
  **presence/colony-count tally per nation×continent**, summed across all 4
  Euro nations and directly compared (×20) against `continent_tally_b`. This
  is a stronger, more specific hypothesis than "friction" — worth
  reconciling with the `FUN_521d_20e6` read site (`nation*0x10+continent`,
  same indexing) next time either is revisited.

**Still unnamed / not attempted this pass:**
- `−0x6ada` (dev_accum per nation×continent, new table, not seen in any
  prior session's notes).
- `−0x6b5a` (used only as a nonzero/zero presence flag here; already
  flagged in `move_scoring_ship.md`'s `−0x6b1a`/`−0x6b5a`/`−0x6a0e` trio).
- `−0x6e74` / `−0x6e34` (own-vs-rival "development level" comparison tables,
  Euro and Indian sides respectively — gate the expand-vs-military split).
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

**Not ported to Linux.** Six more tables/helpers need naming before this
formula is safely transcribable — same "structural confidence ≠ semantic
confidence" discipline as everywhere else in this project. `−0x7a38`'s
resolution is real, standalone progress (confirms and slightly sharpens the
thin stand-in's existing instinct) but doesn't by itself unblock a full port.
