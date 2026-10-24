# Report screens — F10 Score + Hall of Fame

Split from [reports.md](reports.md) on 2026-10-23 (doc-length limit); same
per-report layout as there. Code: `src/core/reports_score.c` (score computation,
Hall of Fame / Exploits). Shared chrome, fill bars and F2-F9 stay in
[reports.md](reports.md); golden workflow and pitfalls in
[report_screens.md](report_screens.md).

## F10 - Colonization Score

- DOS FUN: title dialog `FUN_41f2_000e` (71034) + line-advance
  `FUN_41f2_0048` (71050) — thunk `FUN_291f_0faa`. Score compute + report
  UI `FUN_41f2_0092` (71068, 346 lines) / mid-entry `FUN_41f2_0294` (72085,
  330 lines, Ghidra split — not the unrelated `152e`/`0038` callee, a
  since-corrected misresolve) — thunk `FUN_291f_03aa`. Gold rebate +
  treasure dialog `FUN_41f2_0b70` (72415) — thunk `FUN_291f_0f9c`.
  High-score table `FUN_41f2_0f56` (72552) — thunk `FUN_291f_0f8e`.
  End-game snapshot `FUN_41f2_14a8` (72727).
- Background: `WOODPANL.PIK` (full-screen wood — same file as the
  title-menu Hall of Fame, not a `REPORT<N>.PIK`).
- Data source (`reports_compute_score`, byte-faithful to `FUN_41f2_0092`
  since 2026-08-29): subtitle = difficulty rank + `player[human].name` +
  nation adjective + season/year. Components in DOS block order, each with
  its DOS gate (a gated-off component prints no line and adds 0):
  - Citizens: colony population always counts (sentinel profession falls
    back to Free Colonist); a map/Europe unit counts only when its raw
    `profession` byte is a genuine assigned job (0..27) — sentinel 28
    contributes zero. Points: 28→2, 25/26/27→1, else 4.
  - Congress: +5 per Founding Father (`reports_ff_owned_by_nation`).
  - Gold: `gold / 1000`, line + points only when gold ≥ 1000.
  - Villages Burned: `burned × (−1 − difficulty)`, only when burned ≠ 0
    ("<n> Villages Burned:  −k", `@MISC` #117).
  - Rebel Sentiment: DS:`0x53d0` `rebel_sentiment_report` (not nation+0x19 —
    both are 94 on the golden), only when ≠ 0.
  - Early Revolution (`@MISC` #142): `(1780 − declare_year) × 2`, gated on
    the independence ACHIEVED bit (`0x5382|0x08`) and `declare_year < 1780`.
    `declare_year` is the DS:`0x53a7`/`0x53a8` byte pair `FUN_43f7_1a26`
    latches at the Declaration (`year/100`, `year%100` — the king-audience
    RNG bytes, dead once the King is gone; `reports_score_declare_year`).
    DOS prints the *current* season/year in the parenthesis.
  - Liberty Bells: `min(100, liberty_bells_total / 100)`, gated on REF
    present (`0x5382|0x02`) and bells ≥ 100; `1a26` zeroes the human's
    `liberty_bells_total` at the Declaration so this is "bells since
    declaring". Label is a runtime pointer (DS:`0x97e4`) outside the `@MISC`
    table — text unresolved, "Liberty Bells" stand-in.
  - Independence Achieved (`@MISC` #116 #119, "(n prior nations)" #143):
    `100 >> prior_nations` percent, where prior_nations = other Euro powers
    with `nation_flags & 0x04`. Total = sum × `(8 + (8 >> prior)) / 8`
    (×2 / ×1.5 / ×1.25 / ×1.125 / ×1) — `reports_score_apply_recognition`.
  - SCORING COMPLETE: with `0x5382|0x10` (`calendar_latch`, set after the
    retire chain) the screen shows only `@MISC` #126 centered at y=0x61 and
    composes nothing.
  - `reports_score_rating` (`FUN_41f2_0b70`): Colonization Rating =
    `((mult × total) / 100) >> 1`, mult `{4,5,6,8,10}` by difficulty;
    exploits tier = largest n−1 (n 1..24) with `n²/3 < (mult × total)/100`,
    cap 23, −1 when none. Not shown on F10 — feeds the Retire exploits
    screen and the Hall of Fame sort key.
- Columns/layout: single-column hand-placed layout, not the shared
  row/step grid — subtitle (y=12), "<Nation> Citizens: +N" (y=24) +
  wrapping citizen-portrait icon strip (y=32, 8px pitch, wraps at 37
  icons/row with alternating half-icon row offset), "<Nation> Continental
  Congress: +N" (y=60) + 4-col x 3-row FF name grid (x=16/88/160/232,
  y=67 step=7), large blank gap, then the gated component lines in DOS
  order at a 7px pitch starting y=150 ("Gold: (N$) +N" / villages / "Rebel
  Sentiment: +N" / early revolution / bells / independence) ending in
  "Total Score: N" (golden: exactly Gold/Rebel/Total at 150/157/164); when
  more than 5 lines qualify the block shifts up so Total stays clear of the
  bar (DOS y for the extra lines unconfirmed — no golden with them), then a
  plain two-tone fill-rect progress bar (x=35..285, y=186,
  fill=min(total,1000)/1000).
- Ordering: fixed layout, no rows to order.
- Scroll/paging: none.
- Click targets: none — and no OK button at all (unlike every other
  report); dismisses on click anywhere, same as Congress page 2.
- Strings: title/subtitle/Total Score use ink index 149, everything else
  index 68 (WOODPANL.PIK-specific, not the usual report-plate 14/15/97).
  "Gold"/"Citizens"/"Continental Congress"/"Rebel Sentiment"/"Total Score"
  resolve live from `@MISC` #59/#115/#134/#69+#71/#121 (2026-08-28).
- Retire chain (`FUN_41f2_14a8`, `game_retire_after_score`; with
  `cheats_enabled` set F10 / REPORTS Score enter it directly, no confirm,
  `game_open_score_report`, port_bugs.md #1169): `@RETIRE`
  confirm → this report → **exploits screen** (`FUN_41f2_0b70`,
  `reports_render_exploits`: WOODPAN2.PIK, GAME.TXT `@EXPLOITS` with
  `%NUMBER0` = rating / `%STRING0` = nation name at y=5, the first tier+1
  `@SCORE` category fields stacked upward from y=195, `SCORE<tier+1>.SS`
  frame 0 — a painting of the tier-th item, SCORE17 = the university —
  blitted at x=100 over the list last, the last row's name field with
  `%STRING0` = leader's surname (DOS `strchr(name,' ')+1`) at y=142;
  skipped when tier < 0 or scoring complete; any key/click continues; every
  line is `FUN_281f_0100` = `FUN_104b_0318` centred over (0, 0x140), (0xa0, 0xa0)
  or (0x22, 0x8c), port_bugs.md #1217, static read) →
  Hall of Fame (record inserted first) → title menu. Sheet palette is
  remapped onto WOODPAN2's like ICONS.SS; the sheet-header y DOS uses for
  the picture is unread (placed under the header lines instead).
  At the exploits reveal, `OVL06:3e11-3e2d` plays `0x21` for tier 0–6,
  `0x25` for tier 7–22, or `0x24` for tier 23; `game_retire_after_score`
  uses the same `exploits_tier` through `sound_retire_tune_id`.
- Port status: Done (golden `score.png`) — `reports_render_score`/
  `reports_score_collect_citizen_jobs`/`reports_score_draw_citizen_icons`
  (`reports_score.c`). Golden's citizen breakdown (142
  colony-pop + 16 field-colonist points = 158) required the exact
  "profession byte 0-27 or nothing" rule above; flipped one pre-existing
  unit test's expected value (a Pioneer with sentinel profession byte 28
  no longer scores).

## Hall of Fame (title-menu screen, shares F10's WOODPANL.PIK)

- DOS FUN: `FUN_41f2_0f56` (72552, "high-score table load/insert/save +
  present UI") — same function that inserts an F10 Retire result. No
  separate DOS renderer confirmed distinct from this one; `HALLFAME.DAT`/
  `INDEPENDENT`/`NAMES` strings sit in the same string-table region per
  `viceroy_unpacked.asm`.
- Background: `WOODPANL.PIK`.
- Data source: port-local `ColonizeHofEntry[]` (`game_loop.c`), loaded/
  ranked from `HOF.TXT` (`game_hof_path`/`_load`/`_save`/`_insert`) standing
  in for DOS `HALLFAME.DAT` (6 × 42-byte records: name[24], nation,
  declared, achieved, year, season, difficulty, score, rating, tier —
  `FUN_41f2_14a8` builds one at Retire). Row format since 2026-08-29:
  `score|leader|nation|year|difficulty|rating|declared|achieved|nation_id`
  (older 5-field / bare-integer rows still load; rating back-filled from
  score+difficulty). Ranked by **Colonization Rating** (DOS compares word
  19), score as tie-break; 10 stored, 5 shown (DOS shows 5 of 6 slots).
- Columns/layout (`FUN_41f2_0f56` presenter, asm 41f2:10a0-12a8; static read,
  port_bugs.md #1217): FONTINTR (DS:0x268a) through `{skip, DS:0x830, DS:0x833, 0}`;
  title `@MISC` #192 centred over (0, 0x140) at y 3 by `FUN_104b_0430`
  (x + (w >> 1) - ((width - 1) >> 1), `reports_dos_center_x`); entries from y 0x10,
  each y += 4, line 1 at x 0xa (`FUN_104b_039a`), y += font[0] + 2, line 2 at x 0x19,
  y += font[0] + 2, line 3 centred, y += font[0] + 1 (the port drew all three
  centred in FONTTINY, 1 px left). The Retire entry's slot (local_156, `hof_inserted` from
  `game_hof_insert`; -1 from the title menu) draws all three lines in DS:0x831 (hilite); when it
  only took the catch-all slot 5, the 5th line is numbered "6." and hilited. DOS still reads
  that line's fields from slot 4 (asm 41f2:12b2-12ef), so its new 6th place is never shown; the
  port shows the retired entry there (`hof_retired`, docs/dos_bugs.md DB-57; port_bugs.md
  #1219, static read; test `unit_reports hall_of_fame_layout_is_dos`):
  1. `"<n>. <Difficulty> <Leader> of the [Free ]<Nation>"` (#19 "of the",
     #191 "Free" when declared)
  2. `"<President, <NAMES @INDEPENDENT row> | General, Continental Army |
     Leader, <Nation> Colonies> to A.D. <year>. Score: <score>"`
     (#195/#196/#197+#95, #193 "to", #194 "A.D.", #198 "Score")
  3. `"--- <#199 Colonization_Rating>: <rating>% ---"` (DS literals
     `"--- "` / `" ---"` at 0x121a/0x121f)
- Ordering: rank descending by rating (`game_hof_insert`).
- Scroll/paging: none — 5 rows shown.
- Click targets: none; Esc/Enter returns to the title menu.
- Strings: all live from `LABELS.TXT @MISC` (indices above) with the
  matching literal fallbacks; `@INDEPENDENT` republic name from NAMES.TXT
  by the entry's `nation_id`. Separator glyphs (". ", ", ", ": ") are the
  `FUN_281f_01dc/01b4/01be` strcat thunks per FUNCTION_CATALOG.
- Port status: Done (DOS layout, no golden screenshot to diff against;
  headless render checked 2026-08-29). Functions and persists correctly, but
  no golden exists for this screen (unlike every F2-F10 report), so exact
  DOS column widths/positions/chrome are unconfirmed.
  `reports_render_hall_of_fame`.

### Why there is still no Hall of Fame golden (T5.3, 2026-09-07)

Searched for reference material and found none. For the record, so nobody
repeats the search:

- `original_saves/report-screen-goldens/` holds one PNG per F2-F10 plate and
  nothing for the title-menu screens.
- `original_screenshots/` contains only `europe/` (4 PNGs).
- `dosbox-x-dumps/` and `original_memory_dumps/` are debugger text logs and
  DOSBox save states captured for AI / `VR_B465X` / brave-movement work.
  None was taken with the Hall of Fame on screen, so none carries a usable
  VGA framebuffer for it.
- There is no `HALLFAME.DAT` in `COLONIZE/` — DOS only writes one the first
  time a game is retired, and this repo's install has never been retired.
  `COLONIZE/HOF.TXT` is the **port's** own file, not a DOS artefact.

**No golden was invented and no comparison test was wired.** Doing either
would have meant asserting pixels derived from our own renderer, which is
exactly the failure mode `report_screens.md` warns about.

What a usable capture has to contain, to be reproducible:

1. A DOS `HALLFAME.DAT` (6 x 42-byte records: `name[24]`, nation, declared,
   achieved, year, season, difficulty, score, rating, tier — the record
   `FUN_41f2_14a8` builds at Retire) committed **alongside** the PNG, so the
   expected text is derivable rather than read off the image. Without the
   .DAT the screenshot pins layout but not content, and any later change to
   `game_hof_insert`'s ranking silently invalidates it.
2. A 640x400 PNG (2x native, same convention as every other golden;
   `report_screens.md` "Golden screenshots are 2x") of the title-menu Hall
   of Fame with **at least 5 filled rows**, since DOS shows 5 of its 6 slots
   and a short table would leave the row pitch and the bottom edge unpinned.
3. Rows that differ from each other in the branch-y fields, or the branches
   stay unproven: at least one `declared` entry (the "Free " prefix, @MISC
   #191) and one not; at least one `achieved` entry (line 2 becomes
   "President, <@INDEPENDENT>" instead of "Leader, <Nation> Colonies");
   and at least two different difficulty ranks.
4. Captured from the **same DOS install** as this repo's `COLONIZE/`, so
   `LABELS.TXT @MISC` #19/#95/#191..#199 and the `WOODPANL.PIK` palette
   match. Note the DAC caveat recorded under F9 in
   [reports.md](reports.md): capture it the same way the
   F2-F10 goldens were, or a raw RGB diff will be useless.

With that in hand the test is mechanical and should follow the existing
style: extend `tools/render_report_main.c` with a Hall-of-Fame mode (it
cannot reach `reports_render_hall_of_fame` today — that call takes a
`ColonizeHofRow[]`, not a `ColonizeReportId`), load the .DAT through
`game_hof_load`, render, and diff against the PNG.
