# Porting a report screen (F2–F10)

Method + pitfalls from porting the F2–F10 reports, written so future screen
passes go faster. Read this before starting one. Code:
`src/core/reports.c`/`.h`; `reports_render()` dispatches per
`ColonizeReportId`.

## Tools

- `scripts/grid_overlay.sh <image> [step] [out]` — coordinate grid overlay.
- `scripts/render_diff.sh <reference> <candidate> [out]` — pixel diff;
  smoke check only (see "diff noise" below).
- `build/render_report <data_dir> <save.SAV> <out.ppm> [report_id] [congress_page2]`
  — calls `reports_load()`/`reports_render()` directly, no SDL/xvfb. **Use
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
(`reports_render_body_start`, `reports_render_ok_button`). Exceptions:
Congress page 2 (full-bleed image, no chrome) and F10 Score (no OK button)
— both need click-anywhere dismissal wired in `game_loop.c`. Multi-page
reports plumb page state through `reports_render()` and the OK hit-test.

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
- **Branch order in value pickers:** `founding_fathers_sync_from_col1()`'s
  "treat bells as cumulative" branch ran first and almost always won,
  zeroing a valid live pool — symptom "right after gameplay, zero on fresh
  load" (accrual doesn't run the sync path). If a value is sometimes
  mysteriously zero, check earlier branches before suspecting the data.

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
  removed (bugs.md Revere/Drake) — do not reintroduce one.

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
the golden shows pure orange — DOS likely reserves palette slots for
chrome; unresolved, left as nearest-match.

Same investigation: ICONS.SS #0-3 colony markers carry a baked-in blue
flag (15 fixed pixels) that DOS recolors per nation (`FUN_112b_0c64`).
`unit_chrome_nation_flag_shades_for_palette()` +
`colonies_blit_settlement_icon()` overpaint them everywhere (dark shade =
light × ~82%, approximation).

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

## Per-report facts

### Naval (F7)

4-column table (Ship/Cargo/Location/Destination), 7 rows/page; row rules
native y = 40..180 step 20, column rules x = 82/162/242. Passengers get
their own row above the ship's. Body font FONTTINY.

### Foreign Affairs (F8)

Unpaginated, fixed English/French/Spanish/Dutch blocks (rules y =
10/55/100/145, line pitch 7, columns x = 2/80). Withdrawn
(`player[n].control == 2`) nations show "(Withdrawn from New World)" and
are skipped from every peer list. Title sits at native y=2 (per-report
override; everyone else keeps the y=5 default).

- **War/Peace decode:** `ai_diplo.h`'s `AI_DIPLO_WAR` (0x01) never appears
  in the captured save; bit **0x02 set in either direction's byte** exactly
  fits every golden pair. Kept report-local (`reports_foreign_at_war()`) —
  a single-save empirical fit must not rewrite a shared live-AI module's
  semantics. (Superseded for the de Witt pass by the bit 0x40 = peace
  reading; see reports.md.)
- **Rebels/Tories total** is `stuff.census_pop_proxy[nation]` (DS:0x9410),
  not summed colony population (misses field colonist-type units);
  `rebels = floor(total × rebel_sentiment / 100)`. Caveat: that field is
  RMW-preserved from load, not recomputed during Linux play — fine for a
  loaded save, stale after live changes.

### Score (F10)

No OK button, no `REPORT<N>.PIK` — full-screen `WOODPANL.PIK` with
hand-placed layout. Ink: title/subtitle/Total idx 149 `(199,162,32)`,
everything else idx 68 green `(85,150,52)` (both exact palette entries,
dist2=0). Bottom bar: plain filled rect x=35..285 y=186 h=7, track idx
138, fill idx 68, proportional to `total/1000`. Golden shows only
Citizens/Congress/Gold/Rebel Sentiment/Total lines — Villages Burned /
Intervention / Independence fold into the total but aren't displayed
(golden's save has them zero; revisit with a later-game golden).

- **Citizens rule (the real find):** colony population always counts
  (invalid profession byte falls back to Free Colonist), but a map/Europe
  unit counts **only** when its raw `profession` byte is a genuine job
  (0..27) — sentinel 28 means zero credit, no type-based fallback.
  Confirmed exact (142+16=158). `reports_score_collect_citizen_jobs` is
  shared by the point total and the icon strip. This flipped an existing
  test's expected values — a pre-existing test asserts "what the code did",
  not DOS truth.
- Rebel sentiment reads `nation.rebel_sentiment` (nation+0x19) directly,
  not a recomputed colony-weighted average.
- Citizens icon strip wraps at pitch 8px (37 icons/row), odd rows shifted
  half an icon right, each row half an icon-height down; icon dims read
  from the sheet at runtime.

### Indian Adviser (F9)

Flat unpaginated list (DOS `FUN_3f41_010a` has no paging — don't invent
it), 21px blocks from y=28. Per tribe: headband portrait + plural name
(`NAMES.TXT @TRIBES` **column 0**, trailing colon) + right-aligned level,
then stats line (Villages always; Missions/Muskets/Horse Herds skip 0).
"Missions"/"Horse Herds" resolve live from `@MISC` 28/45;
"Villages"/"Muskets" have no shipped label string.

- **Muskets = `(indian.muskets + count of Armed Braves + Mtd. Warriors)
  × 50`** — from reading `FUN_3f41_010a` line-by-line. Horse Herds is the
  raw field. **Lesson:** when a field doesn't reproduce a golden number by
  obvious arithmetic, check whether the screen's exact DOS function is
  already decompiled and catalogued (`FUNCTION_CATALOG.md`) before
  guessing scale factors.
- **Missions counts villages whose mission nibble equals the *viewing*
  nation**, not "any mission" — the two read identical in the one golden
  (all missions there are Dutch), which would have hidden the bug forever
  without the decompile read.
- Tribe name/level colors need **no** remap against REPORT9.PIK (probe
  came back dist2=0) — probe per report, it varies.
- Name/level lines have a 1px down-right black drop shadow
  (`reports_draw_line_shadowed`); the black stats line doesn't.
- **Unresolved:** headband variants #113-117 are alarm-picked via an
  unidentified `0x281f` helper; both golden tribes render #113, so the
  port always uses #113.
