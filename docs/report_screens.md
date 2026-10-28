# Porting a report screen (F2–F10)

Method + pitfalls from porting the F2–F10 reports, written so future screen
passes go faster. Read this before starting one. Code:
`src/core/reports.c`/`.h`; `reports_render_w()` dispatches per
`ColonizeReportId`.

## Tools

- `scripts/grid_overlay.sh <image> [step] [out]` — coordinate grid overlay.
- `scripts/render_diff.sh <reference> <candidate> [out]` — pixel diff;
  smoke check only (see "diff noise" below).
- `build/render_report <data_dir> <save.SAV> <out.ppm> [report_id] [congress_page2]`
  — calls `reports_load()`/`reports_render_w()` directly, no SDL/xvfb. **Use
  this, not the live app**, for iterating. `convert out.ppm out.png` to
  view. Prints FF bells pool/need to stderr.
- `build/sav_json <save.SAV>` — JSON dump; the fastest way to check what a
  field actually holds for a golden.

Goldens: `original_saves/report-screen-goldens/` — one `.SAV` + PNG per
report, all captured from the same moment. A value read off a golden must
reproduce exactly from that `.SAV`; a mismatch is a real bug, never "save
drift".

## Golden screenshots are 2x

All golden PNGs are 640×400 = 2x native 320×200.

1. **Divide every measured coordinate by 2.**
2. **Never measure off a resized copy** — `convert -resize` blends and
   shifts text several pixels (caused a real 14px Y error that survived
   multiple eyeball passes). Measure with PIL/python on the full-res golden;
   resize only with `-filter point` for visual side-by-sides.

Measurement recipe: threshold-scan rows/columns on the full-res image for
the actual ink color (report text is often a specific yellow/cream, not
white — a generic brightness threshold picks up wood-grain highlights),
then divide by 2.

## Verify identity before trusting a coordinate

A plausible position is not confirmed content. Crop **one candidate at a
time**, at its measured size, and look at it individually — never in a
stacked comparison grid (a large busy element like Franklin's portrait
dominates several nearby crop windows and gets misattributed). If the crop
doesn't show what you expect, the position is wrong; don't rationalize.

## Fonts: titles and some bodies use FONTTINY, not FONTSMAL

Titles: FONTTINY.FF (`ColonizeReportsView.title_font`). Verify a font by
`font_text_width()` against a golden-measured string width. Congress page-1
body and Naval's body also need FONTTINY (FONTSMAL is wider AND renders
upper-case-only at this size) — check width *and* case against the golden
before assuming a body font.

## Three distinct progress/tally widget shapes — don't conflate

All look like "a row of small icons"; mixing them up only shows at extreme
counts. `reports_draw_icon_bar(_pair)` implements all three — reuse:

1. **Proportional fill bar** (Religious crosses, Congress bells): displayed
   width = `max_width * current / needed`, icons packed into it; overlays
   the count as a number when packing gets ≤1px spacing or
   `always_show_number`. **Bug seen twice:** integer division truncating
   the width to 0 makes the whole bar vanish for small nonzero values —
   clamp to ≥1.
2. **Two-icon split bar** (Congress rebel/tory): always full width, both
   counts from percentages against a fixed slot budget, packed
   back-to-back.
3. **Natural tally** (Congress expeditionary force): no target, width =
   `count × ~2.2px`, number always shown.

## Shared chrome

Title + OK button drawn once in the dispatcher
(`reports_render_body_start`, `reports_render_ok_button`). Every plate's title call is
`FUN_281f_0022(title, 0, 0x140, y, ink)` -> `FUN_281f_0100` = `FUN_104b_0318`
(`reports_draw_dos_centered`): FONTTINY, one pass, x + (w >> 1) - ((width - 1) >> 1). F2-F9
(`FUN_3f41_0618` / `06d0` / `0d3e` / `10d8` / `1438` / `1710` / `1b94` / `1e80` / `20b4` / `2548`
/ `010a`) use y 5 (F8 y 2) and ink 0x90, a raw index of the plate's PIK (255,255,190 in every
REPORT*.PIK); F10 (`FUN_41f2_0092`) uses DS:0x831 = 149. The F4 / F5 / F6 subtitles (and F4's
"(job)" detail line) are the same call at y FONTTINY[0] + 6 = 12, ink 0x91; F10's subtitle is at
5 + FONTTINY[0] + 1 = 12 in 149, built with DS:0x55 ": " (one space). OK = `FUN_3f41_008a`
(x -2 -> 0x11e, y -1 -> 0xb8): outline x..x + 0x1d, y..y + 0xd in 0x77, then LABELS @MISC 46 in
FONTTINY ink 0x92, centred in x w 0x1e at y - ((FONTTINY[0] >> 1) - 7) = 188 (port_bugs.md
#1211). Title strip (y 0-21) and OK box now match every golden (religious, continental_p1,
labor_detail, economic_p1/p2, foreign, indian (palette-mapped), score: 0 px); the leftovers are
the DOS mouse pointer over labor.png's title (82 px), colony_p1/p2 body header rows 20-21 (47 /
15 px, not chrome) and naval's row rule at y 180 under the OK box (4 px). Every golden also shows
a 10 px black column at x 319 at a different y (indian.png y 85-94): the DOS mouse pointer parked
on the right edge, clipped to its outline column; nothing to draw. Test:
`unit_reports report_chrome_golden`. Exceptions:
Congress page 2 (full-bleed image, no chrome) and F10 Score (no OK button)
— both need click-anywhere dismissal wired in `game_loop.c`. Multi-page
reports plumb page state through `reports_render_w()` and the OK hit-test.

## Backgrounds and palettes

- **Check orphaned `REPORT<N>.PIK`s before building a background from
  scratch:** REPORT3.PIK is Congress page 1's real background (CCBKGD.PIK
  is page 2's) — the `REPORT<N>` = F`<N>` naming assumption broke there.
- A background loaded outside `backgrounds[id]` needs its palette wired
  into `game_loop.c`'s palette-selection ternary — right shapes, wrong
  colors otherwise.
- Sprite sheets (`ICONS.SS`) are remapped to a specific background's
  palette via nearest-RGB LUT (per-file duplicate pattern, no shared
  header — deliberate). If a report's icons look off-colored, check which
  background they were remapped against.
- **Plate DAC = VICEROY.PAL + PIK 152..251.** DOS-LITERAL `FUN_3f41_0000` (every
  F1-F8 body calls it with its plate digit; Indian passes **1**, so F9's plate is
  `REPORT1.PIK`, and `REPORT9.PIK` ships unused) loads `REPORT<n>.PIK` into a stack
  palette and programs only DAC 0x98..0xfb from it (`FUN_1c2e_0022`, AX 0x98, DX 0x64).
  DAC 0..151 and 252..255 keep the master palette `VICEROY.PAL` (`FUN_7a7c_000e`, 0x300
  bytes of RGB triples): nation pair 5 (170,73,0) / 13 (255,113,0), pure red 12
  (255,0,0), which the plates' own palettes leave as EGA magenta / pink.
  `reports_plate_dac` (`reports.c`) applies this at load to every plate except CCBKGD
  (page 2) and WOODPANL (F10), which other loaders install. Red inks are 12, not
  the plate's 112: economic negatives are attribute 4 + 8 (`FUN_3f41_1710`), foreign
  War is `(peace ? 0 : 0xfd) + 0xf` as a byte (`FUN_3f41_2548`, 15 / 12).
- **Don't assume a palette index means the same color across two
  `REPORT*.PIK`s** — probe each report's own palette (`pik_load` +
  nearest-RGB against the golden's sampled ink). F8's leader yellow is
  idx146, its paler label yellow idx145 (genuinely two colors), War red
  idx112, rule dark-red idx119; some carry over (cream 97), some don't.

## Data-field pitfalls

- **`head.founding_father[i]` is not "owning nation".** The authoritative
  per-nation source is `nation.founding_fathers[4]` bitmask (bit i = has
  FF i) — confirmed byte-exact via `sav_json` vs golden. When a struct
  comment and a golden disagree, the golden wins. The wrong field also fed
  a fallback chain in the Score FF count (broken field tried first,
  correct bitmask only on exact zero) — grep other reads of a field once
  it's proven unreliable.
- **Branch order in value pickers:** a since-retired bells sync ran a "treat
  bells as cumulative" branch first that almost always won, zeroing a valid
  live pool ("right after gameplay, zero on fresh load"). If a value is
  sometimes mysteriously zero, check earlier branches before suspecting the
  data.
- **A pre-existing test asserts what the code did, not DOS truth.** The F10
  Citizens rule (reports_score.md) flipped one; when a golden disagrees with
  a test, the golden wins.

## Portrait compositing (Congress page 2, any multi-sprite scene)

- Automated template matching (`compare -subimage-search`) is **not
  reliable** in cluttered scenes — it converged on the same wrong local
  minimum for unrelated sprites. Identify manually, verify each with an
  individual crop.
- Sprites are photo cutouts with opaque canvas margins — draw
  back-to-front sorted by `(y + height)` ascending, or an earlier sprite
  vanishes under a later one's background-colored padding.
- FF portraits draw from CC-xx.SS sprite anchors
  (`k_ff_portrait_draw_order[]`); a partial 10-of-25 coordinate table was
  removed (port_bugs.md Revere/Drake) — do not reintroduce one.

## `render_diff.sh` mismatch count is noisy for text

Font antialiasing between the golden capture and an indexed-palette PPM
yields nontrivial mismatch counts even at perfect alignment — was mistaken
for "still misaligned" once. Use it only as a coarse smoke check; confirm
position with a plain side-by-side (`convert a.png b.png -append`) or
crops.

## unit_chrome on report backgrounds: palette override

`unit_chrome`'s nation-color indices are tuned against **ICONS.SS's native
palette**; the box fill writes the raw index (no sprite-remap LUT), so
report palettes render it magenta. Fix generalized project-wide:
`unit_chrome_blit_unit_for_palette()` takes the caller's active palette
and nearest-matches; every screen (map, colony, Europe, pedia, combat
analysis, reports) passes its own palette. Caveat: REPORT7.PIK has no
color near Dutch orange `(255,113,0)` (nearest is visibly duller) while
the golden shows pure orange. Resolved 2026-10-28: the plate DAC keeps
VICEROY.PAL's 0..151 (see "Backgrounds and palettes"), so the nearest match is
now the exact slot 13.

Same investigation: ICONS.SS #0-3 colony markers carry a baked-in blue
flag (15 fixed pixels) that DOS recolors per nation (`FUN_112b_0c64`).
`unit_chrome_nation_flag_shades_for_palette()` +
`colonies_blit_settlement_icon()` overpaint them everywhere (dark shade =
light × ~82%, approximation).

## Body polish 2026-10-28 (verdicts + traps)

2x differing pixels after the plate-DAC fix (same day): religious / continental_p2 /
labor_detail / economic_p1 / foreign / indian / score 40 (pointer column), labor 328
(pointer over title), economic_p2 72, colony_p1 152 / p2 236, continental_p1 640, naval 1200.

- Body inks are PIK indices pinned by golden colour, not EGA guesses: yellow labels / names
  146 (0x92, (255,243,93); index 14 is (255,255,85)), cream counts / lists 97.
- `labor_detail.png` is job 19 (Free Colonists): `render_report ... 2 0 19`.
- Labor detail = `FUN_3f41_0d3e`: labels x 0xa0, values +0x5f, rows from y 0x19 step
  FONTTINY[0]+1; colony list y 0x19+0x28 step 9, columns 2 + n*0x69, 10 rows.
- `FUN_1097_0394` multi-slot bar (static in reports_congress.c; Ghidra drops AX=x DX=y BX=w,
  stack = gap; count-0 slots not queued; step = min(sprite w + 1, room / sum(count-1))). Congress
  sentiment slots = census_pop_proxy[nation] (DS:0x9410), flags = floor(slots * rebel% / 100).
  The colony meter row (`FUN_2f2b_17d0`) calls the same routine.
- Naval: cells centre with `reports_draw_dos_centered`; Ship / Destination centres 42 / 280 and
  row rules x 2..314 are golden-measured. Economic / colony row rules also start at x 2.
- Remaining residue: continental_p1 600 / colony_p2 84 are the water-cycle ramp DAC
  120..127. `FUN_3f41_0000` restores DS:0x372, so `FUN_1a0a_007a` keeps rotating it under
  a report and each golden froze its own phase (continental_p1 3 steps, colony_p2 7);
  `render_report` draws phase 0. Colony p1/p2 112: the colony-icon flag's dark shade is
  still the ~82% approximation (`unit_chrome_nation_flag_shades_for_palette`); the
  golden shows slot 5 (170,73,0). Naval 1160: ship-column unit chrome (x 0-20, y 40-110:
  silhouette / box pixels) and the cargo sprites at x 80-100, y 100-120 — shape, not palette.
- The live game does not rotate the report's 120..127 (the plate palette is static);
  DOS shows the current cycle phase there when cycling is on.

## `col1_bridge_apply` transport-chain bugs (reachable outside reports)

Col1 reuses `transport_chain` for plain same-tile stacking, not just
passenger manifests. `col1_find_ship_root` boarded any same-tile land unit
onto a docked ship, forcing `orders = Sentry` + `aboard_ship_id`:

- Fix 1: skip units whose raw orders are Fortify/Fortified before walking
  the chain (a fortified unit is definitionally not aboard). A Sentried
  bystander can still slip through — the Colony report additionally
  re-derives orders from raw `col1->unit[]` for display.
- Fix 2: `cargo_hold[]` past `holds_occupied` can be stale (DOS never
  clears trailing bytes on unload) — import only the first
  `holds_occupied` slots, don't per-slot check `0 < amt < 255`.
