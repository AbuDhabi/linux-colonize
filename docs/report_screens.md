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
