# Porting a report screen (F2–F10)

How the Religious Adviser (F2) and Continental Congress (F3) reports were
ported, written up so the remaining ones (Labor F4, Economic F5, Colony F6,
Naval F7, Foreign F8, Indian F9) go faster. Read this before starting one —
several mistakes here cost hours and were entirely avoidable.

Code lives in `src/core/reports.c` / `reports.h`. Rendering is
`reports_render()`, dispatching per `ColonizeReportId` to a
`reports_render_<name>()` function.

## Tools

- `scripts/grid_overlay.sh <image> [step] [out]` — coordinate grid overlay
  for reading off approximate positions by eye.
- `scripts/render_diff.sh <reference> <candidate> [out]` — pixel diff,
  highlights mismatches. Useful as a smoke check but see the "diff noise"
  pitfall below before trusting the mismatch count.
- `build/render_report <data_dir> <save.SAV> <out.ppm> [report_id] [congress_page2]`
  (`tools/render_report_main.c`) — calls `reports_load()`/`reports_render()`
  directly and dumps the framebuffer to a PPM. **Use this, not the live SDL
  app**, for iterating — no xvfb, no window, no manual save-load-navigate
  cycle. `convert out.ppm out.png` to view. Prints the founding-fathers
  bells pool/need to stderr as a side effect (handy for that specific bar).
- `build/sav_json <save.SAV>` — dumps a save to JSON next to it. The fastest
  way to check what a field actually holds for a specific golden, rather
  than guessing from a struct comment (see the founding-fathers pitfall).

Goldens live in `original_saves/report-screen-goldens/`: one `.SAV` plus a
PNG per report, all captured from the *same* save/moment — so a value you
read off one golden (e.g. the bells pool number) should reproduce exactly
from that one `.SAV` file. If it doesn't, that's a real bug, not save drift
— don't shrug off a mismatch as "probably a different save state" (see
the founding-fathers pitfall for a case where that assumption cost real time).

## Golden screenshots are 2x

All golden PNGs are 640×400 — exactly 2x the game's native 320×200. Two
consequences:

1. **Divide every measured coordinate by 2** before putting it in code (the
   framebuffer and all draw calls are native-resolution).
2. **Never measure off a resized copy.** `convert golden.png -resize
   320x200! copy.png` uses a blending filter by default, and blended/shifted
   text can read several pixels off from where it actually is. This
   directly caused a real bug: several Y-coordinates for the Congress report
   were measured wrong early in that session (one by 14px) and the error
   wasn't caught until a much later comparison pass, because renders were
   only eyeballed at low zoom in between — "looks about right" hid a defect
   `render_diff.sh`'s pixel count did also hide (font antialiasing noise
   swamps the real signal — see below). Measure Y/X positions with
   PIL/python directly on the full-resolution golden and divide by 2
   yourself; only resize (with `-filter point`, nearest-neighbor) for a
   *visual* side-by-side, never as the basis for a coordinate.

A working measurement recipe (python + PIL): scan a color/brightness
threshold row-by-row or column-by-column across the *full-res* image to find
where a bar/text band starts and ends, then divide by 2. For text, scanning
for the actual ink color (report text is often a specific yellow/cream, not
plain white) is far more reliable than a generic brightness threshold, which
picks up background wood-grain highlights too.

## Verify identity before trusting a coordinate

The single biggest time-sink this session: measuring a plausible-looking
position is not the same as confirming it's the *right* content. Twice, a
position was accepted because a stacked comparison grid of several crops
"looked about right" at a glance — and was wrong, because a large, busy
element (Franklin's seated portrait, in this case) visually dominates
several nearby crop windows and is easy to misattribute at a quick glance.

The fix that actually worked: crop **one candidate at a time**, at its own
measured size, and look at it individually — never in a multi-row stack
where rows can bleed into each other visually. If it doesn't show what you
expect, the position is wrong; don't rationalize it.

## Font: report titles and some report bodies use FONTTINY, not FONTSMAL

The live game's `font` parameter threaded through `reports_render()` is
`menu_font` (FONTSMAL.FF). Report **titles** are centered and drawn with
FONTTINY.FF instead (`ColonizeReportsView.title_font`, loaded in
`reports_load()`) — confirmed by rendering the same string with every
`COLONIZE/*.FF` font and comparing glyph width/shape against the golden
(`font_text_width()` against a known string is a fast first filter: FONTTINY
gave ~92px for a string the golden measured at ~90px; FONTSMAL gave 135px).

Congress page 1's **body** text also turned out to need FONTTINY, not
FONTSMAL — the golden's 4-column Founding Fathers list only fits in the
78px column width DOS actually uses (see below) with FONTTINY; FONTSMAL
overflowed every column. Don't assume a report's body should use the shared
`font` param just because earlier reports (Religious) got away with it —
check text width against the golden's actual column/line widths first.

## Three distinct progress/tally widget shapes — don't conflate them

All of these are visually "a row of small icons," but they're built
differently and mixing them up gives subtly wrong behavior that only shows
up at extreme values (very low or very high counts), which is exactly what
happened here (a fixed-width crosses bar looked right at one save's data
point purely by coincidence, and was wrong for every other data point until
corrected). `reports_draw_icon_bar()` / `reports_draw_icon_bar_pair()` in
`reports.c` implement all three; reuse them rather than re-deriving:

1. **Proportional fill bar** (Religious crosses, Congress bells): represents
   *progress toward a target*. Full width = a fixed max (screen width minus
   measured margins); *displayed* width = `max_width * current / needed`,
   clamped, then `current` icons are packed evenly into that scaled width
   (`reports_draw_icon_bar`, single icon, width computed by the caller).
   When packing density gets tight enough (icons would overlap to ≤1px
   spacing) the template overlays the count as an outlined number instead —
   this is also used when the caller forces `always_show_number` (Congress's
   force counters always show their number, since packing density there is
   *always* tight by construction). **Bug seen twice**: computing the scaled
   width with plain integer division can truncate to 0 for a nonzero-but-small
   numerator, which makes the whole bar (and its number overlay) vanish even
   though there's a real nonzero value to show. Clamp the computed width to
   at least 1 before drawing.

2. **Two-icon proportional split bar** (Congress rebel/tory flags & crowns):
   represents a 100%-total *split* between two categories, not progress
   toward anything — the bar is always full width. Both icon counts
   (`amount0`, `amount1`) are computed from percentages against a fixed
   "slot budget" (e.g. 50 slots), then packed back-to-back stretched evenly
   across the *same* fixed width (`reports_draw_icon_bar_pair`). No gap by
   construction.

3. **Natural/organic tally** (Congress expeditionary force counters):
   represents a plain count with *no* target and *no* fixed total width —
   each of the 4 boxes is a different, data-dependent width. Width is
   `count * fixed_px_per_unit` (a small tuned constant, ~2.2px here, derived
   by measuring several boxes off the golden and back-solving), and the
   count is packed into that self-determined width with the same evenly-spread
   algorithm. `always_show_number = true` — packing density is always tight
   here, so DOS always overlays the number.

## Shared chrome, not per-report

Title (centered, FONTTINY) and the bottom-right OK button
(`reports_render_ok_button`, `reports_ok_button_hit`) are drawn once in
`reports_render()`'s dispatcher / `reports_render_body_start()`, not
per-report. F10 Score has neither an OK button nor (as it turns out) some of
the assumptions other reports share — check its golden before assuming
parity. A multi-page report (Congress) needs its own page-state plumbed
through `reports_render()`'s signature and the OK-button hit-test, gating
the shared chrome off for pages that don't have it (Congress page 2 is a
full-bleed image with *no* title/text/OK box at all).

## Background PIK ≠ what `k_report_files[]` currently claims

`REPORT1.PIK` and `REPORT3.PIK` sit unused in `COLONIZE/`, orphaned by a
naming-convention assumption (`REPORT<N>.PIK` for F`<N>`) that Congress (F3)
breaks — it was wired to `CCBKGD.PIK` and REPORT3.PIK went unreferenced.
Turned out REPORT3.PIK is Congress *page 1*'s real background; CCBKGD.PIK is
page 2's. If a report looks wrong from the very first pixel (background
itself doesn't match), check for an orphaned `REPORT<N>.PIK` before assuming
the background needs building from scratch — `pik_load` + dump-to-PPM every
unused `REPORT*.PIK` and eyeball them.

A background loaded outside the normal `backgrounds[id]` array (as
Congress page 1's REPORT3.PIK is, via `ColonizeReportsView.congress_page1_bg`)
needs its own palette wired into `game_loop.c`'s big palette-selection
ternary too — the framebuffer is indexed color, and forgetting this means
right *shapes*, wrong *colors*.

## Sprite sheets loaded for a report need their own palette remap

`ICONS.SS` (crosses/bells/flags/crowns/unit icons) is loaded once in
`reports_load()` and remapped to a specific background's palette
(`REPORT2.PIK`'s, since Religious was first) via a nearest-RGB LUT — the
same per-file pattern already used in `colony_screen.c`/`europe.c` (no
shared header, deliberately duplicated). If a new report's icons look
subtly wrong-colored, check they were remapped against *that* report's own
background palette, not an unrelated one. In practice most report PIKs
share the same base ~32-color EGA-derived palette, so this often doesn't
bite — but don't assume it never will.

## Data-field pitfall: don't trust a promising-looking field on sight

`head.founding_father[i]` (`-1` unclaimed, `0..3` = "owning nation" per an
existing code comment) looked like exactly what was needed to filter a
report to "this nation's founding fathers." It isn't reliable for that:
two founding fathers visibly present in a Dutch golden read as nation 2 in
that field, for reasons that were never fully pinned down (founding fathers
turned out not to be nation-exclusive the way that array alone implies).

The actual authoritative source was a *different*, less obvious field:
`nation.founding_fathers[4]`, a per-nation bitmask (bit *i* set = this
nation has FF *i*) — confirmed by dumping the golden's save with
`sav_json` and checking that the Dutch nation's bitmask bits matched the
golden's displayed names exactly, byte for byte. **General lesson**: when a
struct comment and a golden screenshot disagree, the golden wins — go
looking for a different field (`grep` the struct for anything
plausible-sounding, check the JSON dump) rather than assuming the golden is
stale or the save doesn't quite match. It rarely is, and it usually does.

The same wrong field was also feeding a *pre-existing* bug in the Score
report's founding-father count (a fallback chain that tried the broken
field first and only fell through to the correct bitmask when the broken
field returned exactly zero, so it silently undercounted rather than never
firing at all) — worth a quick grep for other reads of a field once you
learn it's unreliable.

## Priority-order bugs in multi-branch value pickers

`founding_fathers_sync_from_col1()` had three branches choosing how to
interpret a nation's `liberty_bells_total` (as a spent-and-reset live pool,
vs. a lifetime cumulative total to subtract past election costs from). The
branches were correct individually, but checked in the wrong order: the
"treat as cumulative, subtract costs already spent" branch ran *first* and
almost always "won" once several founding fathers were elected (the
subtracted sum compounds fast), silently zeroing out a perfectly valid
small live pool that the *later*, correct branch would have used. Symptom
looked exactly like "value works after some gameplay but not on fresh
load" — because turn-by-turn accrual (`founding_fathers_accrue_bells`)
doesn't go through this buggy sync path at all, only the load path does.
**General lesson**: if a computed value is "sometimes right, sometimes
mysteriously zero," check whether an earlier branch in a value-selection
chain is firing when it shouldn't, before suspecting the data itself.

## Portrait sprite compositing (Congress page 2, and any future
multi-sprite scene)

- **Positions must be found per-sprite, individually verified.** Automated
  template matching (`compare -metric RMSE -subimage-search`) was tried and
  is **not reliable** for a cluttered scene where one sprite (again,
  Franklin) is large and shares a lot of background/lighting with the rest
  of the image — it repeatedly converged on the same wrong local minimum for
  *unrelated* sprites. Manual identification (crop a generous region, match
  the distinctive silhouette/color against each sheet's own dump, refine by
  eye) was slower but actually correct. If reusing the automated approach
  for a future report, sanity-check every result with an individual crop
  before trusting it — don't skip straight to a comparison grid.
- **Sprites are photo cutouts with opaque canvas margins, not clean alpha
  mattes.** Blitting them in the wrong order can make an earlier sprite
  vanish entirely under a later one's *background-colored* padding, even
  well outside the later sprite's visible silhouette. Draw back-to-front:
  sort by `(y + height)` ascending so the frontmost/lowest figures paint
  last, over the ones behind them.
- 10 of 25 Congress Founding Father portrait positions are known (whichever
  appeared in the one available golden); the other 15 have no confirmed
  position and are simply not drawn. If a future golden surfaces a
  different combination, extend `k_ff_portrait_slots[]` in `reports.c`
  rather than guessing coordinates.

## `render_diff.sh`'s pixel-mismatch count is noisy for text

Font-stroke antialiasing differences between the golden's original capture
and a freshly-rendered indexed-palette PPM produce a nontrivial mismatched-pixel
count even when every text row is in exactly the right place — this was
mistaken for "still misaligned" at one point in this session before a
direct side-by-side (not a diff) showed the rows actually lined up
perfectly. Use `render_diff.sh` as a coarse smoke check (did something move
by a lot?), but confirm real position questions with a plain side-by-side
image (`convert golden.png render.png -append out.png`) or individual crops,
not the mismatch count alone.

## `unit_chrome_blit_unit` on a report background needs `_colored` + an override

Colony report (F6) was the first report to draw real map-style unit chrome
(orders box + allegiance color, via `unit_chrome_blit_unit`). The orders
box came out solid magenta for a Dutch garrison instead of golden's orange.

Root cause: `unit_chrome_nation_color()`/`unit_chrome_letter_color()`'s
palette indices (`k_european_fill`/`k_european_names`, unit_chrome.c) are
tuned against **`ICONS.SS`'s own native palette** — confirmed by loading
`ICONS.SS` fresh (no remap) and checking: index 13 there really is a
saturated `(255,113,0)` Dutch orange, index 5 a matching `(170,73,0)` for
the Fortify/Fortified letter, both pixel-identical to golden. But
`unit_chrome_draw`'s box fill writes that raw index straight to the
framebuffer — it does *not* go through the sprite-remap LUT
(`reports_remap_sheet_to_palette`) the way sprite pixels do. Report
background palettes (`REPORT*.PIK`) repurpose indices 5/13 back to plain
EGA magenta, so the raw fill renders wrong on report screens even though
it's correct wherever the active palette matches `ICONS.SS`'s native one
closely (apparently map/colony-screen/europe, though there was no golden
on hand to directly confirm those independently).

Fix: added `unit_chrome_blit_unit_colored()` (unit_chrome.c/.h) —
identical to `unit_chrome_blit_unit` plus two trailing
`fill_override`/`letter_override` params (`-1` = default nation-color
computation, unchanged for every existing caller). `reports.c` computes
the override once per report by nearest-RGB-matching `ICONS.SS`-native's
per-nation fill/letter RGB (hardcoded from that one-time probe) against
the *current report background's own palette*
(`reports_colony_chrome_colors` / `reports_nearest_palette_index` —
same nearest-match technique as `reports_remap_sheet_to_palette`'s LUT
build). Any future report that draws `unit_chrome_blit_unit` on a report
background should do the same rather than assume the raw nation-color
index is correct there.

## A save's `orders` byte isn't the last word — check `col1_bridge_apply`'s
## transport-chain boarding pass before trusting `ColonizeUnit.orders`

Colony report garrison icons showed the wrong orders letter (`S` Sentry
instead of the save's real `F` Fortified) for land units standing on a
colony tile that also had a ship docked there. Traced (via a scratch probe
dumping both the raw `ColonizeCol1Unit.orders` and the post-bridge
`ColonizeUnit.orders`/`.aboard_ship_id`) to `col1_bridge_apply`'s "Board
passengers via transport chain" pass: `col1_find_ship_root` walks a raw
unit's `transport_chain` prev/next links looking for *any* sea unit, with
no check that the relationship is actually a passenger manifest — Col1
apparently reuses `transport_chain` for plain same-tile stacking order too,
not just genuine Europe-dock cargo chains. A land unit merely stacked next
to a docked ship gets `units_board_stacked()` called on it, which forces
`orders = 1` (Sentry) and sets `aboard_ship_id`, silently overwriting the
save's real orders.

This is a real, pre-existing `col1_bridge.c` bug reachable outside reports
too (anything that reads `ColonizeUnit.orders`/`.aboard_ship_id` for a
land unit sharing a tile with a docked ship). Not fixed at the source this
session — `col1_find_ship_root` has no obvious cheap discriminator between
a genuine passenger chain and a same-tile stack (both use the same field;
a coordinate-based Europe-only gate would also wrongly stop *real*
ship-hold boarding at a colony dock). The Colony report works around it
locally: it re-derives orders straight from `col1->unit[]` (matched by
x/y/nation/type, one raw record consumed per drawn icon) instead of
trusting the bridged pool's `.orders` for display. Sprite/type selection
is unaffected by the bug and still comes from the pool. Worth a real fix
with a live DOSBox-X trace if this bites another screen.

## Follow-up: the palette fix generalized project-wide, plus a colony-icon flag bug found the same way

The unit-chrome palette fix above wasn't report-only — every screen that
draws `unit_chrome_blit_unit` (map, colony screen, Europe, Colonizopedia
preview, Combat Analysis) had the same magenta-box bug, since none of
their own backgrounds' palettes preserve ICONS.SS-native's index 5/13
either. Generalized: `unit_chrome_blit_unit_for_palette()` (unit_chrome.c)
takes the caller's own active output palette and does the nearest-match
lookup itself; `unit_chrome_blit_unit`/`_colored` are unchanged/still the
raw-index defaults. Every call site across `map_panel.c`, `colony_screen.c`,
`game_loop.c`, `units.c`, `combat_analysis.c` now passes its actual active
palette (`game->map_palette`, `view->frame.palette`, `game->europe.
background.palette`, `game->pedia_wood.palette`) — reports.c's Colony
report switched to the same shared function, dropping its own duplicate.

Same investigation surfaced a second, unrelated palette-coupled bug:
ICONS.SS #0-3's colony settlement markers carry a small baked-in **blue**
flag (native RGB (65,89,166)/(52,73,158), 15 fixed pixels, identical
across all 4 fortification tiers) that DOS recolors to the owning nation
(`FUN_112b_0c64`, the colony-map-chrome decompile, reads the same
`@COUNTRY`/DS:0x848 table `unit_chrome`'s own nation-color constants do —
not independently confirmed beyond that, no live trace available for the
exact per-nation shade formula). The port never recolored it — every
nation's colonies showed the same stored blue. Fixed similarly:
`unit_chrome_nation_flag_shades_for_palette()` derives a light/dark index
pair per nation (light = the same native fill color as the chrome box;
dark = light scaled ~82%, matching the original blue's own two-shade
ratio — not DOS-confirmed, closest reproducible approximation),
`colonies_blit_settlement_icon()` (colony.h) draws the icon then
overpaints those 15 fixed pixels. Every colony-icon draw site (world map,
map info-panel, Colony report both pages) now goes through it instead of
a bare `ss_blit_sprite`.

## Naval report (F7): two real `col1_bridge_apply` bugs found via the ship
## table's Cargo column, plus a font/palette pitfall

Naval is a single paginated 4-column table (Ship / Cargo / Location /
Destination), 7 rows/page (golden: naval.png). Geometry: row rules at
native y = 40, 60, … 180 (`REPORTS_NAVAL_ROW0_Y`/`_ROW_STEP`), column rules
at native x = 82/162/242, all measured the usual way (full-res golden,
divide by 2). Each ship is one row (icon + class name, goods icons packed
left-to-right in its own Cargo cell); each passenger aboard gets its own
row *above* the ship's row (unit_chrome icon + unit type name, no ship
info) — confirmed by the golden's one example, a Caravel carrying a
Colonist passenger plus a full (100-unit, colored not grey) Trade Goods
stack.

Building the row list surfaced two genuine, reachable-outside-reports
`col1_bridge_apply` bugs, both caught only because the golden's ships
happened to exercise them:

- **Fortified land units at a colony dock get "boarded" onto the docked
  ship.** `col1_find_ship_root`'s transport-chain walk (already known to
  reuse same-tile stacking order, not just genuine manifests — see the
  Colony report pitfall above) had no discriminator at all — every
  same-tile land unit got boarded regardless of its own orders. The
  golden's Privateer, docked at New Amsterdam, was picking up a Fortified
  Dragoon and a Fortified Artillery as phantom passengers. Fix: skip a
  unit whose raw `orders` is Fortify/Fortified before walking the chain —
  a unit fortified in place is definitionally not aboard a ship (confirmed
  against the same save's one genuine passenger, a Sentry-orders Colonist
  — never Fortified). Doesn't catch every theoretical false positive (a
  Sentried land unit merely standing at the dock would still slip
  through), but it's real, low-risk, and fixes the concrete case.
- **`cargo_hold[]`/`cargo_item_*[]` past `holds_occupied` can be stale.**
  The Merchantman and Privateer both showed phantom goods icons (Furs,
  Tools) that don't exist in the golden. Both have `holds_occupied == 0`
  in the raw save, yet their `cargo_hold[]` arrays still carried old
  nonzero bytes — `docs/savegame.md` already documented "`holds_occupied`
  = goods only" but the goods-import loop never gated on it, just checked
  each of the 6 slots for `0 < amt < 255` independently. DOS apparently
  never clears the trailing array bytes on unload, only the occupied
  count. Fix: only import the first `holds_occupied` slots (clamped to
  `COLONIZE_UNIT_CARGO_MAX`).

Two more pitfalls specific to this report:

- **Body text needs FONTTINY, not FONTSMAL** — same family of mistake as
  Congress page 1's body (see above), rediscovered independently here:
  the ship/cargo/location text rendered in FONTSMAL came out both
  noticeably wider than the golden's ~2px/char text *and* upper-case only
  (FONTSMAL apparently has no distinct lower-case glyphs at this size, or
  at least renders as if it doesn't) even though the underlying strings
  (`NAMES.TXT` unit names) are stored mixed-case. Switching to
  `view->title_font` fixed both symptoms at once. Worth checking width
  *and* case against the golden before assuming a report body's font,
  not just width.
- **`unit_chrome_blit_unit_for_palette`'s nearest-match can't always find
  a good orange.** REPORT7.PIK's 256-color palette has no entry within
  useful distance of ICONS.SS-native's saturated Dutch orange
  `(255,113,0)` — nearest is `(203,105,48)` (squared RGB distance 5072),
  a visibly duller shade, confirmed by direct palette probe. The golden
  screenshot still shows the pure saturated orange, meaning DOS's real
  per-report palette for chrome elements isn't simply "nearest-match into
  this PIK's own stored 256 colors" the way every other report's chrome
  has matched close enough to look right — there's some other mechanism
  (reserved/dynamic palette slots for chrome, most likely) not identified
  this session. Left as the nearest-match result (recognizably orange,
  right position/letter, just duller) rather than guessing at a palette
  architecture change — flagging here in case a future report hits the
  same gap harder (e.g. a background with *no* orange-family color at
  all) and it's worth resolving properly with a live DOSBox-X palette
  trace.

## Foreign Affairs report (F8): a real DOS byte disagreeing with this
## port's own AI abstraction of the same field, plus a population field
## that isn't colony population

Foreign (golden: foreign.png) is unpaginated: one fixed block per Euro
nation, always English/French/Spanish/Dutch order
(`reports_foreign_build_rows`, `COLONIZE_COL1_NATION_COUNT` rows, no page
param — every nation always fits in the 4 fixed block slots, so unlike
Naval/Economic/Colony there's no `reports_foreign_page_count`). Each block:
a header rule, `"<Leader>'s <Adjective>:"` (leader name yellow idx146,
adjective cream idx97 — two `reports_draw_line` calls split at the leader
segment's measured width), then either a centered `"(Withdrawn from New
World)"` (`player[n].control == 2`) or a 2-column grid of `"<peer
country>: Peace|War"` for every *other* non-withdrawn nation (own nation
and withdrawn peers are skipped entirely — confirmed against the golden,
where Spain's withdrawal removes it from every other block's relation
list, not just its own), then a `"Rebels: N   Tories: N"` line. Geometry:
block rule at native y = 10, 55, 100, 145 (`REPORTS_FOREIGN_BLOCK0_Y`/
`_BLOCK_STEP`), body text at a uniform 7px (native) line pitch
(`REPORTS_FOREIGN_LINE_STEP`), two columns at native x = 2, 80. All
measured the usual way (full-res golden, divide by 2) — see the header
"Golden screenshots are 2x" pitfall, which bit here too: an early column-x
reading taken from a crop limited to the image's left half accidentally
still worked (0–320 happened to contain both columns), but only by luck —
re-measure over the *full* image width, not an arbitrary crop, or a
column past the crop boundary silently reads as missing.

Colors are report-background-specific, not reused blind from Naval/
Economic's index numbers — REPORT8.PIK's palette layout differs from
REPORT7's at several indices. Probed directly (`pik_load` + nearest-RGB
against the golden's sampled ink colors, same technique as the ICONS.SS
remap): leader yellow `(255,243,93)` → idx146, adjective cream
`(247,243,199)` → idx97 (same index Naval uses — this one *does* carry
over), peer/Rebels/Tories label yellow `(255,255,142)` → idx145 (a
*different*, slightly paler yellow than the header's — confirmed as two
genuinely distinct colors by sampling both, not an antialiasing artifact),
Peace white → idx15, War red `(255,0,0)` → idx112 (nearest stored color is
`(243,0,0)`, same index Economic's negative-value red already uses), rule
dark red `(134,0,0)` → idx119 (same index Naval/Economic use). Don't
assume a shared index number means a shared color across two different
`REPORT*.PIK`s — probe each report's own background palette.

Two data-semantics findings, both from cross-referencing dutch-reports.SAV
against foreign.png rather than trusting a plausible-looking existing
field (see the "don't trust a promising-looking field on sight" pitfall
above — this report produced two more instances of exactly that):

- **War/Peace can't be read via `ai_diplo.h`'s `AI_DIPLO_WAR`/`_PEACE` bit
  constants at face value.** Those bits (`0x01`/`0x02` on
  `nation[a].euro_relation[b]`) are documented as DOS-genuine in
  `original_sources_annotated/ai/euro_diplo.md`, and *are* what this
  port's own `ai_diplo_*` calls write during live AI turn processing — but
  applied directly to dutch-reports.SAV's raw bytes, `AI_DIPLO_WAR`
  (`0x01`) never appears set anywhere in the save, including the golden's
  one confirmed War pair (French/Dutch). Decoded by hand instead: bit
  `0x02` being set in *either* direction's byte
  (`nation[1].euro_relation[3] == 0x22` has it; the reverse
  `nation[3].euro_relation[1] == 0x20` doesn't) exactly identifies every
  War pair the golden shows and excludes every Peace pair. Implemented as
  a report-local `reports_foreign_at_war()`, deliberately **not** fed back
  into `ai_diplo.h`'s shared constants — that module drives live AI
  simulation behavior project-wide, and this is a single-save empirical
  fit (the byte's real DOS semantics may be a small state value rather
  than a clean bitmask; not worth guessing further without a live
  DOSBox-X trace). **General lesson**: a shared module's documented bit
  meaning for a field, even one cited as DOS-genuine from a decompile, is
  not automatically the right decode for that same field's raw bytes in
  an actual captured save — when the two disagree, re-derive from the
  golden and keep the finding local rather than editing the shared
  module's semantics on a one-save sample.
- **Rebels/Tories total isn't colony population.** `col1->colony[].
  population` summed per nation undercounts — e.g. the English total in
  dutch-reports.SAV is 41 by that sum, but golden shows Rebels:21 +
  Tories:54 = 75. The gap is field colonist-type units (Soldiers,
  Dragoons, etc. — anyone who's fundamentally a colonist even off in a
  colony's garrison or roaming) that colony population doesn't count.
  `col1->stuff.census_pop_proxy[nation]` (DS:0x9410, "+1 skilled unit + Σ
  colony pop" per its `col1_save.h` comment) is the real total — confirmed
  exact for all three surviving nations (75/45/54), with
  `rebels = floor(total * rebel_sentiment / 100)`, `tories = total -
  rebels` reproducing the golden's numbers exactly (21/54, 24/21, 50/4).
  Worth knowing this field's own caveat before reusing it elsewhere:
  `col1_save.h` documents it as RMW-preserved from the loaded save, not
  recomputed by this port during live Linux-side play (only DOS's own
  turn processing keeps it fresh) — fine for reading a loaded save (this
  report's only supported path — same "content prefers `ColonizeCol1Save`"
  convention as every other report), but it would read stale if a future
  caller needed it to reflect population changes since the save was
  loaded without a full reload.

## Colonization Score report (F10): no shared chrome, own palette ink, and
## a citizens rule that only shows up by cross-checking the golden's save data

F10 is structurally unlike every other report — no OK button, no per-report
`REPORT<N>.PIK` (it's the full-screen `WOODPANL.PIK` wood panel also used by
the title-menu Hall of Fame), and its own hand-placed layout with large
blank stretches of wood rather than the shared row/step grid. Golden:
score.png, from `dutch-reports.SAV`. Content:

- Title (shared chrome, FONTTINY, centered) + a second centered line, the
  subtitle: `"<difficulty rank> <leader name> of the <nation adjective>:
  <Season> <year>"` (golden: `"Viceroy Michiel De Ruyter of the Dutch:
  Autumn 1630"` — difficulty rank reuses the existing Discoverer..Viceroy
  names, leader name is `col1->player[human].name`).
- `"<Nation> Citizens:  +<N>"` (native x=16, y=24) then a packed strip of
  small colonist-portrait icons, one per counted citizen, at y=32 spanning
  roughly x=16..304 (`REPORTS_SCORE_ICON_*`). Icons are
  `units_job_icon_sprite(job)` — the same table the Labor report's grid
  uses — packed with the same evenly-spread-across-a-fixed-width math as
  `reports_draw_icon_bar`, generalized to a heterogeneous per-slot icon
  list (`reports_score_draw_citizen_icons`) since every citizen can be a
  different job/portrait.
- `"<Nation> Continental Congress:  +<N>"` (y=60) then a 4-column,
  row-major grid of this nation's Founding Father names (columns at native
  x=16/88/160/232, rows at y=67/74/81, step 7) — `reports_ff_owned_by_
  nation` bitmask, iterated in FF-table order (already existing
  infrastructure from the Congress report, just reused here).
- A large blank gap, then three lines near the bottom: `"Gold:  (<gold>g)
  +<N>"`, `"Rebel Sentiment:  +<N>"`, `"Total Score: <N>"` (y=150/157/164).
  Golden shows *only* these three plus Citizens/Congress above — no
  Villages Burned, Intervention Bells, or Independence breakdown lines,
  even though `reports_compute_score` still folds all of those into the
  total internally. Left undisplayed rather than guessed at, since this
  golden's save has zero villages burned and independence undeclared —
  nothing in score.png suggests those rows exist at all on this screen;
  they may be DOS-invisible always, or only appear post-WoI. Revisit if a
  golden from a later-game save ever surfaces.
- A plain two-tone progress bar at the very bottom (native x=35..285,
  y=186, height 7 — no icons, not `reports_draw_icon_bar`, just a filled
  rectangle): dark track (`60,32,24`, WOODPANL palette index 138) with a
  green fill (`85,150,52`, index 68) proportional to `total score / 1000`
  clamped to the track width — measured 76/250px (30.4%) fill against a
  305 total (30.5%), close enough to confirm the 1000-point nominal max.
  What (if anything) 1000 means to DOS isn't confirmed beyond this one
  data point.

Ink colors are **not** the usual report palette indices 14/15/97/etc. —
`WOODPANL.PIK`'s own palette leaves index 15 as literal EGA white, unlike
every `REPORT<N>.PIK` which remaps it to a report-specific gold. Score's
title/subtitle/Total Score ink is index 149 (`199,162,32`); every other
line (Citizens/Congress/FF names/Gold/Rebel Sentiment) is index 68
(`85,150,52`, the same green as the bottom bar's fill) — both confirmed by
loading `WOODPANL.PIK` standalone and nearest-matching the golden's sampled
ink RGB (both came back an exact `dist2=0` match, i.e. these colors are
genuinely *in* the palette, not approximated). `reports_render_body_start`'s
shared title draw now branches on `id == COLONIZE_REPORT_SCORE` for this;
every other report keeps its existing index-15 draw unchanged.

**The real bug worth remembering**: the citizens formula. The manual/FAQ
rule (`+1` indentured servant/petty criminal, `+2` free colonist/Indian
convert, `+4` skilled, already correctly coded) undercounts against the
golden if you count colony population alone (142 vs. the golden's 158),
and *overcounts* if you also add every scored-colonist-type map/Europe
unit via the same type-based job fallback colonies use (180). The correct
rule, reverse-engineered by testing both hypotheses against
`dutch-reports.SAV`'s actual unit records: colony population always counts
(an occupied colony slot is unconditionally a person, so an invalid/
sentinel profession byte still falls back to Free Colonist) — but a
map/Europe unit only counts when its own raw `profession` byte is a
genuine assigned job (0..27); the common sentinel value (28, "no expert
skill") means *no* citizen credit at all for that unit, full stop, no
type-based fallback. Exactly 4 of the golden's 9 land/Europe units had a
real profession byte, contributing exactly the missing 16 points (142+16=
158). `reports_score_collect_citizen_jobs` implements this and is shared
between `reports_compute_score`'s point total and the citizens icon strip,
so both always agree. This flipped one existing unit test's expected
numbers (`COLONY01`'s lone Pioneer, profession byte 28, no longer scores)
— a reminder that a pre-existing test's asserted values aren't necessarily
DOS-verified truth, only "what the code did when someone wrote the test."

Also fixed while re-deriving Citizens: `reports_rebel_sentiment_pct` used
to recompute a population-weighted average from each colony's `rebel_
dividend`/`rebel_divisor` (91 on the golden) instead of reading `nation.
rebel_sentiment` (nation+0x19) directly — the DOS-maintained value the
report actually shows (94, exact match). Same function name kept, body
swapped to a direct field read.

## Indian Adviser report (F9): a genuinely undocumented DOS formula found by
## reading the real decompile, not by guessing at scale factors

Indian is a flat, unpaginated list (golden: indian.png) — one two-line block
per tribe the viewing nation has met (`indian.euro_diplo[human] != 0`, bit
0x20 met / 0x40 peace), 21px/row from native y=28
(`REPORTS_INDIAN_ROW0_Y`/`_ROW_STEP`), built once by
`reports_indian_build_rows()` and drawn by `reports_render_indian()`. No
page-count function exists for this report (unlike every other list-style
report here) — DOS's own decompiled `FUN_3f41_010a` has no paging logic at
all, just an unconditional 8-iteration loop, so a save with every tribe
contacted would in principle run under DOS's own OK button too; porting in
pagination anyway would be inventing behavior DOS doesn't have.

Each block: a 16x16 headband portrait (`ICONS.SS` #113) + `"<PluralTribeName>:"`
colored per-tribe, right-aligned tribe level (Semi-Nomadic/Agrarian/Advanced/
Civilized) same color, then a black fixed-column stats line — Villages
(always shown) / Missions / Muskets / Horse Herds (each skipped when 0).

**The real find this report needed**: golden's Muskets numbers (150, 700)
don't match `ColonizeCol1Indian.muskets` directly (0, 5) by any obvious
scale factor — tempting to assume the field is wrong or needs a fudge
constant. It isn't wrong. The DOS decompile for this *exact* screen exists
(`FUN_3f41_010a`, `original_sources_decompiled/viceroy_unpacked.c:69451`,
already catalogued in `original_sources_annotated/ai/settlement_record_8d4a.md`
as "count-by-owner" but never actually read line-by-line before this
session) and spells out the formula: `local_6c` starts at `indian.muskets`,
then adds one for every unit this tribe owns whose type is Armed Brave (20)
or Mtd. Warrior (22) — the two musket-equipped native unit types
(`docs/indians.md`'s `@UNIT` table) — then multiplies the total by 50.
Confirmed exact against `dutch-reports.SAV`: Arawak (muskets=0 + 3 Mtd.
Warriors)×50 = 150; Cherokee (muskets=5 + 7 Armed Braves + 2 Mtd.
Warriors)×50 = 700. Both match the golden's printed numbers to the digit.
Horse Herds, by contrast, *is* `indian.horse_herds` read raw with no
formula at all (5 and 6, matching directly) — the two fields look
symmetric in the struct but aren't symmetric in what the report does with
them. **General lesson, sharper than the usual "check the golden save's
JSON dump" advice**: when a plausible-looking field doesn't reproduce a
golden's number by any obvious arithmetic, check whether the exact DOS
function for that screen has already been decompiled
(`original_sources_annotated/FUNCTION_CATALOG.md`, search for the report's
name) before spending time guessing at scale factors — the real formula is
often sitting there, already found and catalogued, just never actually
read.

The Missions count has the same "read the real function" flavor: it's not
"villages with any mission" (what a first guess, and this report's old
placeholder implementation, both assumed) but `local_58` — villages whose
`mission` byte's low nibble equals the *viewing* nation
(`COL1_TRIBE_MISSION_NATION_MASK`), i.e. missions *this specific European
power* has established, not any power's. The two happened to read identical
for Arawak in the one golden available (every mission there happens to be
Dutch), which would have hidden the bug indefinitely without the decompile
read.

Two more identifications from the same pass, both cheap to confirm once
suspected: the plural tribe-name strings ("Arawaks:", not "Arawak:") come
from `NAMES.TXT` `@TRIBES` column 0, not column 1 (the singular/adjective
form some other code in this file already used) — golden text is
`"<name>:"` with a trailing colon, easy to misread as a comma at FONTTINY
size (measure it, don't guess). And the per-tribe name/level color
(`k_indian_tribe_colors`, duplicated from `unit_chrome.c`'s `k_tribe_colors`
per this project's established no-shared-header convention) turned out to
need *no* palette remap at all against `REPORT9.PIK` — a direct
nearest-RGB probe against that PIK's own palette landed at squared distance
**0** for both golden tribes' colors, unlike the ICONS.SS sprite pixels
(which do go through the usual remap) or Naval's Dutch-orange chrome (which
famously doesn't land exactly). Confirm with a probe before assuming either
way; it varies report to report.

**Left unresolved**: DOS's headband portrait actually has five near-identical
variants (`ICONS.SS` #113-117), and the decompile computes a 0-4 index
(alarm-derived, via a still-unidentified `0x281f`-segment helper — *not*
the unrelated `FUN_521d_0a60` euro-goal-planning function of confusingly
similar name/suffix) to presumably pick between them. Both tribes in the
one available golden — one at alarm 0, one at alarm 34-48 toward the
viewing nation — render pixel-identical portrait #113, so this port always
uses #113. A future golden showing a visibly different portrait would be
needed to pin down the actual index formula.
