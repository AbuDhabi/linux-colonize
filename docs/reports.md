# Report screens — DOS FUN map + layout owner doc

Owner doc for `port_plan.md` P2 (F2-F10 report plates + Hall of Fame; F10 and
the Hall of Fame are in [reports_score.md](reports_score.md)).
Companion to `report_screens.md` (golden-comparison workflow, pitfalls found
while porting). This file records, per report: DOS renderer address, data
source, column layout, ordering, scroll/paging model, click targets,
strings, and current status against `src/core/reports.c`.

Code: `src/core/reports.c`/`reports.h` (split family `reports_{congress,economy,
military,names,score}.c`, seams in `reports_internal.h`), dispatcher
`reports_render_w()`. Host input/paging state lives in `src/core/game_loop_update.c`
(report click/page-advance handling, ~line 1055).

DOS key numbering: F1 opens the Colonizopedia terrain article (not a report
plate); F2 Religious through F10 Score are the nine report plates
(`reports.h` comment, cross-checked against `MENU.TXT`'s `@REPORTS` menu and
the `FUN_291f_03xx` thunk table below).

## Current-state note

All nine F2–F10 plates have golden-derived pixel layouts, real `REPORT<N>.PIK`
backgrounds, and a matching golden PNG in `original_saves/report-screen-goldens/`.
Per-report facts are below (F10 in reports_score.md); `report_screens.md` holds
the porting method and pitfalls only. P2.12 user-passed 2026-09-03.

Only one click-to-zoom exists anywhere in the DOS decompile: Labor (F4)'s
profession grid to its own per-profession detail page
(`FUN_3f41_10d8` -> `FUN_3f41_0d3e`). There is no DOS mechanic for clicking a
report row to jump to the colony/map screen.

Open gaps: leftover hardcoded English with no shipped string (port-only
empty states, HoF "Nation" + Esc hint). Hall of Fame has no golden — see
[reports_score.md](reports_score.md) "Hall of Fame golden: what a capture
needs". Congress page 2 draws all 25 FF
portraits from CC-xx.SS sprite anchors (the old 10/25 slot table is gone —
port_bugs.md Revere/Drake).

## Proportional fill bars are a DOS routine, not a spread

Every "row of little icons that fills as a resource accumulates" on a report
plate is `FUN_1097_0174` (draw loop) over `FUN_1097_0004` (geometry), reached
through the `FUN_281f_0236` thunk (`281f:023b` JMPFs straight at
`1097:0174`). Ported verbatim as `reports_draw_dos_icon_bar` (`reports.c`).
The port's own `reports_draw_icon_bar` — proportional width, icons spread
evenly across it — is *not* what DOS does and is now only used by the bars
that have not been traced to a call site yet.

    iw    = sprite width                    (+2 when the flags word has bit 2)
    step  = clamp((w - iw) / (denom - 1), 1, iw + 1)     (1 when denom < 2)
    span  = (denom - 1) * step
    shift = smallest s with (span >> s) <= w - iw        (halve until it fits)
    rem   = w - ((span >> shift) + iw)
    count = amount >> shift        den = denom >> shift
    if (min_w != 0) x += rem >> 1                        (both plates pass 0)
    per icon: blit at (x, y+1); x += step; acc += rem;
              while (acc >= den) { acc -= den; ++x; }

Three things the port had wrong and this fixes:

1. **The bar is always the full `w`.** The fill is the *length of the drawn
   run*, not a proportionally shortened bar. Both plates pass `w = 0x12c`
   (300).
2. **The step comes from the denominator, the count from the numerator.**
   `denom` = what a full bar would hold (needed crosses / bells for the next
   FF); `amount` = what you have.
3. **Icons overlap and clip each other.** With a four-digit denominator the
   step collapses to 1 and `shift` climbs, so DOS draws a dense, mostly
   self-erasing run.

Number overlay (`1097:028e` → `FUN_1097_00de`): shown when DOS's global
numbers toggle DS:0x70 is on **or** when `step == 1 && amount > 1` (the
"fused into an unreadable smear" override). `FUN_1097_00de` bumps its y by 2,
paints a `(text_width + 1) × 7` black plate at (x, y+2), then draws the
glyphs at (x+1, y+3) — so from the bar's own origin: plate at
(start_x + 2, y + 2), text at (start_x + 3, y + 3).

DOS sprite ids in these calls are **1-based over ICONS.SS**: 0x39 → #56
(cross), 0x3f → #62 (bell), 0x72 → #113 (calm chief). All three are
independently golden-confirmed.

## Shared chrome (every F2-F9 report)

- Plate bring-up: `FUN_3f41_0000` (load art+palette into the "2da8" UI box)
  -> `reports_load()`/`reports_render_w()`.
- Footer/title strip: `FUN_3f41_008a` (default y=0xb8) -> centered
  FONTTINY.FF title in `reports_render_body_start()` (`reports.c`),
  native y=5 for every report except Foreign (y=2, golden override) and
  Score (own layout).
- OK button: bottom-right, native (286,184)-(316,198)
  (`REPORTS_OK_X/Y/W/H`), `reports_render_ok_button`/`reports_ok_button_hit`.
  F10 Score and Congress page 2 have no OK button in DOS; both dismiss on a
  click anywhere instead.
- Three progress/tally widget shapes (proportional fill bar, two-icon split
  bar, natural/organic tally) — see `report_screens.md` "Three distinct
  progress/tally widget shapes"; `reports_draw_icon_bar()`/
  `reports_draw_icon_bar_pair()` implement all three.
- F-key dispatch thunks (`FUN_291f_03aa`..`041a`, listed per report below;
  F10's in reports_score.md) live in segment `291f`'s far-thunk table; `FUN_291f_0f4a` is the shared
  plate bring-up thunk, `FUN_291f_0ee8` the shared footer-strip thunk.

## F2 - Religious Adviser

- DOS FUN: **`FUN_3f41_0618`** (viceroy_unpacked.c:69611, 39 lines; not
  `06d0`, which is F3). `0618` pushes `2` to the plate bring-up
  (→ `REPORT2.PIK`) and draws its fill bar with sprite `0x39` (= ICONS.SS
  #56, the cross) over `BX = nation+0x2e` (current) / `DX = nation+0x30`
  (needed).
- The whole body is: plate bring-up, the crosses bar, one cheat-gated line,
  footer strip. There is **no** recruit-pool line, immigrants-en-route line
  or FF-name tail list, and `0x53da`/`0x53e2` are F3's force arrays, untouched
  by `0618`. `religious.png` below the bar is plain REPORT2.PIK art, and
  `reports_render_religious` correctly draws nothing there.
- Background: `REPORT2.PIK`.
- Data source: nation crosses pool only — `nation+0x2e` accumulated,
  `nation+0x30` needed.
- Columns/layout: single column — title, crosses proportional fill bar
  (`3f41:0670` pushes x=10, y=25, w=300, min_w=0, split=0, flags=1;
  ICONS.SS#56; icons blit at y+1=26).
- Cheat-only line: under `if ((*(byte *)0x5383 & 0x20) != 0)` — DS:0x5383
  bit5 = `head.game_options.cheats_enabled`, the Alt-WIN unlock — DOS
  formats DS:0x11a9 `"(%d of %d)"` from the same `nation+0x2e`/`+0x30` pair
  and draws it at x=10, y=25, colour 15 (port_bugs.md #977). The template
  has no catalog row, so it is read out of the shipped
  `VICEROY.EXE` (`reports_ds_string`) and composed by hand, never through
  printf; with no EXE present nothing is drawn.
- Ordering: crosses bar, then the cheat line when the flag is set.
- Scroll/paging: none.
- Click targets: none (shared OK button only).
- Strings: title "RELIGIOUS ADVISER REPORT" resolves live (`reports_title`).
- Font: FONTTINY for the bar's number overlay and the cheat line, like every
  other report body — the 3f41 overlay loads no second font (port_bugs.md #428,
  #979); the golden cannot catch a wrong font here, its bar has step 2 and so
  draws no number.
- Port status: Done (golden `religious.png`) —
  `reports_render_religious`. The crosses bar is **pixel-exact** via
  `reports_draw_dos_icon_bar`: 25 crosses over a denominator of 128 give
  step 2 / shift 0 / rem 38, i.e. icon origins 10, 12, 14, 16, 19, 21, 23,
  26 … — the irregular 3px gaps are the Bresenham remainder, and they are on
  the golden too.

## F3 - Continental Congress

- DOS FUN: **`FUN_3f41_06d0`** (69650, 174 lines) + its Ghidra-split tail
  `FUN_3f41_0ae6` (not `0618`, which is F2). `06d0` pushes `3` to the plate bring-up (→ `REPORT3.PIK`), calls
  `FUN_291f_0f66` = `FUN_4345_0982` (bells needed), reads DS:0x53d0
  (`rebel_sentiment_report`), and draws its fill bar with sprite `0x3f`
  (= ICONS.SS #62, the bell). FF debate/nominate is a separate DOS screen,
  `FUN_4345_06d2` (73177) via `FUN_2a1f_0000`/`FUN_291f_0f74` — not part of
  the F3 plate itself.
- Background: page 1 `REPORT3.PIK` (own desk/study — was orphaned in
  `k_report_files[]` until fixed during porting), page 2 `CCBKGD.PIK` (hall
  photo, full-bleed, no chrome).
- Data source: `nation.liberty_bells_total` / next-FF threshold
  (`FUN_4345_0982`), `nation.rebel_sentiment`/tory split, expeditionary-
  force pool counts (Regulars/Cavalry/Artillery/Man-O-War), FF-owned
  bitmask for the name list (`FUN_4345_01a6`).
- Columns/layout: page 1 — bells proportional fill bar (`3f41:0890` pushes
  x=4, y=`TEXT1_Y + one text row`, w=300, min_w=0, split=0, flags=1; icons
  blit at y+1), rebel/
  tory two-icon split bar (flags then crowns, 50-slot budget, x=4,y=71),
  expeditionary-force 4-box natural tally (y=102, ~2.2px/unit), 4-column FF
  name grid (x=8, step=78; `FUN_3f41_0ae6` is the Ghidra-split tail of this
  list). Page 2: full-bleed FF group portrait composite, no text.
- Ordering: fixed bells -> sentiment -> force -> FF list; page 2 portraits
  paint from CC-xx.SS sprite anchors in `k_ff_portrait_draw_order[]` (all 25).
- Bells bar (not "36 bells at a 5px pitch, capped by `max_icons`"). The 2×7 mark on
  `continental_p1.png` is not a separate glyph and there is no missing
  narrow sprite: it is `ICONS.SS` #62 with its whole body painted over by
  the *next* bell. Feeding `FUN_1097_0004`'s real geometry (see
  "Proportional fill bars are a DOS routine") with `amount = min(pool,
  need) = 1135`, `denom = need = 1849`, `w = 300`, `iw = 10`:

      step = (300-10)/1848 = 0 -> clamped to 1
      span = 1848,  shift = 3 (1848>>3 = 231 <= 290),  rem = 300-241 = 59
      count = 1135>>3 = 141 bells,  den = 1849>>3 = 231

  so DOS draws **141** bells from x=4, each 1px on from the last plus a
  Bresenham +1 every 59/231 of a step. A bell 1px from its neighbour is
  erased completely; the ~36 that happen to get a 2px gap keep exactly two
  columns — the leftmost ink pixel of each sprite row (#62 col 2 on rows 3
  and 9, col 3 on rows 5-8: the brown crown-end over the grey body edge).
  The last bell has no successor and shows whole. Predicted origins run
  7, 12, 17 … 175, 179 with three 4px steps; the golden's are identical.
- Bells clamp: `3f41:07c8` reads the threshold into `local_56`, then
  `local_68 = min(pool, threshold)` — so an over-full pool cannot overrun
  the bar. (When DS:0x5382 bit 1 is set the threshold is first raised to
  `max(pool, threshold)`.)
- Cheat-only line (port_bugs.md #978): between the clamp and the bar, under
  `if ((*(byte *)0x5383 & 0x20) != 0)` (DS:0x5383 bit5 =
  `head.game_options.cheats_enabled`), DOS composes a line from the
  shortfall `local_56 - min(pool, local_56)`, the DS:0x2dee token and
  `local_56`. The overlay asm (OVL06_L0040 000c01-000c78) settles the
  composition the decompiled C had lost: `" (" <needed - min(pool, needed)>
  " " <@MISC 26 "in"> " " <needed> ")"`, appended to the header buffer, which
  DOS then draws whether or not the header itself is empty.
- Not a gate: Ghidra structures `06d0`'s tail so that
  the "Founding Fathers:" list *and* the page-1→page-2 advance sit inside
  `if (sum(backup_force) != 0)`, with an `else` falling into
  `FUN_2f2b_0d89` (a terrain-tile helper — an unresolved overlay
  placeholder, see conventions.md). It is a mis-structure, not behaviour:
  `dutch-reports.SAV` has `backup_force = [0,0,0,0]` and
  `continental_p1.png` still shows the FF header and name grid (ink at
  native rows 126-176). Drawing the list unconditionally is correct.
- Scroll/paging: 2 pages; any dismiss on page 1 advances to page 2 instead
  of leaving the report; page 2 closes on any click.
- Click targets: none inside a page; page-advance only.
- New-father reveal: when page 2 opens straight off an election (FUN_4345_024a
  with `param_2` = the elected index), DOS clears that father's nation bit,
  presents the hall without him, sets the bit back and presents the redraw
  through the LFSR fizzle FUN_281f_03ea(8), so the new portrait un-dissolves
  into place. Port: `game_congress_reveal_new_father` (game_loop.c), called from
  the FF-announce OK in game_dialogs.c. Opening Congress from the reports menu
  passes no index and animates nothing.
- Header line (three-way, `06d0` @ 69679-69695; smell audit #87): with
  DS:0x5382 bit 0 clear it is "Next Continental Congress Session:" plus
  " (name)" when `head+0x12 >= 0`; with bit 0 set and bit 1 (REF arrived)
  clear it is "<DS:0x53d4 adjective> Intervention:"; once the REF has
  arrived the string buffer stays empty and **no header is drawn**. The
  ally adjective is `FUN_281f_09a4(DS:0x53d4)` read raw — no crown/self
  exclusion and no fallback search (same raw read for the "<Ally>
  Intervention Force:" lineup header at 69777).
- Strings: title "CONTINENTAL CONGRESS ACTIVITIES" resolves live
  (`reports_title`); "Next Continental Congress Session" (#112),
  "Rebel"/"Tory"/"Sentiment" (#69/#70/#71), "Expeditionary Force" (#85)
  and "Founding Fathers" (#89) resolve live from `@MISC` (2026-08-28),
  composed with the golden's ":"/spacing; need FONTTINY not FONTSMAL.
- Port status: Done (golden `continental_p1.png`/`continental_p2.png`) —
  `reports_render_congress_page1`/`_page2`. The bells bar (and the "1135"
  plate/number over it) is **pixel-exact** (0 differing pixels in
  x=2..189, y=33..45). Page 2 is 10 pixels off; page 1 as a whole is 10 369, of which the
  rebel/tory bar (rows 67-80, 3 520) and the expeditionary-force tally
  (rows 100-116, 3 407) are the two remaining `1097`-family widgets that
  have not been traced to their call sites — the obvious next pass.

## F4 - Labor Adviser

- DOS FUN: grid `FUN_3f41_10d8` (70058, 123 lines, "profession grid;
  click->0d3e") — thunk `FUN_291f_03f0`. Detail `FUN_3f41_0d3e` (69914, 144
  lines, "one profession's colony placements") — thunk `FUN_291f_0f3c`.
- Background: `REPORT4.PIK`.
- Data source: colonist `profession` byte, summed three ways: colony
  population slots ("In Colonies"), map units of colonist-derived types
  0-5 (Colonists/Soldiers/Pioneers/Missionaries/Dragoons/Scouts) not on
  Europe/own-colony tiles ("On Mapboard"), same types on a Europe-side tile
  ("Off Mapboard/Europe"). The map-unit bucket is filed under the **raw**
  profession byte — DOS `local_c6[*(char *)(i*0x1c + 0x315b)]++` at 70113,
  and the detail view compares the same raw byte at 69967. There is no
  unit-type fallback in either (smell audit #90: `u->type` is an @UNIT id,
  a different id space from @JOB); a byte outside the job table is skipped.
- Columns/layout: 9-row x 3-column fixed table, not a straight 0..27 job-id
  scan — `k_labor_layout[3][9]` skips job 18 (Expert Teacher — a colonist
  type cut from the final DOS game; unreachable, references to it are dead
  ends, do not port) and 23
  (Veteran Dragoon), and job 19 (Free Colonist) is out-of-order at the
  bottom of column 3. Row0 y=26 step=18; col0 x=2 step=105. Detail view:
  header (icon+name+total), Off Mapboard/On Mapboard/In Colonies
  breakdown, then a 3-column list of "<colony name>: N".
- Ordering: fixed layout table, not job-id or count order.
- Scroll/paging: none (single screen); detail reached by click, no page
  index.
- Click targets: the only click-to-zoom in any report — grid cell -> detail
  (`reports_labor_cell_hit`, wired via `game->labor_detail_job` in
  `game_loop.c`). Esc/Enter/OK on the detail view returns to the grid.
- Strings: title "LABOR ADVISER REPORT" resolves live (`reports_title`);
  job names (live from `NAMES.TXT @JOB`) via
  `reports_job_name()`; "(Click on item to zoom)" (`@MISC` #56) and the
  detail page's "Off Mapboard (Europe)"/"On Mapboard"/"In Colonies"
  (#53/#54/#55) resolve live (2026-08-28).
- Port status: Done (golden `labor.png`/`labor_detail.png`) —
  `reports_render_labor_grid`/`_detail` (`reports_congress.c`). Real gap
  found and fixed while porting: `UNITS_JOB_NONE` (28) must fold into Free
  Colonists (19) or unspecialized colonists silently drop out of every
  bucket.

## F5 - Economic Adviser

- DOS FUN: header chrome `FUN_3f41_1438` (70181) — thunk
  `FUN_291f_03e2`/`0ef6`. Body `FUN_3f41_1710` (70281, 145 lines, "cargo
  buy/sell ledger table"). Page-2 cargo rows `FUN_3f41_1550` (70212,
  "colony cargo-stock rows") — thunk `FUN_291f_0f2e`.
- Background: `REPORT5.PIK`.
- Data source: page 1 (European Trade) — `nation.trade.tons[c]`/`.gold[c]`
  (net bought/sold per cargo), `EuropeScreen.cargo[c].bid`/`.ask` when a
  live Europe session exists, else `nation.trade.euro_price[c]` as bid
  fallback. Page 2+ (Cargo in Port) — per-colony warehouse stock, one row
  per owned colony.
- Columns/layout: page 1 — 16-row x 4-column table (Tons/Gold/Bid
  Price/Ask Price), row0 y=33 step=8, divider x=67, right-aligned columns
  at x=90/144/199/251. Page 2 — colony-name rows (17/page) x cargo-icon
  columns, row0 y=42 step=8, divider x=87, col step=14.
- Ordering: page 1 fixed cargo-id order 0..15 (`reports_render_economic_trade`); page 2
  colony order = save's colony array order, filtered to this nation.
- Scroll/paging: 1 + ceil(colony_count/17) pages
  (`reports_economic_page_count`).
- Click targets: none; OK/Esc/Enter advances page, wraps to map from last.
- Strings: title "ECONOMIC ADVISER REPORT" resolves live (`reports_title`);
  "Tons"/"Gold"/"Bid Price"/"Ask Price" column headers from `LABELS.TXT`
  `@MISC` #58/#59/#203/#204 via `reports_labels_field`; page subtitles
  "European Trade" (`@MISC` #206) and "Cargo in Port" (`@MISC` #207).
- Number format (`FUN_3f41_1710`, raw 70281-70422):
  tons come from `(nation*0x4f + cargo)*4 + -0x773c` and gold from the same
  index at `-0x777c`, both 32-bit. Per value, independently:
  `if (high >= 0 && (high > 0 || low > 9999)) { FUN_1d1d_0f92(&low, 1000, 0);
  splice DS:0x2e2c; }` — a divide-by-1000 plus a separator/suffix token for
  large figures — DS:0x2e2c resolves to LABELS.TXT @MISC 57, "K", and Gold
  then appends DS:0x11b4 "$" after it ("12K$") (port_bugs.md #975). The sign
  attribute (`local_8c = high < 0 ? 4 : 2`) is likewise computed twice, once
  per column (port_bugs.md #976; the palette indices stay as measured off the
  golden, since DOS's 2+8/4+8 index its own default palette).
- Port status: Done (golden `economic_p1.png`/`economic_p2.png`) —
  `reports_render_economic_trade`/`_cargo` (`reports_economy.c`).

## F6 - Colony Adviser

- DOS FUN (both halves of F6 live here; the Garrisons pair `1e80`/`1ed8` is
  **not** F7): all four functions push `6`
  to the plate bring-up (`thunk_FUN_291f_0f4a(...,6)` → `REPORT6.PIK`) and
  differ only in the subtitle string they load.
  - Military Garrisons: chrome `FUN_3f41_1e80` (70538, subtitle DS:0x2f5a)
    — thunk `FUN_291f_0f58`; body `FUN_3f41_1ed8` (70555) — the stack
    count/reorder/pitch loop described under Columns/layout below. It tail-
    calls `thunk_FUN_291f_0f20` to hand over to the Sons of Liberty half.
  - Sons of Liberty: chrome `FUN_3f41_1b94` (70426, subtitle DS:0x2f5c) —
    thunk `FUN_291f_0f04`; body `FUN_3f41_1bec` (70443, 95 lines) — SoL
    percent, the `FUN_281f_09fc(0x14)`/`(0x13)` building test and the
    worker loop over `colony+0x1f`. It draws no unit stack at all.
  - Panel draw helper `FUN_647e_09da` (102793, "draw colony report panel")
    — thunk `FUN_2a1f_0770`.
  - F-key dispatch thunk: `FUN_291f_03d4` → `FUN_3f41_1ed8`, i.e. the key
    lands on the Garrisons half and the SoL half is reached by its tail
    call.
- Background: `REPORT6.PIK`.
- Data source: per colony — population, fortification bits (popcount ->
  marker tier), SoL % via `colony_prod_sol_percent` (not the raw rebel-pct
  field alone, which under-reports Bolivar's +20% bonus), building in
  progress, liberty-bell accumulator, garrison unit list matched by
  x/y/nation/type directly against `col1->unit[]` rather than the bridged
  pool (see `report_screens.md`'s `col1_bridge_apply` orders-byte pitfall).
- Columns/layout: shared left sidebar (fort icon + population digit + name,
  colored by SoL tier: white <50%, green >=50%, blue 100%). Military
  Garrisons pages draw the tile's garrison from x=110 for as long as
  x <= 300, with pitch = clamp(210 / n, 1, 18) where n is the tile stack's
  FUN_1427_0d38 case-10 count (non-ship units whose @UNIT attack > 1) — so a
  large garrison packs tighter instead of being truncated (a 15-unit stack
  draws 14 at pitch 14). Drawing itself takes any non-ship unit with
  attack >= 1, so a Scout is drawn but does not tighten the row. Sons of
  Liberty pages show SoL flag+percent, building name, bell icon+count, up
  to 6 worker-slot icons (x=252, step=12, y row_top-6, shadow ink 95; flag
  and bell at row_top-5; sidebar/units at row_top-7, golden-measured 2026-10-28). 9 rows/page, row0 y=27 step=17.
- Ordering: colony array order, filtered to this nation.
- Scroll/paging: 2*ceil(colony_count/9) pages — first half Military
  Garrisons, second half Sons of Liberty (`reports_colony_page_count`).
- Click targets: none; OK/Esc/Enter advances page.
- Strings: title "COLONY ADVISER REPORT" and the two page subtitles
  ("Military Garrisons" `@MISC` #208, "Sons of Liberty" `@MISC` #209) all
  resolve live (same `reports_labels_field` pattern as F5); colony/building
  names from save.
- Port status: Done (golden `colony_p1.png`/`colony_p2.png`) —
  `reports_render_colony_sidebar`/`_garrisons`/`_sol`
  (`reports_economy.c`). The garrison row is re-sorted before drawing as DOS
  does (`FUN_1427_04d6(head, 1)`: stable pseudo-size **descending** in the final
  chain, golden Artillery before Dragoon; Dragoon/Scout/Cont.Cav. = 2,
  Artillery = 3). Project-wide palette fixes that came
  out of this report: `report_screens.md` "unit_chrome on report
  backgrounds".

## F7 - Naval Adviser

- DOS FUN: header chrome `FUN_3f41_20b4` (70630-70671) — thunk
  `FUN_291f_0f12`; body `FUN_3f41_220c` (70675-70785). `20b4` is the one that
  pushes `7` to the plate bring-up (→ `REPORT7.PIK`), draws the four column
  headers DS:0x2e34/0x2e36/0x2e38/0x2e3a, three dividers and eight row rules
  (y 40..180 step 20). Not `FUN_3f41_1e80`/`1ed8`: both push `6` and are
  F6's Military Garrisons pair (port_bugs.md #973).
  F-key dispatch thunk: `FUN_291f_03c6` → `FUN_3f41_220c`.
- Background: `REPORT7.PIK`.
- Data source (`FUN_3f41_220c`): a single pass over the unit array for
  `(unit+0x3147 & 0xf) == nation`. Ships are always listed; a non-ship unit
  (`type < 0xd || 0x12 < type`) is listed only when
  `FUN_281f_0768(unit+0x3144, +0x3145) != 0`. Per row: cargo icons from a
  loop over the `unit+0x3150` holds, the row name from the @UNIT name table
  (`type*0xe + 0x5230`), Location from `FUN_291f_0f82(x, y, nation&0xf, 0)`,
  Destination from the goto target (`+0x314d`/`+0x314e`) when the orders
  byte `+0x314c` is 3, 0xb or 2 (note: **not** 12/AI_MOVE, which the port's
  shared `units_orders_follow_goto` does accept), else `FUN_281f_0302(x, y)`
  and, when that is 0, a Europe-lane arm keyed on `nation - x`: 0x18/0x1c →
  `DS:0x5426 + nation*0x34`, 0x0c/0x10 → `DS:0x838c + nation*2`, anything
  else → empty. `FUN_291f_0f82` = `FUN_49dd_02d0` = colony name at the tile
  else the `(x, y)` pair — `naval.png` shows both forms.
- Columns/layout: 4-column table — Ship (icon+class) / Cargo (goods icons,
  100-unit stacks colored, partial grey) / Location / Destination. DOS row0
  y is `0x2a` = 42, step `0x14` = 20, unit sprite at row_y, cargo icons at
  row_y+3, text at row_y+6; the port's golden-measured ROW0_Y 40 + ICON_DY 2
  + TEXT_DY 8 reproduce all three exactly, so the constants differ but the
  ink does not. Cargo icons start at x=88 (`[BP-0x56] + 0x56`) with pitch
  `0xc` = 12. Ship sprite from
  x=2 and its name at x=26; a non-ship row's sprite sits at x=88 and its
  name at x=112. Column dividers x=82/162/242; Location is centred in
  [162,242] and Destination in [242,318].
- Ordering: unit-array order, with each listed non-ship unit taking its own
  row wherever the array puts it — there is no pinning of a passenger to its
  carrier (on `naval.png`'s save the two orders coincide).
- Scroll/paging: 7 rows/page (`reports_naval_page_count`).
- Click targets: none; OK/Esc/Enter advances page.
- Strings: title "NAVAL ADVISER REPORT" and the 4 column headers ("Ship"/
  "Cargo"/"Location"/"Destination", `@MISC` #61-64, a clean consecutive
  block) all resolve live; body needs FONTTINY not FONTSMAL (FONTSMAL
  renders upper-case-only and too wide at this size).
- Port status: Done (golden `naval.png`, 1896 px diff) —
  `reports_render_naval` (`reports_military.c`). Row list, ordering, row
  labels (@UNIT name for every row, port_bugs.md #605) and the cargo-icon
  geometry follow `FUN_3f41_220c` (port_bugs.md #973). Still divergent by
  necessity: the Europe-lane rows —
  [known_divergences.md](known_divergences.md) #974. Import-side traps this
  report exposed: `report_screens.md` "`col1_bridge_apply` transport-chain
  bugs". REPORT7.PIK has no fully-saturated Dutch orange for
  `unit_chrome_blit_unit_for_palette`'s nearest-RGB match; left duller,
  mechanism not identified.

## F8 - Foreign Affairs Advisor

- DOS FUN: `FUN_3f41_2548` (70787, 247 lines, "euro rivals, war, strength")
  — thunk `FUN_291f_03b8`.
- Background: `REPORT8.PIK`.
- **Read from the raw `.asm`** (3f41:2548..2aca) — Ghidra drops this
  function's pushed values and reorders them, so the decompiled C mis-pairs
  every label with its number.
- Data source: per-nation block, fixed English/French/Spanish/Dutch order.
  - **Crown slot:** `head.crown_nation_id` (DS:0x53d2) — that block prints
    a centered "(Withdrawn from New World)" (@MISC 190) and nothing else.
    This, not `player[n].control`, is DOS's gate; `dutch-reports.SAV` has
    Spain as both, which is why the old control-based reading fit.
  - **"Free":** `nation[n].nation_flags` bit `0x04` splices @MISC 191
    ("Free") into the header between the leader's name and the adjective,
    and drops that block's Rebels/Tories line.
  - **War/peace + met:** one byte, one direction —
    `nation[a].euro_relation[b]` (DOS `FUN_281f_0a38(a, b)`): bit `0x20`
    = met (an unmet peer is not listed at all), bit `0x40` = at peace,
    clear = at war. Not "`(ab|ba) & 0x02` in either direction" (a
    single-golden empirical fit): `ai_diplo.h`'s `AI_DIPLO_WAR` 0x02 is the
    attack-site bit in the same byte, and this report never reads it.
    The Euro-attack `@HAVETREATY` gate reads **the same byte in the same
    direction** — DOS `FUN_465b_0000` tests `FUN_281f_0a38(attacker,
    target) & 0x40` (`viceroy_unpacked.c:75545`) — so this report and that
    prompt can never disagree. port_bugs.md #382: the port used to OR both
    directions there, which made a peer's one-sided peace bit produce
    "War" here and "we have signed a peace treaty" on the map. DOS's
    both-direction OR appears further down `465b` and only decides whether
    a war *declaration* is announced.
  - **Rebels/Tories:** `col1->stuff.census_pop_proxy[nation]` (DS:0x9410),
    not summed colony `.population` (which undercounts by every field
    colonist-type unit). `rebels = pop * rebel_sentiment / 100`.
- Columns/layout: one fixed block per nation, always 4 slots (own nation
  and unmet/crown peers skipped inside a block, not compacted). Block rule
  y=10/55/100/145; `local_60` starts at 13 and every drawn body line
  advances it by `FONTTINY.height + 1` = 7px:

  | line | y | contents |
  |------|---|----------|
  | header | block_top+3 | "<Leader>'s [Free ]<Adjective>:" |
  | de Witt A | block_top+10 | Colonies / Average Colony / Population |
  | de Witt B | block_top+17 | Military Power / Naval Power / Merchant Marine |
  | peers | +7 | up to 3 cells, **one line** |
  | rebels | +7 | "Rebels: N   Tories: N" (only if a peer was drawn) |

  Without de Witt the two detail rows are absent and the peer line sits at
  block_top+17, as the golden shows. Cell x for both the detail grid and
  the peer line is the `.asm`'s `if (x < 0x50) x = 0x50; else x += 0x50`
  ladder: **2 / 80 / 160 / 240** — a single row, never a 2-column wrap
  (the earlier "2-column grid" reading came from a golden where every
  block has only 2 peers).
- **Jan de Witt detail grid.** Gated on the *viewing*
  nation owning FF #4 or on `head.show_entire_map` (DOS
  `FUN_281f_07b4(viewer, 4) || DS:0x53a2`). Six cells, drawn as one
  "<label>: <n>" string each in the Rebels/Tories colour, values straight
  off the DOS census block (`stuff`, written by `FUN_4962_0018`):

  | cell | @MISC | source |
  |------|-------|--------|
  | Colonies | 95 | `stuff.colony_counts[n]` (DS:0x9298) |
  | Average Colony | 97 | `stuff.avg_colony_pop[n]` (DS:0x944e, u16) |
  | Population | 96 | `stuff.census_pop_proxy[n]` (DS:0x9410) |
  | Military Power | 98 | `stuff.land_combat_strength[n] >> 3` (DS:0x941c) |
  | Naval Power | 99 | `(unit_type_counts[n][16] + [17]) * 8` — @UNIT 16 Privateer / 17 Frigate; Man-O-War (18) excluded |
  | Merchant Marine | 100 | `stuff.ship_cargo_totals[n]` (DS:0x9414) |

  Screen order is **not** @MISC order: row A is 95/97/96, row B is
  98/99/100, exactly as the `.asm` pushes them (`avg_colony_pop`: see
  [save_format_map.md](save_format_map.md) Stuff row 556).
- Ordering: fixed English->French->Spanish->Dutch, always 4 block slots.
- Scroll/paging: none — no `reports_foreign_page_count` exists (unlike
  Naval/Economic/Colony); 4 fixed blocks always fit.
- Click targets: none.
- Strings: title "FOREIGN AFFAIRS REPORT" (drawn ~3px higher than the
  shared default — a real REPORT8.PIK-specific override, resolves live
  via `reports_title`); "Rebels"/"Tories" `@MISC` #86/#87 (the plurals exist,
  past a 64-line cap that once hid them); "Peace"/"War" `@MISC` #102/#101;
  "(Withdrawn from New World)" `@MISC` index 190 (an index, not raw line
  "#205").
- **Static-buffer pitfall:** `reports_labels_field` hands back a single
  `static char[64]`, so resolving @MISC 101 and 102 into two `const char*`
  up front leaves both pointing at the last one ("Peace" in the War colour).
  Use `reports_misc_word`'s caller-owned buffer.
- Port status: Done (golden `foreign.png`) — `reports_render_foreign`.

## F9 - Indian Adviser

- DOS FUN: `FUN_3f41_010a` (69451, 160 lines, "tribe rows, villages,
  converts") — thunk `FUN_291f_041a`.
- Background: `REPORT1.PIK` (`FUN_3f41_010a` passes 1 to the plate loader `FUN_3f41_0000`; `REPORT9.PIK` ships unused).
- Data source: every tribe with `indian.euro_diplo[human] != 0` (bit 0x20
  met / 0x40 peace). Muskets shown = `indian.muskets` plus one per live
  tribe unit of type Armed Brave (20) or Mtd. Warrior (22), sum x50 — a
  real DOS formula found only by reading `FUN_3f41_010a` directly, no
  scale factor on the raw field alone reproduces the golden. Horse Herds =
  `indian.horse_herds` read raw. Missions = villages whose `mission` byte's
  low nibble equals the viewing nation, not "any mission".
- Columns/layout: flat unpaginated list, 2-line block per tribe — 16x16
  chief portrait at x=10, top = name_y - 3 (ICONS.SS #113 + alarm quartile;
  see "Chief portrait" below) + "<PluralTribeName>:"
  (NAMES.TXT @TRIBES col 0) + right-aligned tribe level, then a black stats
  line: settlement count + `@LEVELS` noun (always shown) / Missions /
  Muskets / Horse Herds (each skipped when 0). Row0 y=28 step=21.
- Ordering: tribe array order, filtered to tribes met by the viewing
  nation.
- Scroll/paging: none in DOS — `FUN_3f41_010a` has an unconditional
  8-iteration loop, no paging logic. Porting in pagination would invent
  behavior DOS doesn't have.
- Click targets: none.
- Strings: title "INDIAN ADVISER REPORT"; tribe names from NAMES.TXT
  @TRIBES col 0 (plural, not the singular/adjective col 1 used elsewhere);
  tribe-level words live from `@LEVELS` (`reports_tribe_level`, 2026-08-26);
  "Missions"/"Horse Herds" from `@MISC` 28/45, "Muskets" from `@CARGO`. The
  settlement noun is **not** a hardcoded "Villages": DOS-LITERAL
  `FUN_3f41_010a` raw 69557-69564 takes the tribe's own `NAMES.TXT @LEVELS`
  row (indexed by `indian.tech`), column 1 when the count is exactly 1
  ("1 City") and column 2 otherwise ("3 Camps").
- Name/level lines are four FONTTINY passes (`FUN_3f41_010a`, 3f41:018f-022c /
  0265-02cb): ink 0 at (x+1, y), (x, y+1), (x+1, y+1), then the tribe ink at (x, y);
  the level's x is 0x136 - (width - 1) (`FUN_281f_0114`). The black stats line is one
  pass, columns every 0x38 from x 40. Name lines are pixel-exact against indian.png
  (`unit_reports indian_name_lines_golden`, port_bugs.md #1207). Tribe name/level
  inks are VICEROY.PAL indices < 152 (dist2=0 on the plate DAC).
- Chief portrait (not "always #113"). `3f41:0522`..`3f41:05d2`:

      q = FUN_281f_0a60(FUN_281f_030c(tribe, viewing_nation))
        = quartile of the Indian->Euro alarm word, cuts 25/50/75
      if (indian_record[+3] & 0x80)  q = 3        // the `extinct` bit
      FUN_281f_0254(AX = q + 0x72, ..., DX = x, y)

  `0x72` is a DOS sprite id and those are 1-based over ICONS.SS, so the
  sheet index is **113 + q** — #113 calm … #116 hostile. (#117 exists but
  this call site cannot reach it.) The five sprites are byte-identical
  except for colour 136 spreading through the mouth rows, i.e. one
  expression ramp, not five tribes. `local_64` — a second, `4 - (alarm mod
  25)/5` value clamped to 0..4 — is computed in the same block and then
  never read; it is dead in this function.
  Golden: `indian.png`'s two rows are Arawak (tribe 2) and Cherokee
  (tribe 4), both `alarm_by_player[3] == 0` in `dutch-reports.SAV`, so both
  q=0 (#113 at x=10, y=25/46) — which is why #113 alone used to fit.
- Port status: Done (golden `indian.png`) — `reports_render_indian`. Both
  chief portraits are pixel-exact. `indian.png`'s plate colours used to look
  like "a different DAC": the port drew REPORT9.PIK, DOS draws REPORT1.PIK.
  With plate 1 the raw RGB diff is 40 px (the mouse pointer column).

## F10 - Colonization Score and Hall of Fame

Moved to [reports_score.md](reports_score.md): F10 Score, the Retire chain / exploits screen, the Hall of Fame, and what a Hall of Fame golden would need.
