# Report screens — DOS FUN map + layout owner doc

Owner doc for `port_plan.md` P2 (F2-F10 report plates + Hall of Fame).
Companion to `report_screens.md` (golden-comparison workflow, pitfalls found
while porting). This file records, per report: DOS renderer address, data
source, column layout, ordering, scroll/paging model, click targets,
strings, and current status against `src/core/reports.c`.

Code: `src/core/reports.c`/`reports.h`, dispatcher `reports_render()`. Host
input/paging state lives in `src/core/game_loop.c` around line 7564.

DOS key numbering: F1 opens the Colonizopedia terrain article (not a report
plate); F2 Religious through F10 Score are the nine report plates
(`reports.h` comment, cross-checked against `MENU.TXT`'s `@REPORTS` menu and
the `FUN_291f_03xx` thunk table below).

## Current-state note

All nine F2–F10 plates have golden-derived pixel layouts, real `REPORT<N>.PIK`
backgrounds, and a matching golden PNG in `original_saves/report-screen-goldens/`.
`report_screens.md` has dedicated write-ups for F2/F3/F6/F7/F8/F9/F10; F4/F5
carry the same golden-measured constants in code without a prose section.
P2.12 user-passed 2026-09-03.

Only one click-to-zoom exists anywhere in the DOS decompile: Labor (F4)'s
profession grid to its own per-profession detail page
(`FUN_3f41_10d8` -> `FUN_3f41_0d3e`). There is no DOS mechanic for clicking a
report row to jump to the colony/map screen.

Open gaps: leftover hardcoded English with no shipped string ("Villages" on
F9, port-only empty states, HoF "Nation" + Esc hint). Hall of Fame has no
golden and no reference capture exists in the repo — see the Hall of Fame
section for exactly what one would have to contain. Congress page 2 draws
all 25 FF portraits from CC-xx.SS sprite anchors (the old 10/25 slot table
is gone — bugs.md Revere/Drake).

T5.3 close-out (2026-09-07): the "fat bell glyph" and the "F9 headband is
always #113" gaps are both closed — see "Proportional fill bars are a DOS
routine" below, and F9's Icon note. Both fixes are pixel-exact against their
goldens. Still open on Congress page 1: the rebel/tory split bar (rows
67-80) and the expeditionary-force tally (rows 100-116) are the *other* two
entry points into the same `1097` family and are still port approximations
— together they are ~6.9k of the plate's remaining 10.4k differing pixels.

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
  -> `reports_load()`/`reports_render()`.
- Footer/title strip: `FUN_3f41_008a` (default y=0xb8) -> centered
  FONTTINY.FF title in `reports_render_body_start()` (`reports.c:421`),
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
- F-key dispatch thunks (`FUN_291f_03aa`..`041a`, listed per report below)
  live in segment `291f`'s far-thunk table; `FUN_291f_0f4a` is the shared
  plate bring-up thunk, `FUN_291f_0ee8` the shared footer-strip thunk.

## F2 - Religious Adviser

- DOS FUN: **`FUN_3f41_0618`** (viceroy_unpacked.c:69611, 39 lines) — the
  F2/F3 pair was **swapped in this doc until 2026-09-07**. `0618` pushes `2`
  to the plate bring-up (→ `REPORT2.PIK`), formats DS:0x11a9 `"(%d of %d)"`
  (the recruit-pool / immigrants-en-route line), and draws its fill bar with
  sprite `0x39` (= ICONS.SS #56, the cross) over `BX = nation+0x2e`
  (current) / `DX = nation+0x30` (needed). `06d0` does none of that and is
  the Congress plate — see F3.
- Background: `REPORT2.PIK`.
- Data source: nation crosses pool (needed/accumulated split), founding-
  father bitmask for the FF-name tail loop (0x25-entry table at `-0x69ae`),
  immigration/recruit-pool counts (4-slot arrays at `0x53da`/`0x53e2`).
- Columns/layout: single column — title, crosses proportional fill bar
  (`3f41:0670` pushes x=10, y=25, w=300, min_w=0, split=0, flags=1;
  ICONS.SS#56; icons blit at y+1=26 — 2026-09-07, was x=10/y=27 with a
  proportional width), two conditional summary lines (recruit
  pool / immigrants en route — suppressed entirely when zero, not shown as
  "0"), FF-name tail list wrapping across 4 columns.
- Ordering: crosses bar always first; summary lines conditional.
- Scroll/paging: none.
- Click targets: none (shared OK button only).
- Strings: title "RELIGIOUS ADVISER REPORT" resolves live (`reports_title`);
  FF names live from `NAMES.TXT @FATHERS` (`reports_ff_name`, `k_ff_names[]`
  is the no-assets fallback).
- Port status: Done (golden `religious.png`) —
  `reports_render_religious`. The crosses bar itself is **pixel-exact**
  since 2026-09-07 (0 differing pixels in x=4..79, y=24..41; was 471) after
  moving it onto `reports_draw_dos_icon_bar`: 25 crosses over a denominator
  of 128 give step 2 / shift 0 / rem 38, i.e. icon origins 10, 12, 14, 16,
  19, 21, 23, 26 … — the irregular 3px gaps are the Bresenham remainder, and
  they are on the golden too. The rest of the plate (FF-name tail) still
  differs.

## F3 - Continental Congress

- DOS FUN: **`FUN_3f41_06d0`** (69650, 174 lines) + its Ghidra-split tail
  `FUN_3f41_0ae6` — corrected 2026-09-07 (this doc had `0618`, which is F2;
  the F-key→thunk column, not the code, was the thing that was swapped).
  `06d0` pushes `3` to the plate bring-up (→ `REPORT3.PIK`), calls
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
  blit at y+1; 2026-09-07, was x=6/y=36 with a proportional width), rebel/
  tory two-icon split bar (flags then crowns, 50-slot budget, x=4,y=71),
  expeditionary-force 4-box natural tally (y=102, ~2.2px/unit), 4-column FF
  name grid (x=8, step=78; `FUN_3f41_0ae6` is the Ghidra-split tail of this
  list). Page 2: full-bleed FF group portrait composite, no text.
- Ordering: fixed bells -> sentiment -> force -> FF list; page 2 portraits
  paint from CC-xx.SS sprite anchors in `k_ff_portrait_draw_order[]` (all 25).
- Bells bar (**resolved 2026-09-07, T5.3**; supersedes the 2026-08-30
  "36 bells at a 5px pitch, capped by `max_icons`" reading). The 2×7 mark on
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
- Scroll/paging: 2 pages; any dismiss on page 1 advances to page 2 instead
  of leaving the report; page 2 closes on any click.
- Click targets: none inside a page; page-advance only.
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
- Port status: Done (golden `continental_p1.png`/`continental_p2.png`,
  2026-08-25 per port_plan.md) — `reports_render_congress_page1`/`_page2`.
  The bells bar (and the "1135" plate/number over it) is **pixel-exact**
  since 2026-09-07: 0 differing pixels in x=2..189, y=33..45. Page 2 is 10
  pixels off; page 1 as a whole is 10 369 (was 11 743), of which the
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
  scan — `k_labor_layout[3][9]` skips job 18 (Expert Teacher) and 23
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
  `reports_render_labor_grid`/`_detail` (`reports.c:1340`/`1392`). Real gap
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
- Ordering: page 1 fixed cargo-type order (`k_cargo_names[]`); page 2
  colony order = save's colony array order, filtered to this nation.
- Scroll/paging: 1 + ceil(colony_count/17) pages
  (`reports_economic_page_count`).
- Click targets: none; OK/Esc/Enter advances page, wraps to map from last.
- Strings: title "ECONOMIC ADVISER REPORT" resolves live (`reports_title`).
  **Fixed 2026-08-27**: "Tons"/"Gold"/"Bid Price"/"Ask Price" column
  headers now also resolve live from `LABELS.TXT` `@MISC` (#58/#59/#203/
  #204) via `reports_labels_field`, same as the report titles. Both page
  subtitles also resolve live now: "European Trade" (`@MISC` #206) and
  "Cargo in Port" (`@MISC` #207).
- Port status: Done (golden `economic_p1.png`/`economic_p2.png`) —
  `reports_render_economic_trade`/`_cargo` (`reports.c:1588`/`1700`).

## F6 - Colony Adviser

- DOS FUN: header chrome `FUN_3f41_1b94` (70426) — thunk `FUN_291f_0f04`.
  Body `FUN_3f41_1bec` (70443, 95 lines, "per-colony pop/build/garrison
  rows") — thunk `FUN_291f_0f20`. Panel draw helper `FUN_647e_09da`
  (102793, "draw colony report panel") — thunk `FUN_2a1f_0770`.
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
  to 6 worker-slot icons (x=249, step=21). 9 rows/page, row0 y=27 step=17.
- Ordering: colony array order, filtered to this nation.
- Scroll/paging: 2*ceil(colony_count/9) pages — first half Military
  Garrisons, second half Sons of Liberty (`reports_colony_page_count`).
- Click targets: none; OK/Esc/Enter advances page.
- Strings: title "COLONY ADVISER REPORT" and the two page subtitles
  ("Military Garrisons" `@MISC` #208, "Sons of Liberty" `@MISC` #209) all
  resolve live (**fixed 2026-08-27**, same `reports_labels_field` pattern
  as F5); colony/building names from save.
- Port status: Done (golden `colony_p1.png`/`colony_p2.png`) —
  `reports_render_colony_sidebar`/`_garrisons`/`_sol`
  (`reports.c:1878`/`1912`/`2032`). Row sizing was wrong until
  2026-09-04 (fixed slot cap of 8, fixed pitch 18): a 15-unit New Amsterdam
  in `port_saves/campaign2/COLONY09.SAV` showed 8 icons instead of 14.
  Still unported: DOS reorders the tile stack (`FUN_281f_07ea` ->
  `FUN_1427_04d6`, mode 1) by ship/treasure/defence-class before drawing, so
  icon *order* within a row can differ from the port's unit-pool order. Surfaced two real project-wide bugs
  fixed during porting: magenta nation-color box on report backgrounds
  (`unit_chrome` palette), unrecolored colony-icon flag — see
  `report_screens.md`.

## F7 - Naval Adviser

- DOS FUN: header chrome `FUN_3f41_1e80` (70538) — thunk `FUN_291f_0f58`.
  Body `FUN_3f41_1ed8` (70555, 75 lines, "combat units docked per colony")
  — thunk `FUN_291f_03d4`.
- Background: `REPORT7.PIK`.
- Data source: this nation's ship units (types 0x0d-0x12), each ship's
  cargo hold, any boarded passenger (transport-chain walk), colony/Europe
  location string, destination.
- Columns/layout: 4-column table — Ship (icon+class) / Cargo (goods icons,
  100-unit stacks colored, partial grey) / Location / Destination. Each
  ship is one row; each passenger gets its own row above the ship's row
  (icon+type name only). Row0 y=40 step=20, column dividers x=82/162/242.
- Ordering: save's unit array order for this nation's ships, filtered to
  naval types.
- Scroll/paging: 7 rows/page (`reports_naval_page_count`).
- Click targets: none; OK/Esc/Enter advances page.
- Strings: title "NAVAL ADVISER REPORT" and the 4 column headers ("Ship"/
  "Cargo"/"Location"/"Destination", `@MISC` #61-64, a clean consecutive
  block) all resolve live (**fixed 2026-08-27**); body needs FONTTINY not
  FONTSMAL (FONTSMAL rendered upper-case-only and too wide at this size).
- Port status: Done (golden `naval.png`) — `reports_render_naval`
  (`reports.c:2417`). Two real pre-existing `col1_bridge_apply` bugs found
  and fixed while building this report's row list: Fortified land units at
  a colony dock were being "boarded" onto the docked ship; `cargo_hold[]`
  bytes past `holds_occupied` can be stale, producing phantom cargo —
  fixed to only import the first `holds_occupied` slots. Also:
  `unit_chrome_blit_unit_for_palette`'s nearest-RGB match can't find a
  fully-saturated Dutch orange in REPORT7.PIK's palette; left duller,
  mechanism not identified.

## F8 - Foreign Affairs Advisor

- DOS FUN: `FUN_3f41_2548` (70787, 247 lines, "euro rivals, war, strength")
  — thunk `FUN_291f_03b8`.
- Background: `REPORT8.PIK`.
- **Fully re-read from `FUN_3f41_2548` on 2026-08-31** (raw `.asm`
  3f41:2548..2aca — Ghidra drops this function's pushed values and
  reorders them, so the decompiled C mis-pairs every label with its
  number). Everything below is that read.
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
    clear = at war. This **supersedes** the old report-local
    "`(ab|ba) & 0x02` in either direction" empirical fit, which happened
    to agree on every pair in the golden. `ai_diplo.h`'s `AI_DIPLO_WAR`
    0x01 is still not this byte's DOS decode and is still untouched.
    The Euro-attack `@HAVETREATY` gate reads **the same byte in the same
    direction** — DOS `FUN_465b_0000` tests `FUN_281f_0a38(attacker,
    target) & 0x40` (`viceroy_unpacked.c:75545`) — so this report and that
    prompt can never disagree. bugs.md 388: the port used to OR both
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
- **Jan de Witt detail grid (P2.13, 2026-08-31).** Gated on the *viewing*
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
  98/99/100, exactly as the `.asm` pushes them. `avg_colony_pop` was
  `unknown_ds_944e`; resolved the same pass (see
  [save_format_map.md](save_format_map.md) Stuff row 556).
- Ordering: fixed English->French->Spanish->Dutch, always 4 block slots.
- Scroll/paging: none — no `reports_foreign_page_count` exists (unlike
  Naval/Economic/Colony); 4 fixed blocks always fit.
- Click targets: none.
- Strings: title "FOREIGN AFFAIRS REPORT" (drawn ~3px higher than the
  shared default — a real REPORT8.PIK-specific override, resolves live
  via `reports_title`); "Rebels"/"Tories" **fixed 2026-08-28** — resolve
  live from `@MISC` #86/#87 (the earlier "only singular forms exist" note
  was wrong: the plurals are there, past the 64-line cap that hid them).
  "Peace"/"War" **fixed
  2026-08-27**, now resolve live (`@MISC` #102/#101). "(Withdrawn from New
  World)" **fixed 2026-08-26** — resolves live from `LABELS.TXT` `@MISC`
  index 190 (was cited here as raw line "#205", same line-number-vs-index
  mix-up the title fix corrected elsewhere).
- **Fixed 2026-08-31:** a War pair printed the word "Peace" in the War
  colour. `reports_labels_field` hands back a single `static char[64]`, so
  resolving @MISC 101 and 102 into two `const char*` up front left both
  pointing at "Peace"; both now go through `reports_misc_word`'s
  caller-owned buffer. Moves the golden diff from 3580 to 3372 px.
- Port status: Done (golden `foreign.png`) — `reports_render_foreign`.

## F9 - Indian Adviser

- DOS FUN: `FUN_3f41_010a` (69451, 160 lines, "tribe rows, villages,
  converts") — thunk `FUN_291f_041a`.
- Background: `REPORT9.PIK`.
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
  line: Villages (always shown) / Missions / Muskets / Horse Herds (each
  skipped when 0). Row0 y=28 step=21.
- Ordering: tribe array order, filtered to tribes met by the viewing
  nation.
- Scroll/paging: none in DOS — `FUN_3f41_010a` has an unconditional
  8-iteration loop, no paging logic. Porting in pagination would invent
  behavior DOS doesn't have.
- Click targets: none.
- Strings: title "INDIAN ADVISER REPORT"; tribe names from NAMES.TXT
  @TRIBES col 0 (plural, not the singular/adjective col 1 used elsewhere);
  tribe-level words live from `@LEVELS` (`reports_tribe_level`, 2026-08-26);
  "Missions"/"Horse Herds" from `@MISC`, "Muskets" from `@CARGO`
  (2026-08-28); "Villages" stays hardcoded (no bare-word string shipped).
- Chief portrait (**resolved 2026-09-07, T5.3** — was "variant
  unidentified, always renders #113"). `3f41:0522`..`3f41:05d2`:

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
  Golden proof: `indian.png`'s two rows are Arawak (tribe 2) and Cherokee
  (tribe 4); both have `alarm_by_player[3] == 0` in `dutch-reports.SAV`
  (the earlier "alarm 0 vs 34-48" note was reading other nations' columns),
  so both are q=0. A palette-consistency search over (sprite, x, y) picks
  #113 at x=10, y=25/46 with **0** violations across 226 opaque pixels;
  #114 scores 2, #117 scores 12, and x=11 scores 120+.
- Port status: Done (golden `indian.png`) — `reports_render_indian`. Both
  chief portraits are pixel-exact as of 2026-09-07. Note that `indian.png`
  itself was captured with a slightly different DAC than the other report
  goldens — every pixel of the plate is off by a small RGB delta, so a raw
  RGB diff against it is meaningless (~62k "differences" on a correct
  render). Compare it by palette consistency (does one port colour map to
  exactly one golden colour over the region?), not by equality.

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
- Retire chain (`FUN_41f2_14a8`, `game_retire_after_score`): `@RETIRE`
  confirm → this report → **exploits screen** (`FUN_41f2_0b70`,
  `reports_render_exploits`: WOODPAN2.PIK, GAME.TXT `@EXPLOITS` with
  `%NUMBER0` = rating / `%STRING0` = nation name at y=5, the first tier+1
  `@SCORE` category fields stacked upward from y=195, `SCORE<tier+1>.SS`
  frame 0 — a painting of the tier-th item, SCORE17 = the university —
  blitted at x=100 over the list last, the last row's name field with
  `%STRING0` = leader's surname (DOS `strchr(name,' ')+1`) at y=142;
  skipped when tier < 0 or scoring complete; any key/click continues) →
  Hall of Fame (record inserted first) → title menu. Sheet palette is
  remapped onto WOODPAN2's like ICONS.SS; the sheet-header y DOS uses for
  the picture is unread (placed under the header lines instead).
- Port status: Done (golden `score.png`) — `reports_render_score`/
  `reports_score_collect_citizen_jobs`/`reports_score_draw_citizen_icons`
  (`reports.c:3335`/`3071`/`3294`). Golden's citizen breakdown (142
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
- Columns/layout (`FUN_41f2_0f56` presenter, ported 2026-08-29): title
  `@MISC` #192 centered at y=3; entries from y=0x10, three centered lines
  each at font-height+2 pitch:
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
   match. Note the DAC caveat recorded under F9: capture it the same way the
   F2-F10 goldens were, or a raw RGB diff will be useless.

With that in hand the test is mechanical and should follow the existing
style: extend `tools/render_report_main.c` with a Hall-of-Fame mode (it
cannot reach `reports_render_hall_of_fame` today — that call takes a
`ColonizeHofRow[]`, not a `ColonizeReportId`), load the .DAT through
`game_hof_load`, render, and diff against the PNG.
