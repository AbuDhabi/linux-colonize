# Porting the Colony screen (multipurpose view) to golden fidelity

How the Colony screen (`src/core/colony_screen.c`/`.h`) was aligned to six new
DOS reference screenshots — two colonies (New Amsterdam, Recife, both Dutch)
× three `ColonyMultiMode` tabs (Production / Units ("military") /
Construction) — from `original_saves/report-screen-goldens/dutch-reports.SAV`.
Written up so future passes on this screen (or a similar deep pass on another
screen) go faster. Follows the methodology in `docs/report_screens.md` —
read that first if you haven't; this doc assumes it.

## Tool

`build/render_colony <data_dir> <save.SAV> <colony_name> <multi_mode> <out.ppm>`
(`tools/render_colony_main.c`) — this screen's equivalent of `render_report`.
Loads a Col1 save, bridges it (`col1_bridge_apply`, same as a live load),
finds the named colony, and calls `colony_screen_render()` directly (no
SDL/xvfb). `multi_mode`: 0 Production, 1 Units, 2 Construction. `convert
out.ppm out.png` to view; the golden-comparison workflow (2x goldens, divide
measured coordinates by 2, `-filter point` for zoomed crops, never trust
`render_diff.sh`'s raw pixel count for text) is identical to the report
screens' — see `docs/report_screens.md`.

The `COLONIZE/` asset directory is gitignored and wasn't present in the
worktree this session ran in; it was symlinked in from the main checkout
(`ln -s <main-checkout>/COLONIZE COLONIZE`) rather than copied, since
worktree isolation doesn't extend to gitignored data directories.

## Real bugs found and fixed

### Top bar: three separately-positioned fields → one centered string

DOS (`FUN_2f2b_0fce`) draws a single centered, green (`WOODPANL.PIK` idx 68,
exact RGB match `(85,150,52)`) string: `"<Name>.  <Season>, <Year>.  Gold:
<N>$"` — period after the name and after the year, comma after the season,
double space after each period, single space after the comma. The port drew
three independent white strings at fixed x positions (name left, date
center, gold right) with no punctuation. Confirmed by measuring the golden's
green-pixel bounding box (native width ~158px, centered at x≈160) and
matching that width exactly against `font_text_width()` for several
candidate format strings before confirming the punctuation by zooming the
golden's actual glyph shapes (a period is a bare baseline dot; a comma has a
trailing hooked descender — the mark after the colony name and after the
year are periods, the one after the season is a comma). Built locally in
`colony_screen_draw_top_bar` (splits `turn_format_date()`'s "Season Year"
string with `sscanf` to reinsert punctuation) rather than changing
`turn_format_date()` itself, which other screens use for a plain unpunctuated
date. Cross-checked against Recife (different colony name, same nation
gold) — matches.

### Area-view (minimap) field-tile badges: SoL bonus was excluded by design, but shouldn't be

`colony_screen_draw_area_overlays()` called `colony_yield_for_worker()` and
`colony_yield_town_commons()` with `sol_bonus=0`/`colony_flags=0` hardcoded,
on the stated rationale (see the old comment, and `docs/building_production.md`'s
"UI: settlement badges vs Production tab" table) that these on-map badges
show the *base* rate, SoL excluded, as a deliberate design choice. That
design note predates any Colony-screen golden ever existing — with one now
in hand, every field-tile badge on New Amsterdam's minimap (SoL 50%+100%
latched) is off by exactly the SoL bonus: a Lumberjack tile golden-shows 22,
base-rate-only gives ~18; an Ore Miner tile shows 14 vs ~10, etc. Fixed:
compute `sol_b_field = colony_prod_sol_bonus_field(col1, colony)` once and
pass it (plus `colony->colony_flags`) through to both calls, matching what
`colony_preview.c` (the Production tab's data source) already does. All
seven of New Amsterdam's field-tile numbers (14, 22, 5, 6, 7, 14, 16) and
Recife's (3, 3, 3, 4) now match exactly. The `building_production.md` table
entry claiming this exclusion "matches golden" was itself unverified
(written before this golden existed) — worth a skim-and-correct pass on that
doc's other unverified claims if a future session has spare time (see
"Left unresolved" below for one more instance of the same pattern).

### Settlement (per-building) badges: same SoL gap, plus missing font

Two independent bugs in `colony_screen_blit_buildings()`'s "Production
strip" badge (the small icon+number over each built building, e.g. the
Blacksmith's Tools output):

1. `colony_prod_building_display_output()` also hardcodes `sol_bonus=0` in
   its manufacturing branches (Blacksmith/Armory/Weaver/etc.) — same class of
   bug as the area-view fix above. Golden-confirmed: Blacksmith badge reads
   24 (SoL-boosted craft output), the sol=0 function gives 18. Fixed by
   preferring `view->preview.craft_gross[cargo]` (see below) for any badge
   whose icon is a cargo icon (manufacturing buildings); Town Hall/Church/
   Carpenter badges (bell/cross/hammer icons) also turned out to need the
   same swap — golden shows Church=19/Town Hall=82, matching the People
   band's crosses/bells totals exactly, not `colony_prod_building_display_
   output`'s own smaller local estimate (7/13) which predates FF/AI-subsidy
   folding (`colony_prod_colony_crosses_ff`/`_bells_ff`).
2. `colony_screen_blit_buildings()` had no `font` parameter at all and
   passed `NULL` to its number-drawing call — so no badge ever showed its
   number, full stop, regardless of the SoL fix. Not something a quick
   glance would catch: the icon alone still renders, and at low zoom a
   plausible-looking icon in a black box is easy to mistake for "badge with
   number" without a tight crop (this is exactly the "verify identity
   before trusting a coordinate" pitfall from `docs/report_screens.md`,
   recurring for a different kind of mistake — verify *content*, not just
   position, before moving on). Fixed by threading `font` through from
   `colony_screen_render()`.

### Production tab (multifunction pane): net-after-consumption shown where DOS wants each tier's gross output

The biggest content bug. `colony_screen_draw_multifunction()`'s Production
mode iterated `view->preview.goods[]` — every cargo's *net* change this tick,
after any downstream recipe consumes it (documented in `colony_preview.h` as
"net field+craft"). Golden shows something different: **each tier's own
gross production**, uncollapsed by whatever the *next* tier consumed from it.
New Amsterdam's Ore badge reads 28 (two Ore Miner tiles' raw output) even
though the colony's net Ore stock change that tick is only 4 once the
Blacksmith's own 24-ore draw is netted out; the Tools badge reads 24 (the
Blacksmith's gross output) even though net Tools is only 14 once the
Armory's 10-tool draw is netted out further. Confirmed unambiguously by
comparing the badge's printed digits against the sum of the per-tile debug
values from the (already-fixed) area-view loop — 14+14=28, matching the
golden pixel-for-pixel — before touching any code.

Fix: added two new fields to `ColonizeColonyPreview`
(`src/core/colony_preview.h`):

- `field_gross[COLONIZE_CARGO_COUNT]` — field-tile worker output only,
  captured by accumulating in parallel with the existing per-tile loop
  (*not* a `memcpy` of `goods[]` after the loop, which would double-count
  the town-commons auto-yield added earlier in the same function — the
  first attempt at this fix hit exactly that bug, caught via Recife's
  Cotton-shaped field_gross showing up unexpectedly when Recife has no
  Cotton Planter).
- `craft_gross[COLONIZE_CARGO_COUNT]` — each recipe's actual (stock-clamped)
  production this tick keyed by `out_cargo`, plumbed out of
  `colony_craft_preview()` via a new optional `gross_out` parameter (mirrors
  `colony_craft_one_colony`'s existing `actual_out` local, just also
  written to an out-array instead of only folding into the net `delta`).

The Production tab's slot-building loop now shows, per cargo (1..15, Food
excluded — see below): `field_gross[c] + craft_gross[c]` for everything
except Horses (`goods[HORSES]` directly — horse breeding has no craft
recipe of its own, so net already equals gross there). Town-commons
secondary yield (e.g. Recife's auto Sugar) is deliberately *not* folded into
`field_gross` — it only ever shows on the minimap's center-tile badge, not
duplicated in this pane (confirmed: New Amsterdam's town-commons Cotton
output is fully consumed by the Weaver and never gets its own Production-tab
badge, even though the golden shows the Weaver's Cloth output).

**Food is excluded from this pane entirely** — golden confirms no Food badge
here (it's shown on the People band's fish/grain meter instead); the old
code drew one via a fish/grain icon pair.

### People band: food meter read the wrong (pre-breeding) field

`colony_screen_draw_people_band()`'s food meter used `p->food_produced`,
which `colony_preview_compute()` captures *before* horse breeding subtracts
its feed cost from `goods[FOOD]`. Golden shows 32 (post-breeding), the port
showed 34 (pre-breeding) — a 2-point gap matching New Amsterdam's exact
horse-breed amount that tick. Fixed: read `p->goods[COLONIZE_CARGO_FOOD]`
instead (still `food_produced` in the `food_net < 0` shortfall branch, which
wants the raw pre-consumption number to decide grey-vs-colored display).

### Badge widget: DOS always shows the number, and never repeats the icon per unit

Every one of the badges above shares `colony_screen_draw_resource_count(_pair)`.
Two related, previously-unnoticed bugs (both pre-existing, not something
this session's other fixes introduced — they only became visible once the
underlying numbers were finally correct enough to compare shapes against):

1. **Number visibility was conditional on packing tightness**
   (`start_step <= 1`, the same heuristic `reports_draw_icon_bar()` in
   `reports.c` uses for its *progress* bars), inherited from that file
   without re-checking it applies here. It doesn't: every resource-count
   badge on this screen — area-view tiles, settlement badges, Production
   tab, People band — shows its number unconditionally in every golden
   example, regardless of how loosely the icon(s) pack. Cotton's
   minimap secondary badge (amount=5, packs loosely) was the first concrete
   case where the heuristic failed to fire and silently dropped the number.
   Fixed: the number is now always drawn (the `always_show_number`
   parameter is vestigial — every call site already passed `false` and
   relied on the heuristic, so nothing needed to change at call sites).
2. **One-icon-per-unit tally vs one-icon-plus-number label.** The pre-existing
   code drew `amount` copies of the icon spread across the cell (collapsing
   to a single visual blob only when the icon happened to be wide relative
   to the cell — which is why some badges looked right by accident and
   others, with narrower icons or bigger amounts, visibly fanned out into a
   wide strip of repeated icons that no golden shows). Every badge this
   function draws is actually a **plain count label**: one static icon (two,
   side by side, for a genuine two-good pair like fish+grain) plus the
   number — never a per-unit repeated tally. That distinction matters
   because `reports.c`'s icon bars on other screens (Religious crosses,
   Congress bells) *do* legitimately pack one icon per unit into a
   proportional-width bar — don't backport this screen's single-icon
   simplification there, and don't backport that file's repeat-per-unit
   packing here. Fixed: `colony_screen_draw_resource_count_pair()` now
   always draws at most one copy of each of up to two icon types,
   positioned immediately adjacent to the number (not centered in the full,
   often much wider, cell — golden-confirmed on the Production tab, where
   cells are ~46px wide but a badge like "24"+pickaxe is only ~35px of
   actual content), plus a **content-sized black background rect** behind
   the icon+number (golden-confirmed on every badge type; previously absent
   entirely off the minimap, where dark terrain pixels had been
   coincidentally standing in for it).
3. **One deliberate, documented exception**: the Construction tab's
   accumulated-hammers bar (see below) *is* a genuine one-icon-per-unit
   proportional tally, not a count label — it was pulled out of the shared
   function into its own small block rather than trying to make one
   function serve both widget shapes.
4. **Colors**: the Production tab's goods badges used palette index 10
   (renders green in this screen's active palette) where the golden's ink
   is white (index 15, the same index every other badge on this screen
   already used correctly) — a leftover from before any golden existed to
   check against.

### Construction tab

Three issues, one of them a real save-import bug reachable outside this
screen:

1. **Title/number color and centering**: matches the "Units Present" fix
   below (same idx 57 dark blue, centered — this session's second
   independent discovery of the same color, both confirmed by direct
   palette probe against `WOODPANL.PIK`).
2. **Accumulated-hammers row**: golden shows one wide proportional bar (e.g.
   "32" + ~24 packed hammer icons spanning nearly the full pane width), not
   the old code's two stacked half-height rows. This is the one genuine
   per-unit tally exception noted above — implemented as its own small
   block (content-sized black background, icons spread evenly across the
   pane width with no `COLONY_OUTSIDE_MAX` cap, unlike the shared
   selectable-strip helper).
3. **`building_in_production` raw-code decode bug in `col1_bridge.c`** (real
   bug, not this-screen-specific — reachable by anything reading
   `ColonizeColony.building_in_production` for a save-loaded colony). A
   pre-existing special case assumed "COL1 raw code 6 always means
   Stockade" and remapped it via `colonies_find_building(..., "Stockade")`.
   Recife's golden Construction tab reads **"Docks"**, not "Stockade" — and
   Docks is `@BUILDING`'s 7th (0-indexed 6th) entry in `NAMES.TXT`, i.e.
   raw code 6 *is* the file-order `@BUILDING` index directly, with no
   special case needed at all. The Stockade special case was an unverified
   guess (predating any Construction-tab golden) that silently mis-tracked
   any colony actually building Docks as building a Stockade instead —
   wrong `hammers`/`tools_cost` requirement shown, and (more seriously)
   wrong building type for `colonies_try_complete_building()` to actually
   complete once hammers reach the threshold. Fixed on both the import
   (`col1_bridge_apply`) and export (`col1_bridge_capture`) sides by simply
   removing the special case — raw code flows straight through as the
   `@BUILDING` index both ways.
4. **Artillery (a buildable *unit*, not a building) — display-only fix, real
   gap left open.** New Amsterdam's raw code is 42, one past `@BUILDING`'s
   last valid index (41, "Iron Works") — Col1 encodes buildable units past
   the building table's own range, and this port doesn't model unit
   construction as a queueable project at all (no completion/spawn logic
   exists anywhere in `colony.c`). `colonies_building_type()` correctly
   returns `NULL` for code 42, but the screen had no fallback and just
   printed "none". Fixed *only* the display: a small local special-case in
   `colony_screen_draw_multifunction()` recognizes raw code 42 as
   "Artillery" (title, and a synthesized 192 hammers / 40 tools
   requirement — the tools figure is golden-confirmed via the "(Requires 40
   Tools)" line; the hammers figure is the well-known DOS value, not
   independently re-derived this session) purely so the tab renders
   correctly. BUY/CHANGE still won't actually act on this project the way
   they would a real building — implementing unit-as-construction-project is
   a real feature gap for a future session, not a rendering bug.

### Units tab: missing "Units Present" title

`LABELS.TXT` `@CMISC` has this exact string, but nothing in `colony_screen.c`
ever loaded `LABELS.TXT` or drew it. Given the scope of plumbing a whole new
message-catalog load through `colony_screen_load()`/`ColonyScreenView` for a
single static label, it was hardcoded as a literal string instead (matching
this project's occasional precedent for stable, simple UI labels that don't
vary by save). Centered, dark blue (`WOODPANL.PIK` idx 57, exact RGB match
against the golden's sampled ink `(65,89,166)`). Reserves a fixed
`COLONY_MULTI_UNITS_TITLE_H` (8px) band above the roster grid — applied
identically at both the draw call and the hit-test call
(`colony_screen_hit_test`) via the same shared `colony_screen_multi_units_
layout()` so click regions never drift from what's drawn.

## Follow-up session: top bar height, minimap border, building placement

Three more player-reported gaps, fixed against `new_amsterdam_production.png`
specifically (New Amsterdam set as the reference colony for this pass).

**Top bar 4px too tall.** `COLONY_TOP_BAR_H` was 11; golden-measured (first-
bright-pixel-row scan) the real separator sits at native y=7, content starts
y=8. Fixed to 7 — `COLONY_MIDDLE_Y`/`COLONY_MINIMAP_SECTION_Y`/
`COLONY_VIEWPORT_Y` all derive from it, so the settlement and minimap panels
shifted up automatically with the one constant change; no other geometry
needed touching. (Left `COLONY_BOTTOM_SEPARATOR_Y` alone — golden-measured it
separately and it looks ~1px off too, but that wasn't reported and is a
separate, smaller issue.)

**Minimap had no border.** `colony_screen_render_minimap()` filled the 3×3
terrain grid but never framed it. Golden-measured: a 1px black square exactly
matching the grid's own `COLONY_MINIMAP_GRID*COLONY_MINIMAP_TILE` bounding
box (73×73 native, i.e. the grid_px the code already computes). Added via
the existing `colony_screen_draw_selection_box(...,0)` helper, drawn after
the tiles so the cursor's green box and unit/badge overlays
(`colony_screen_draw_area_overlays`, called right after) still layer
correctly on top.

**Building placement — investigated the real DOS mechanism, then measured
New Amsterdam's actual positions.** Player asked directly: is it
pseudorandom, or picked from a small set of fixed arrangements? Traced it to
`FUN_2f2b_0434` (`original_sources_decompiled/viceroy_unpacked.c:47259`):

- The ~15 real building categories are grouped into 5 size classes, each
  with its own fixed pool of candidate screen slots (baked static tables at
  segment-relative offsets — not extracted this session).
- For each category, DOS calls its RNG (`FUN_281f_04d4`, elsewhere confirmed
  as `dos_rng`) to pick a random slot within its size class, rejection-
  sampling (retrying) if that slot's already taken by an earlier category.
- The one confirmed call site is inside a debug/cheat-key handler (a `'!'`
  case in a large keystroke-dispatch switch) that also bumps
  `warehouse_level`/`capitol_level` — plausibly a "level up this colony"
  cheat that also forces a fresh layout roll. A legitimate non-cheat trigger
  (most likely colony founding) almost certainly exists too but wasn't
  chased down.
- Initially read the resolved assignment as living in fixed absolute-address
  arrays (not a per-colony save field), suggesting one mapping shared by
  every colony — **empirically wrong**: Recife's golden shows Town Hall
  top-center, New Amsterdam's shows it bottom-center, same building type.
  So a single static table can't match every colony simultaneously, because
  DOS itself doesn't produce one from a save file's perspective — there's no
  seed to reproduce, only a screenshot to match.

Given that, the only viable path was measuring the *resolved* positions
directly off one golden (not simulating RNG, not guessing): cropped each
building's exact sprite (dimensions read from `view->buildings.sprites[]`,
not guessed) from this port's own already-correct-content render, then
slid it pixel-by-pixel against `new_amsterdam_production.png` (`ImageChops.
difference` per candidate position — fast enough in pure PIL, no numpy
available in this environment), greedily claiming the best-scoring template
first each round so an ambiguous later template can't collide with an
already-resolved building's footprint. All 12 built buildings matched
cleanly (visually confirmed against the golden crop at each resolved
position — distinct, recognizable icon per building, no shape collisions);
the school tree-placeholder clump also matched well. `k_building_slots[]`
updated with the 14 resulting coordinates (see its own comment for the
full writeup, including why `stable`'s position is a low-confidence
fallback — no second distinct tree clump was found for it, since the port's
14-slot/3-group model doesn't map 1:1 onto DOS's real 15-category/5-group
one).

Recife (and any other colony) will legitimately not match this table —
confirmed not a further bug to chase, per the finding above.

### Building placement, take 2: porting the algorithm's *shape*, not its data

The New Amsterdam-only measured table above was superseded the same session.
Player asked directly whether positions are stored in the save or reseeded —
neither turned out to be quite right, and chasing the real answer down
found the actual mechanism: `FUN_2f2b_0434` opens by calling `FUN_15eb_1476`,
which reseeds DOS's RNG from **the colony's own map coordinates**,
`(colony.y<<8)|colony.x` (plus a second term, DS `0x8d80` /
`save_format_map.md`'s `boot_timer`, itself read back from the save's own
global-state block on load — not a live clock read). That's why positions
are stable across reloads *and* full restarts (same colony ⇒ same reseed ⇒
same rolls) while differing between colonies (different map position ⇒
different reseed) — confirming the single-shared-table read above was wrong
in a *different* way than first thought.

This meant the real algorithm was finally fully understood and, in
principle, portable — `dos_rng`/`dos_rng_range` already exist in this
codebase and colony (x,y) is already a stored field. What was still missing
was DOS's actual static data: the 5 group-size candidate-slot pools. Getting
those needed live memory inspection, and that's where it stalled for good:

- The decompile's `FUN_SSSS_OOOO` naming looks like literal segment:offset
  but isn't — confirmed via `tools/address_mapping.csv`, `FUN_2f2b_0434` is
  actually `OVL03_L0000` offset `0x434`, an overlay loaded to a runtime
  segment picked by DOS at load time, unrelated to the literal `2f2b`.
- A breakpoint on the literal `2f2b:0434` (player-tested, live DOSBox-X):
  never fired.
- A breakpoint on `INT 21h AH=4Bh` (the standard DOS overlay-load call,
  player-tested): never fired either — this game's overlay manager isn't
  the standard MS-DOS one.
- `tools/viceroy_v2_output_layout.json` has per-overlay `loadSegment`
  values from the original `rtlink_decode` work, but they're relative to
  an overlay *area* base, not an absolute runtime segment — still missing
  the one live-only piece.
- Static file extraction using `docs/viceroy_tables.md`'s DS→file-offset
  formula (which works for the map/terrain tables) landed on unrelated
  code — that anchor is specific to a different segment's data, confirmed
  by decoding straight into an `int 21h` opcode at the target address.
- A DOSBox-X save state was considered as a fallback (dump full RAM, search
  for the tables by pattern instead of needing an exact address) but its
  `Memory` component turned out to be DOSBox-X's own paged serialization,
  not a flat dump (17MB file for 4MB configured RAM) — parsing that format
  from scratch was judged not worth it for a cosmetic-only fix, and the
  player agreed to stop chasing exact DOS fidelity here.

**Landed on**: port the algorithm's *shape*, not DOS's actual data. Same
structure as the real mechanism — size-class grouping, per-class candidate
pool, RNG-reseeded-from-colony-xy, reject-sampled assignment — but the pools
are this port's own invented screen real estate (specifically, the 14
positions from the superseded New-Amsterdam-only table above, regrouped by
size class instead of by specific building), and the seed XORs in an
arbitrary salt (`0x434`) in place of DOS's unrecoverable second term.
Result: every colony now gets a full, non-overlapping, size-correct,
per-colony-stable-and-cross-colony-different layout — a *valid* output of
the same kind of process DOS runs, just not *the same* output DOS would
produce for that colony. `colony_screen_assign_slot_positions()` (shared by
the draw path and both hit-test call sites, so clicks always match what's
drawn) is the implementation; see its own comment and
`k_building_slots[]`'s for the full detail. Revisit only if DOS's actual
static tables ever become recoverable (a working live-memory technique for
this game's overlay scheme would be the prerequisite) and exact fidelity
starts to matter more than it does today.

### Follow-up: parchment fill coverage, reserved dock/fence corner

Two more player-reported gaps against `new_amsterdam_production.png`:

1. **Parchment fill left a 5px strip of bare wood-panel chrome along the
   bottom and looked seam-y along the right edge.** `colony_screen_fill_parch`
   was tiling to `COLONY_VIEWPORT_W`/`_H` (202×114), both stale leftovers
   from before the top-bar-height fix above shifted `COLONY_MIDDLE_Y` up —
   the actually-available parchment area (`COLONY_MIDDLE_Y` to
   `COLONY_BOTTOM_SEPARATOR_Y`, left edge to the minimap section) is 200×119.
   Rather than resize `COLONY_VIEWPORT_W/H` themselves (used elsewhere for
   building/hit-test math, already tuned), added `COLONY_PARCH_FILL_W/H` —
   fill-only extent, computed from the same section/separator constants so
   it can't drift out of sync again.

2. **The random building pool could place a building on top of the
   docks/drydock/shipyard slot.** That slot isn't random — it's drawn
   separately, fixed at the settlement's bottom-right corner (`coast_x`/
   `coast_y`/`fence_x`/`fence_y` in `colony_screen_draw_area_overlays`),
   because that's where the coastline art actually is. Player-verified via
   live DOS: drydock's sprite center sits around native (188,66), shipyard's
   around (142,90) — both inside that corner box, consistent with one fixed
   top-left-anchored slot whose apparent center shifts a little with each
   tier's sprite. One pool point (`k_group_med_slots`'s old `{127,45}`, plus
   `k_group_small_slots`'s old `{173,45}`) landed inside that box by
   coincidence of the earlier New-Amsterdam-measured data, producing a
   building rendered half-overlapping the coast/shore art. Fixed two ways:
   relocated both points clear of the corner, and added
   `colony_screen_slot_reserved()` — a runtime reject check (footprint vs. a
   `COLONY_RESERVED_*` box derived from the same corner math) wired into the
   rejection-sampling loop, so a future pool edit that strays back into the
   corner gets skipped instead of silently overlapping again.

## Left unresolved
- **Two golden-confirmed building badges reuse the same displayed number**
  (Town Hall and Printing Press both showed "82" in New Amsterdam — Town
  Hall's is definitely bells, per the People band cross-check documented
  above; Printing Press's badge *type* resolving to the same bells value
  looks like a separate, real pre-existing bug in
  `colony_screen_building_production_badge`'s badge-type assignment for
  Press, not investigated this session) and Carpenter showed a "16" badge
  this session didn't expect from the original badge-value pass (see above)
  — worth a from-scratch recheck of every building's badge *type*, not just
  the amount-source bug already fixed.
- **People band's Food badge has an extra visual element not reproduced**:
  the golden shows a thin gold/olive horizontal bar underneath the "32"
  number (screenshot: `original_saves/report-screen-goldens/
  new_amsterdam_production.png` around native x=2..40, y≈166) that no other
  badge on this screen has. Likely a food-stock or SoL-related proportional
  fill specific to this one meter; not identified or implemented this
  session.
- **A second, unexplained badge in the same golden crop**: golden's People
  band shows a small "2" with no black background box (just icon+number
  directly on the grass) between the Food and Crosses meters, matching
  nothing this port currently draws there. Not identified.
- **Units tab minor style gaps**, lower priority than the content fixes
  above: (1) unit *ordering* within the roster (Artillery vs Dragoon) is
  reversed vs. the golden — likely an `outside_unit_ids[]` ordering
  difference, not investigated; (2) the golden's mode-selector buttons
  (house/rifle/hammer, top-right) don't show this port's green selection
  box style for the active mode — the golden's own "active" indicator style
  wasn't identified; (3) Recife's minimap shows a plain white square outline
  on one ocean tile that this port doesn't draw (purpose unknown — possibly
  a "selected tile" cursor left over from the captured moment, not
  necessarily a static-render feature).
- **BUY/CHANGE button hotkey-letter coloring**: this port highlights the
  first letter of each button label in a distinct color (the `~` hotkey
  convention, shared with Europe-screen buttons); the golden shows plain
  uniform-color text with no highlighted letter. Not changed — didn't want
  to touch shared `ui_button.c` behavior used by other screens without
  golden coverage for those too; flagging in case a future pass finds the
  same gap elsewhere.
- **`docs/building_production.md`'s "UI: settlement badges vs Production
  tab" table** is now stale in two places (the settlement-badges SoL
  exclusion, and implicitly the Production-tab-shows-net description) —
  worth a follow-up edit pass now that real Colony-screen goldens exist to
  check its other claims against, rather than leaving the contradiction
  sitting in two docs.

## Final match quality

All six renders (`build/render_colony` against `dutch-reports.SAV`) are a
tight visual match to their goldens after the fixes above: title bar, area-
view field/settlement badges, People band, Production tab, Units tab title,
and Construction tab all match numerically and stylistically. The one
remaining large visual gap is the buildings-section layout positions (first
item above) — content-correct, position-wrong. Everything else left open is
a small, isolated, independently-flagged detail.
