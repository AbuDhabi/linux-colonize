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

`port_plan.md`'s P2 intro text (reports render as "flat text lines"; two
reports have an unwired click-to-zoom) does not match this tree. All nine
F2-F10 plates already have golden-derived pixel layouts, real
`REPORT<N>.PIK` backgrounds, correct fonts/palettes, and a matching golden
PNG in `original_saves/report-screen-goldens/` (religious.png,
continental_p1/p2.png, labor.png/labor_detail.png, economic_p1/p2.png,
colony_p1/p2.png, naval.png, foreign.png, indian.png, score.png).
`report_screens.md` has dedicated write-ups for F2/F3/F6/F7/F8/F9/F10; F4/F5
carry the same golden-measured constants in code without a prose section.
`docs/assets.md` line ~329 ("only F2/F3 done, rest placeholder") is also out
of date for the same reason.

Only one click-to-zoom exists anywhere in the DOS decompile: Labor (F4)'s
profession grid to its own per-profession detail page
(`FUN_3f41_10d8` -> `FUN_3f41_0d3e`, confirmed by the catalog's "click->0d3e"
note). Grepping `reports.c` for `mouse|click|hit` turns up only Labor's hit
test. There is no DOS mechanic for clicking a report row to jump to the
colony/map screen or center a unit — Colony/Naval/Foreign/Economic/Indian
are read-only paginated tables in the decompile.

Open gaps: column headers/body strings in `reports.c` are hardcoded English
strings matched by eye against goldens, not resolved live from
`LABELS.TXT`/`GAME.TXT` (unlike popup bodies, `popup_string_resolver.md`) —
content is correct today but not string-table-driven. **Screen titles are
the exception, fixed 2026-08-26**: `reports_title` now resolves live from
`LABELS.TXT` `@MISC` (all 9 titles are real shipped strings there — an
earlier pass's "not shipped as text anywhere" conclusion was a spelling
mismatch in its own search, not a real absence; see `port_plan.md` P2.2).
Hall of Fame has no
golden screenshot at all, unlike every F2-F10 report, so its layout is
unconfirmed against DOS. Congress page 2's FF portrait slot table has only
10 of 25 positions confirmed. F9's headband-portrait variant selection
(ICONS.SS #113-117) is unidentified, always renders #113.

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

- DOS FUN: `FUN_3f41_06d0` (viceroy_unpacked.c:69650, 174 lines) — thunk
  `FUN_291f_03fe`.
- Background: `REPORT2.PIK`.
- Data source: nation crosses pool (needed/accumulated split), founding-
  father bitmask for the FF-name tail loop (0x25-entry table at `-0x69ae`),
  immigration/recruit-pool counts (4-slot arrays at `0x53da`/`0x53e2`).
- Columns/layout: single column — title, crosses proportional fill bar
  (native x=10,y=27, ICONS.SS#56), two conditional summary lines (recruit
  pool / immigrants en route — suppressed entirely when zero, not shown as
  "0"), FF-name tail list wrapping across 4 columns.
- Ordering: crosses bar always first; summary lines conditional.
- Scroll/paging: none.
- Click targets: none (shared OK button only).
- Strings: title "RELIGIOUS ADVISER REPORT" resolves live (`reports_title`);
  FF names live from `NAMES.TXT @FATHERS` (`reports_ff_name`, `k_ff_names[]`
  is the no-assets fallback).
- Port status: Done (golden `religious.png`) —
  `reports_render_religious` (`reports.c:873`).

## F3 - Continental Congress

- DOS FUN: page shell `FUN_3f41_0618` (69611, 39 lines) — thunk
  `FUN_291f_040c`. FF debate/nominate is a separate DOS screen,
  `FUN_4345_06d2` (73177) via `FUN_2a1f_0000`/`FUN_291f_0f74` — not part of
  the F3 plate itself.
- Background: page 1 `REPORT3.PIK` (own desk/study — was orphaned in
  `k_report_files[]` until fixed during porting), page 2 `CCBKGD.PIK` (hall
  photo, full-bleed, no chrome).
- Data source: `nation.liberty_bells_total` / next-FF threshold
  (`FUN_4345_0982`), `nation.rebel_sentiment`/tory split, expeditionary-
  force pool counts (Regulars/Cavalry/Artillery/Man-O-War), FF-owned
  bitmask for the name list (`FUN_4345_01a6`).
- Columns/layout: page 1 — bells proportional fill bar (x=6,y=36), rebel/
  tory two-icon split bar (flags then crowns, 50-slot budget, x=4,y=71),
  expeditionary-force 4-box natural tally (y=102, ~2.2px/unit), 4-column FF
  name grid (x=8, step=78; `FUN_3f41_0ae6` is the Ghidra-split tail of this
  list). Page 2: full-bleed FF group portrait composite, no text.
- Ordering: fixed bells -> sentiment -> force -> FF list; page 2 portrait
  positions (`k_ff_portrait_slots[]`) only 10/25 confirmed.
- Scroll/paging: 2 pages; any dismiss on page 1 advances to page 2 instead
  of leaving the report; page 2 closes on any click.
- Click targets: none inside a page; page-advance only.
- Strings: title "CONTINENTAL CONGRESS ACTIVITIES" resolves live
  (`reports_title`); body lines hardcoded
  English, need FONTTINY not FONTSMAL.
- Port status: Done (golden `continental_p1.png`/`continental_p2.png`,
  2026-08-25 per roadmap.md) — `reports_render_congress_page1`/`_page2`
  (`reports.c:970`/`1141`).

## F4 - Labor Adviser

- DOS FUN: grid `FUN_3f41_10d8` (70058, 123 lines, "profession grid;
  click->0d3e") — thunk `FUN_291f_03f0`. Detail `FUN_3f41_0d3e` (69914, 144
  lines, "one profession's colony placements") — thunk `FUN_291f_0f3c`.
- Background: `REPORT4.PIK`.
- Data source: colonist `profession` byte, summed three ways: colony
  population slots ("In Colonies"), map units of colonist-derived types
  0-5 (Colonists/Soldiers/Pioneers/Missionaries/Dragoons/Scouts) not on
  Europe/own-colony tiles ("On Mapboard"), same types on a Europe-side tile
  ("Off Mapboard/Europe").
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
  `reports_job_name()`; "(Click on item to zoom)" hardcoded, matches golden.
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
  Garrisons pages show up to 6 garrison icons (x=110, step=18). Sons of
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
  (`reports.c:1878`/`1912`/`2032`). Surfaced two real project-wide bugs
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
- Data source: per-nation block, fixed English/French/Spanish/Dutch order —
  `player[n].control` (2=withdrawn); war/peace read via
  `nation[a].euro_relation[b]` bit 0x02 in either direction (empirically
  fit against a golden — `ai_diplo.h`'s documented `AI_DIPLO_WAR` bit 0x01
  does not reproduce the golden's War pairs; kept report-local, not fed
  back into the shared AI module); `col1->stuff.census_pop_proxy[nation]`
  (DS:0x9410) for Rebels/Tories, not summed colony `.population` (which
  undercounts by every field colonist-type unit).
- Columns/layout: one fixed block per nation, always 4 slots (own nation
  and withdrawn peers skipped inside a block, not compacted) — header rule,
  "<Leader>'s <Adjective>:", then either centered "(Withdrawn from New
  World)" or a 2-column peer-relation grid ("<peer>: Peace|War", columns
  x=2/80), then "Rebels: N   Tories: N". Block rule y=10/55/100/145, 7px
  line pitch.
- Ordering: fixed English->French->Spanish->Dutch, always 4 block slots.
- Scroll/paging: none — no `reports_foreign_page_count` exists (unlike
  Naval/Economic/Colony); 4 fixed blocks always fit.
- Click targets: none.
- Strings: title "FOREIGN AFFAIRS REPORT" (drawn ~3px higher than the
  shared default — a real REPORT8.PIK-specific override, resolves live
  via `reports_title`); "Rebels:"/"Tories:" hardcoded (no bare-word match
  in `LABELS.TXT` — only "Rebel"/"Tory" singular/adjective forms exist,
  wrong grammatical shape for these labels). "Peace"/"War" **fixed
  2026-08-27**, now resolve live (`@MISC` #102/#101). "(Withdrawn from New
  World)" **fixed 2026-08-26** — resolves live from `LABELS.TXT` `@MISC`
  index 190 (was cited here as raw line "#205", same line-number-vs-index
  mix-up the title fix corrected elsewhere).
- Port status: Done (golden `foreign.png`) — `reports_render_foreign`
  (`reports.c:2653`).

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
  headband portrait (ICONS.SS #113, always) + "<PluralTribeName>:"
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
  tribe-level words hardcoded.
- Port status: Done (golden `indian.png`) — `reports_render_indian`
  (`reports.c:2874`). The muskets x50 formula and mission-nation filter
  were both real, previously-undocumented DOS behavior found only by
  reading the decompile line-by-line.

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
- Data source: subtitle = difficulty rank + `player[human].name` + nation
  adjective + season/year. Citizens: colony population always counts
  (sentinel profession falls back to Free Colonist); a map/Europe unit
  counts only when its raw `profession` byte is a genuine assigned job
  (0..27) — sentinel 28 contributes zero, no type-based fallback (found by
  testing both under- and over-counting hypotheses against a golden).
  Congress: `reports_ff_owned_by_nation` bitmask. Gold, Rebel Sentiment
  (`nation.rebel_sentiment` directly, same fix as F8), Total Score =
  `reports_compute_score`'s full breakdown.
- Columns/layout: single-column hand-placed layout, not the shared
  row/step grid — subtitle (y=12), "<Nation> Citizens: +N" (y=24) +
  wrapping citizen-portrait icon strip (y=32, 8px pitch, wraps at 37
  icons/row with alternating half-icon row offset), "<Nation> Continental
  Congress: +N" (y=60) + 4-col x 3-row FF name grid (x=16/88/160/232,
  y=67 step=7), large blank gap, then "Gold: (Ng) +N" / "Rebel Sentiment:
  +N" / "Total Score: N" (y=150/157/164), then a plain two-tone fill-rect
  progress bar (x=35..285, y=186, fill=min(total,1000)/1000).
- Ordering: fixed layout, no rows to order.
- Scroll/paging: none.
- Click targets: none — and no OK button at all (unlike every other
  report); dismisses on click anywhere, same as Congress page 2.
- Strings: title/subtitle/Total Score use ink index 149, everything else
  index 68 (WOODPANL.PIK-specific, not the usual report-plate 14/15/97).
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
  ranked from a text file (`game_hof_path`/`_load`/`_save`/`_insert`)
  analogous to DOS's `HALLFAME.DAT`; entries also appended on Retire
  (`reports_compute_score` -> `game_hof_insert`/`_save`, `game_loop.c:7651`).
- Columns/layout: title "COLONIZATION HALL OF FAME" (`LABELS.TXT` `@MISC`
  index 192 — the "#207" this doc used to cite was always a raw grep line
  number, not an `@MISC` index; corrected 2026-08-26), "Esc / Enter returns
  to menu", header row "Leader / Nation / Score / A.D.", up to
  `COLONIZE_HOF_ROW_MAX` (10) ranked rows,
  `"%2d.  %-24s %-10s %6d  %d"`.
- Ordering: rank descending by score (`game_hof_insert`).
- Scroll/paging: none — hard-capped at 10 rows.
- Click targets: none.
- Strings: **fixed 2026-08-26** — title and 3 of the header row's 4 words
  ("Leader" #197, "Score" #198, "A.D." #194) now resolve live from
  `LABELS.TXT` `@MISC` (`reports_render_hall_of_fame`, same
  `reports_labels_field` helper P2.2's title fix added), falling back to
  the old literal text when assets aren't loaded — byte-identical output
  today since the live and static text already matched. Only "Nation"
  (no match anywhere in `LABELS.TXT`) and "Esc / Enter returns to menu"
  stay hardcoded — genuinely unconfirmed against DOS.
- Port status: Done thin — functions and persists correctly, but no golden
  screenshot exists for this screen (unlike every F2-F10 report), so exact
  DOS column widths/positions/chrome are unconfirmed.
  `reports_render_hall_of_fame` (`reports.c:3483`).
</content>
</invoke>
