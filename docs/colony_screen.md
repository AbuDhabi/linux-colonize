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

`LABELS.TXT` `@CMISC` index 1 has this exact string. **Fixed 2026-08-27**:
turned out cheaper than this row's own earlier assessment feared — no need
to plumb a catalog through `colony_screen_load()`/`ColonyScreenView` (that
would persist across frames unnecessarily); `colony_screen_render` is
already called fresh every frame, so it just gained a `labels` parameter
(6 call sites total: `game_loop.c`, `tools/render_colony_main.c`, 5 in
`test_colony_screen.c`, all passing `NULL` except the real game loop),
threaded one level down to `colony_screen_draw_multifunction`. Falls back
to the same hardcoded literal when `labels` is NULL. Centered, dark blue
(`WOODPANL.PIK` idx 57, exact RGB match against the golden's sampled ink
`(65,89,166)`). Reserves a fixed `COLONY_MULTI_UNITS_TITLE_H` (8px) band
above the roster grid — applied identically at both the draw call and the
hit-test call (`colony_screen_hit_test`) via the same shared
`colony_screen_multi_units_layout()` so click regions never drift from
what's drawn.

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

### DEBUG menu: Building Rects

`MAP_MENU_ACTION_DEBUG_BUILDING_RECTS` (DEBUG pulldown, default off, persisted
as `debug.building_rects` in settings.json, port-only — `COLONIZE_DEBUG_MENU`)
draws a violet outline (`COLONY_DEBUG_RECT_COLOR` =
13, WOODPANL.PIK's bright EGA magenta — not used anywhere else on this
screen) around every colony building sprite's actual bounds: the 14 random-
pool slots plus the fixed docks/fence corner. `game->debug_building_rects`
threads through `colony_screen_render()`'s new parameter into
`colony_screen_blit_buildings()` and `colony_screen_draw_area_overlays()`.
`build/render_colony` also takes it as an optional 6th CLI arg
(`... out.ppm 1`) for headless iteration without launching the game. Reason:
placement is more than cosmetic — buildings are clickable (workers, badges,
Change/Buy), so getting their clickable bounds visibly right matters, not
just their look.

### Golden-exact overrides: New Amsterdam and Recife

Per-colony casuistry, not a general fix — deliberately narrow, since this
whole placement algorithm (see above) is a placeholder for a real DOS port
later. For the two colonies with reference screenshots, every *built*
structure now sits at its exact golden pixel position, found by brute-force
template matching: dump each `BUILDING.SS` sprite through the same
palette (`colony_screen_load`'s `view.frame.palette`), slide it over the
golden PNG (downscaled 2×→native) computing sum-of-absolute-difference per
candidate position, keep the global-minimum position. Most built structures
matched at an exact **zero** score; the few non-zero ones (Town Hall,
Fort/Stockade) are still the clear global minimum, just partly occluded by
worker-icon/badge overlays drawn on top. `ColonyPlacementOverride` in
`colony_screen.c` keys on the colony's fixed `(x,y)` (New Amsterdam
50,43; Recife 41,38) and overrides only the `k_building_slots[]` indices
with a confident match — `colony_screen_assign_slot_positions()` runs the
normal algorithm first, then overwrites. Unbuilt categories (drawn as a
tree-clump placeholder) are deliberately left on the general algorithm:
template-matching them was inconclusive — DOS scatters filler trees more
freely than the fixed per-class pool used for real buildings — and they
aren't interactive, so exact placement doesn't carry the same bar. One
notable side-finding promoted into the shared code (not colony-specific):
the docks/fence corner anchor (`colony_screen_docks_fence_anchor()`) matched
the same `(123,55)`/`(123,106)` exactly for *both* colonies, including on
Recife's unbuilt coast placeholder — good evidence it's a genuinely fixed
screen slot rather than random, a candidate to become the real formula
(replacing the viewport-relative right-anchor math) once verified on a
third colony.

**Follow-up fix (player-caught): every overridden building sat 1px
right/8px down from its real spot, and one unbuilt tree placeholder
visibly overlapped a real building.** Both from the same bug: the matcher
searches the *full* 320×200 golden frame and returns absolute coordinates,
but `pos[]` is documented (and used, via `slot_ox + slot_x[i]`) as
viewport-relative — every override value needed `COLONY_VIEWPORT_X/Y`
(1,8) subtracted first, and it wasn't. Once corrected, most values landed
exactly on an existing `k_group_*_slots` pool point — real DOS reuses the
same candidate pool, just assigns it differently — which also explains the
overlap: the general algorithm's RNG had no idea a given pool point was
already claimed by an override and could hand the identical point to an
unbuilt neighbor's tree. Fixed by marking any pool point that exactly
matches an override as `taken` before the RNG runs, so unbuilt categories
only draw from what's actually still free.

**Second follow-up fix (player-caught): a placeholder copse still
overlapped a real building — same one in both colonies (Town Hall in
Recife, the church in New Amsterdam).** The exact-pool-point `taken`
bookkeeping above only prevents a *same-group* duplicate; it did nothing
for a *different*-group overlap, and that's exactly what this was: an
unbuilt SMALL category, forced onto the pool's one remaining free point
because the other 7 SMALL slots were all claimed by overrides, and that
last point (`{110,20}`, itself only added this session as a replacement
for the earlier reserved-corner violator) happened to sit under wherever
the LARGE pool's `{86,3}` slot ends up (Town Hall or church, whichever
didn't get the override). Fixed two ways: `colony_screen_slot_overlaps_placed()`
now tracks every already-placed slot's rectangle (overrides seeded in
first) and rejects a candidate that overlaps *any* of them, not just
same-group ones — a real, general improvement, not casuistry, since this
same silent cross-group collision could happen to any sufficiently
built-up colony; and `{110,20}` itself got relocated to `{60,27}`, clear
of both LARGE points and 6 of the other 7 SMALL points outright (a brute-
force check over the *whole* pool found zero fully-clean spots for a 14th
box — this layout is that tightly packed — so `{60,27}`'s one remaining
nick, a 2×18px corner against fur's slot, is the best available; real
sprites have enough transparent margin in their bounding box that it
doesn't show, unlike the canopy-through-a-roof the old spot produced).

**Third follow-up fix (player-caught): `{60,27}` still showed as an extra
copse in both colonies.** The "2×18px corner" estimate above was checked
against only one neighbor (fur) by hand — a fuller check shows it also
clips weaver's slot (New Amsterdam) / the same point reused for a
different category (Recife), and a follow-up brute-force confirmed there
is no 14th box anywhere in this pool that's fully clean against the other
13 — real DOS sprites clearly tolerate bounding-box overlap that a coarse
rectangle can't (transparent margins, irregular silhouettes); this port's
box-based algorithm structurally can't match that without per-sprite pixel
masks, well beyond a placeholder's scope. Rather than keep hunting for a
coordinate that provably doesn't exist, added a third override state —
`COLONY_OVERRIDE_HIDDEN` (`{-2,-2}`) — that skips drawing/hit-testing a
specific unbuilt category outright instead of forcing it onto a bad slot.
Applied to New Amsterdam's `stable` (all 8 SMALL pool points end up
claimed by its other 7 built categories — none left, forced or not).
`COLONY_SLOT_HIDDEN` is a plain sentinel value in `xs[]`/`ys[]` (not a new
drawn state) — `colony_screen_blit_buildings()` and the whole-building
hit-test loop both skip an index when its slot lands on it and the
category is unbuilt (a *built* category can never be hidden — the check
is gated on `built < 0`).

**Fourth follow-up fix (player-caught): Recife still had it too.** Wrong
arithmetic in the third fix's writeup — Recife has 3 unbuilt SMALL
categories (press, stable, custom) and exactly 3 SMALL pool points free
after its 5 built ones, not 2-of-3 with slack as assumed; with the count
exactly matching, all 3 free points get used regardless of which one is
bad, so the conflict was never actually avoided on its own. Hid Recife's
`custom` (the category that happened to land on the bad point) the same
way as New Amsterdam's `stable`.

### Every colony now reuses one of these two layouts

Player's ask, once both were verified overlap-free and golden-exact: stop
generating a synthetic per-colony arrangement altogether and have *every*
colony draw one of these two real ones, to see how they read against other
colonies' actual built/unbuilt mixes. `colony_screen_find_override()` no
longer returns `NULL` for a colony that isn't New Amsterdam or Recife —
it picks between the two tables with a `dos_rng` draw seeded from that
colony's own `(x,y)` (a different salt than the position-assignment RNG,
though that no longer matters much once a layout applies). Stable per
colony across reloads, an even-ish mix across colonies (16 sample colonies
from `dutch-reports.SAV`: 11 New Amsterdam-style, 5 Recife-style).

The general RNG-pool algorithm didn't go away — both tables still leave
some categories at `COLONY_OVERRIDE_NONE` (church on both, several more on
Recife's), and it fills those in exactly as before, now seeded by
`colony_screen_pick_pool_slot()` (the fallback chain factored out of the
old inline loop so it could be reused — see next paragraph). It also
still runs the reserved-corner and cross-group overlap checks, so a
non-source colony's *different* mix of built categories still can't
collide with itself.

The one real risk in reusing someone else's table: `HIDDEN` (New
Amsterdam's `stable`, Recife's `custom`) means *that specific colony's
source layout has no room for this placeholder* — it says nothing about
whether some *other* colony reusing the table actually has the category
built. A built category must never just vanish. Verified against
`dutch-reports.SAV`'s Jamestown/Quebec/Fort Orange/New Holland (all have
`stables: 1`, all draw New Amsterdam's layout): `colony_screen_assign_
slot_positions()` now takes `pool` too, and when it hits a `HIDDEN` slot
checks `colony_screen_best_built()` for real; if built, it falls back to
`colony_screen_pick_pool_slot()` for a genuine (still collision-checked)
position instead of hiding it. In practice this never shows visually
anyway — `BUILDING.SS`'s Stable sprite (#17) is a degenerate 1×1 pixel in
this asset set (no dedicated art), and `colony_screen_blit_slot()`/the
hit-test loop both already skip anything `width <= 2`, same guard that's
been there since before this session.

### Dock corner placeholder: DOS shows it inland too

Player-caught: `colony_screen_blit_buildings()` only drew
`COLONY_COAST_PLACEHOLDER` (#45, trees + shore) when Docks/Drydock/
Shipyard wasn't built *and* the colony sat on a coastal tile
(`map_tile_is_coastal`) — an inland colony (can never build Docks in the
first place) got a blank corner instead. Player confirmed DOS draws this
placeholder in every colony's dock corner regardless of geography — it's
not meant as a literal "there's water here" cue, just the generic "future
building" filler, same as every other unbuilt category's tree clump.
Dropped the `coastal` gate (and the now-unused `coastal` parameter/
`map_tile_is_coastal` call that computed it) — verified against Fort
Orange, the one inland colony in `dutch-reports.SAV`'s 17: the coast art
now renders there too.

### Follow-up session: fence unit filter, Lumber Mill badge, resource-count widget revert

Three more player-caught bugs, one of them undoing part of the "Badge
widget" fix above:

1. **Fortification strip showed Artillery.** The Stockade/Fort/Fortress
   strip (`colony_screen_blit_buildings()`'s fence draw + its hit-test
   counterpart) drew every non-transport on-tile unit, same list
   `colony_screen_multi_units_layout()` deliberately uses for the Units-
   Present/Military tab (which *does* want Artillery there). DOS's fence
   strip is colonist figures only. Added
   `colony_screen_unit_is_artillery()` and skip it at both the fence draw
   and fence hit-test sites; `colony_screen_multi_units_layout()` and the
   People-band "outside" row are untouched (both correctly still include
   Artillery).
2. **Lumber Mill workers got no Hammers badge.**
   `colony_screen_building_production_badge()` matched building names via
   `strstr(name, "Carpenter")` — true for "Carpenter's Shop", false for its
   upgrade "Lumber Mill", so the per-building Hammers badge silently
   disappeared the moment that building leveled up. Added
   `|| strstr(name, "Lumber Mill")`.
3. **Resource-count badges: the "Badge widget" fix above was itself a
   misreading.** That earlier pass concluded DOS badges draw one static
   icon plus a deliberately-painted content-sized black background rect,
   based on the numbered goldens alone. Two new "numberless" reference
   captures (`new_amsterdam_production_numberless.png`, `recife_..._
   numberless.png` — screenshots with the player's "always show numbers"
   toggle off) prove that reading wrong: comparing numbered vs numberless
   goldens side by side shows the toggle affects *only* the area-view
   field-tile badges (`colony_screen_draw_area_overlays`'s per-tile yield
   numbers) — every resource-count badge this function draws (settlement/
   Production-tab/People-band/area-view) shows its number in both
   captures, unconditionally, confirming that half of the "Badge widget"
   fix was right. What was wrong: the badge is **not** a single icon.
   `colony_screen_draw_resource_count_pair()` reverted to blitting
   `amount` real copies of the icon, evenly spread (and mostly overlapping,
   for anything beyond a handful) across the cell — exactly
   `colony_screen_draw_icon_strip()`'s Note-1 approach, same as before the
   "Badge widget" session, minus that session's number-visibility
   heuristic (kept unconditional here, matching point 1 above). The
   explicit black background rect is gone entirely: what looked like a
   painted pill in the goldens is actually the natural result of many
   black-bordered icon copies overlapping almost completely at any
   non-trivial amount — real, and it scales with `amount` (a bigger stock
   badge smears wider), which the fixed-width single-icon-plus-box version
   never did. Re-verified against both goldens (numbered and numberless)
   for New Amsterdam and Recife — no regression on any badge checked.

### Explicit shadow for colonist/on-tile-unit figures, then: a real 3-mode component

Investigated a report of "brownish" settlement-view colonist shadows vs
"black" minimap ones first — sampled shadow pixels at every colonist draw
site (People band, building workers, fence dragoon, minimap field workers)
against golden: all landed on the same near-black (12,12,12), matching
golden exactly, no brown found anywhere in `dutch-reports.SAV`. Player
confirmed exact hue doesn't matter (greyish is fine) — the ask was just to
make sure every such figure actually has *some* shadow, explicitly, rather
than relying on whatever's baked into each sprite's own art (inconsistent:
some working-colonist sprites bake in a dark blob, others don't).

First pass added a colony_screen-local `colony_screen_blit_icon_shadowed()`
with a 1px-left shadow. Player caught that: DOS's shadow (and
`UNIT_CHROME_SHADOW_DX`, the map view's own convention) is 2px, and pointed
out the deeper problem — sprite blitting across the codebase is ad hoc
(colony_screen.c hand-rolling its own one-off shadow helper is exactly
that), and asked for a real shared component instead: a named draw mode per
call site rather than each screen improvising its own blit sequence.

Added to `unit_chrome.h`/`.c` — every screen's unit/colonist sprite draw
collapses into exactly one of three modes:

- `UNIT_CHROME_PLAIN_SPRITE` — just the sprite.
- `UNIT_CHROME_SPRITE_WITH_SHADOW` — sprite + the same 2px-left black
  silhouette underlay `UNIT_CHROME_SHADOW_DX` already uses for the map/
  orders mode (tinted by a caller-supplied `shadow_color`, 0 = black —
  every caller today passes 0), no orders box.
- `UNIT_CHROME_SPRITE_ORDERS` — shadow + nation-color orders/allegiance box
  + sprite; identical to what `unit_chrome_blit_unit_colored` already drew.

One dispatcher, `unit_chrome_blit(fb, font, sheet, sprite_index, x, y, mode,
shadow_color, display_type_index, nation_id, orders_index, show_stack,
aboard, fill_override, letter_override)`, picks the draw path by `mode`;
params outside a given mode's own list are ignored (pass 0/false/-1/NULL).
ORDERS internally shares the exact same code `unit_chrome_blit_unit_colored`
runs (factored into a private `unit_chrome_blit_unit_colored_shadow` both
now call) — no behavior change for any existing `unit_chrome_blit_unit*`
caller, and `unit_chrome_blit_unit`/`_colored`/`_for_palette` stay as the
ergonomic ORDERS-only entry points (no need to pass ORDERS-irrelevant params
just to draw a garrisoned/on-map unit the way every caller already does).

`colony_screen_blit_icon()` and `colony_screen_blit_icon_shadowed()` now
just forward to `unit_chrome_blit()` (PLAIN_SPRITE / SPRITE_WITH_SHADOW
respectively) instead of open-coding `ss_blit_sprite(_color)` — fixes the
1px→2px shadow bug and the ad-hoc-blitting complaint together. Applied
everywhere a colonist/on-tile-unit icon is blit standalone:
`colony_screen_draw_icon_strip()`'s loop (building-worker Note-1 strip +
fence/fortification strip), the People band's colonist row and its
outside-unit row, and the minimap's per-tile field-worker icon. Left alone
(already correct, and already routed through the shared ORDERS impl): the
Units-Present/Military tab and Transport strip
(`unit_chrome_blit_unit_for_palette`).

Follow-up: migrated `reports.c`'s three hand-rolled 2px-shadow call pairs
(`ss_blit_sprite_color(..., 0)` + `ss_blit_sprite`, each a manual copy of
the same shadow convention — Labor report's profession-icon grid and its
per-job detail header, plus the Colony report's Town-Hall-worker row) onto
`unit_chrome_blit(..., UNIT_CHROME_SPRITE_WITH_SHADOW, ...)`, same
mechanical swap as `colony_screen_blit_icon_shadowed`. Checked
`map_panel.c` too: its actual unit draws already go through
`unit_chrome_blit_unit_for_palette` (sharing this same core impl since the
refactor above) — its three other raw `ss_blit_sprite` calls are a village-
tech icon on the map and two cargo-hold sidebar listings (passenger sprite,
goods icon), none of which are shadowed in DOS either, so nothing to
migrate there. Re-rendered the Labor and Colony reports against
`dutch-reports.SAV` after the swap — unchanged.

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
  tab" table** was stale in two places. **Settlement-badges SoL exclusion
  fixed 2026-08-27**: its "callers... pass `sol_bonus=0`" blanket claim
  for settlement badges corrected — both badge types now fold SoL in, per
  the fixes above. The "Production-tab-shows-net" half wasn't a literal
  claim findable in that table as currently worded (its "Every cargo
  good / shortfall + hammers" row doesn't say net vs. gross either way) —
  left as-is rather than editing a claim that isn't actually there; still
  worth a fuller cross-check against goldens if this table's other rows
  are ever revisited.

## 2026-08-26 fix: Custom House popup + Production tab cell grouping

Player-reported, two items:

1. **Clicking the Custom House opens its per-cargo autosell checklist.**
   Previously a plain click fell through to the worker-assignment path
   (`game_colony_assign_building_drop`) like any other building, which now
   always fails for Custom House (see the "worker-blocked buildings" fix
   below) — so a click just silently did nothing. New popup
   (`colony_screen_open/close/draw_custom_house`, `COLONY_HIT_CUSTOM_HOUSE_
   ROW/_OUTSIDE`): lists every export-eligible cargo
   (`europe_cargo_export_eligible`'s existing denylist — not Food/Horses/
   Tools/Muskets), colored green/white by
   `europe_custom_house_cargo_enabled()` (the same read this session's
   cargo-strip fix already added), row click toggles the bit via new
   `colonies_toggle_custom_house_cargo()` and the popup stays open (a
   checklist, not pick-one-and-close like the Jobs popup). Title pulled
   live from GAME.TXT `@CUSTOM` ("Which cargos shall our Custom House
   export?", `@checkbox`/`@smallfont` — the DOS catalog entry actually
   documents this as a checkbox popup) via `popup_msg_fill`, falling back
   to the same string if the catalog isn't loaded. Tried a literal
   `[X]`/`[ ]` checkbox prefix first — this pixel font doesn't have usable
   `[`/`]` glyphs (rendered as unrelated garbage shapes) — dropped it in
   favor of the green/white color alone, which was already legible on its
   own in a render check.
2. **Production tab: group each resource's numbers into one cell.**
   Player-specified rule set, golden-checked pixel-by-pixel against New
   Amsterdam's Production-tab crop (`new_amsterdam_production.png`):
   - produced, nothing downstream wants it → one plain number.
   - not produced, something wants it → one grey/red "short" number.
   - produced, less than downstream wants → produced (white) + short (red)
     together in one cell, two side-by-side boxes with a spacer — same
     rendering as the surplus case below, not overlapping icons (2026-08-26
     follow-up: player asked for the same split-with-spacer style used for
     surplus; confirmed: Cotton "5 | 5", Horses "2 | 2", Cloth "5 | 5" —
     previously each of these was two separate grid cells, then briefly an
     overlapping-icon pair before this follow-up).
   - produced in surplus of what's used → two adjacent white counters (used
     + stored) in one cell with a spacer. The only real case of this in the
     game is Lumber→Hammers, which isn't a `colony_craft_preview()` recipe
     (hammers banking is `colony_prod_colony_hammers`, computed separately)
     so it never earned a `shortfall[]` entry — special-cased directly in
     `colony_screen_draw_multifunction` using `min(hammers, field_gross
     [LUMBER])` as the "used" split; golden-confirmed 22 Lumber = 16 used +
     6 stored, not a plain "22". Ore and Tools are *also* partially consumed
     downstream in this same save (by the Blacksmith and Armory
     respectively) but the golden shows them as plain single numbers (28,
     24) — confirmed craft-recipe consumption alone does *not* trigger the
     split, only Lumber's stock-banked-hammers path does.

## 2026-08-26 fix: cargo strip digit colors + position, worker-blocked buildings

Player-reported, checked against `new_amsterdam_production.png` at pixel level:

1. **Cargo icon/number position**: icons sat 1px too high, numbers 2px too
   high relative to the golden. Fixed: icon blit `y` is now
   `COLONY_CARGO_STRIP_Y + 1`; number draw `y` is now `COLONY_CARGO_NUM_Y +
   2`.
2. **Number colors**: the port drew every digit in one flat color (white,
   or green/red keyed off an unrelated this-turn production delta). Golden
   pixel-sampling (exact RGB match against `WOODPANL.PIK`'s palette, not
   nearest-color) found DOS actually splits each stock number into two
   independently-colored runs: **(a)** a 3-digit stock's hundreds digit is
   always gold (palette index 148, `(227,195,40)`), regardless of cargo —
   confirmed on Food=159 ("1"/"59") and Muskets=130 ("1"/"30"); **(b)** the
   remaining (tens+units) digits are green (index 10) when this cargo is
   currently toggled on in the colony's Custom House per-cargo sell mask —
   `europe_custom_house_cargo_enabled()`, a new public wrapper around
   `europe.c`'s existing (already-implemented) autosell bit-check — and
   navy (index 61, `(24,28,125)`) otherwise. Cross-checked against all 16
   New Amsterdam cargoes: Sugar/Tobacco/Cotton/Furs/Lumber/Ore/Silver/Rum/
   Cigars/Cloth/Coats green (toggled on); Food/Horses/TradeGoods/Tools/
   Muskets navy (Food/Horses/Tools/Muskets structurally ineligible per
   `europe_custom_house_cargo_eligible`'s denylist; TradeGoods eligible but
   not toggled on in this particular save) — exact match, both hue and
   per-cargo assignment. This is the "per-cargo UI chrome" that
   `europe.h`'s `europe_custom_house_autosell` doc comment had flagged
   PARKed. The this-turn-delta suffix mode (`"159+5"`) is unrelated to any
   golden capture and was left as a single flat color, unchanged.
3. **Colonists blocked from Custom House / Printing Press / Newspaper**:
   these three have no worker slot at all (no `@JOB` entry — Custom House
   is a colony-wide autosell trigger, Printing Press/Newspaper a colony-
   wide bell multiplier), but `colonies_assign_workplace` let a colonist be
   dragged onto them anyway. Fixed with a name-match guard (matches
   `colony_screen_building_production_badge`'s own Printing-Press
   exclusion), applied at the single choke point every assignment path
   (human drag/drop, AI worker placement) already goes through.

## 2026-08-27 fix: Custom House checklist content/style, Production tab intentionally diverges from DOS

Player-reported, two items, both follow-ups on 2026-08-26's work above:

1. **Custom House popup was missing rows and using the wrong color scheme.**
   It listed only `europe_cargo_export_eligible()`'s autosell-denylist
   survivors, so Horses/Tools/Muskets (and Food) never appeared — but
   `col1_save.h`'s `ColonizeCol1CustomHouse` bitfield has all 16 cargoes,
   Food included, so the save format itself treats this as a full
   checklist; the denylist is what `europe_custom_house_autosell()` checks
   at EOT sell time, not what the checklist should offer. Fixed: the popup
   now lists every cargo but Food (15 rows) — toggling Horses/Tools/Muskets
   on is harmless (europe_custom_house_autosell still won't act on them)
   but the row exists like DOS's own bit layout implies it should. Also
   fixed the row style: text was green-when-on/white-when-off; player
   wanted one uniform (darker) color and a real DOS-style checkbox
   instead — GAME.TXT's `@CUSTOM` section literally has an `@checkbox`
   directive. Implemented `colony_screen_draw_bullet()` (an 8-pixel ring,
   filled with 5 more when checked) since this pixel font has no usable
   circle glyph, same gap as the `[`/`]` characters found earlier; rows now
   all draw in one dark green (palette index 2) with the bullet carrying
   the on/off state instead of the text color.
2. **Production tab's surplus split (case 4) now applies to every cargo,
   not just Lumber — a deliberate, explicit departure from DOS pixel-
   fidelity.** Player-confirmed: DOS itself does *not* split Ore or Tools
   here even though the Blacksmith/Armory visibly consume part of each
   (golden: New Amsterdam's Ore reads plain "28", Tools plain "24") — the
   player asked for the split everywhere anyway, as a UI improvement this
   pane specifically opts out of matching DOS for. `used = produced -
   goods[c]`, `stored = goods[c]` (the net warehouse delta already *is*
   what's left over, no separate bookkeeping needed) whenever there's no
   shortfall and some of this tick's production got drawn off by another
   recipe. New Amsterdam: Ore now "24 | 4", Tools now "10 | 14" (previously
   plain "28"/"24"). Lumber's hammers-consumption path stays its own
   special case — `colony_prod_colony_hammers` isn't a
   `colony_craft_preview()` recipe, so it doesn't update `goods[LUMBER]`
   the way a real recipe would, and the generic formula can't see it.
   **This is the one place in the colony screen that's intentionally not
   trying to match DOS's actual on-screen behavior** — everywhere else in
   this file, "golden-confirmed" means pixel-matched; here it means "the
   player explicitly asked for something DOS doesn't do."

## 2026-08-27 fix: Construction tab (BUY alignment, tools line, hammer rows), Artillery construction

Player-reported, five items:

1. **BUY moved up 4px to align with CHANGE** (which had already moved on
   its own follow-up earlier) — both buttons now sit on the same row.
   Hit-test region followed (`[10,26)` → `[6,22)` relative to
   `COLONY_PANEL_CONTENT_Y`).
2. **Artillery is now a real, completable construction project**, not
   display-only. `colonies_unit_build_info()` (colony.h/.c) is the new
   single source of truth for its name/hammers/tools_cost (192H/40T,
   golden-confirmed on the one save that had it queued) — @UNIT's own
   NAMES.TXT row is a Europe *purchase* price, not a colony hammers cost,
   so this pair isn't independently cross-checked against it.
   `colonies_list_buildable()` now offers it (as raw code
   `COLONIZE_UNIT_BUILD_ARTILLERY` = 42, past the real @BUILDING table's
   range — matches how col1_bridge_apply already round-trips this code)
   when the colony has an Armory, Magazine, or Arsenal;
   `colonies_set_construction()` accepts the code under the same gate.
   Completion needed a new function, not a branch in
   `colonies_try_complete_building()`: that one only ever sets
   `has_building[]`, but a unit-type project has to spawn an actual map
   unit, which needs a `ColonizeUnitPool` colony.c's existing building-
   completion path never took (real buildings never spawn anything). New
   `colonies_try_complete_unit_construction(pool, colony_id, units)` — same
   hammers/tools gate as the building path, spawns via
   `units_spawn_allow_stack` + `units_set_nation`, deducts tools, resets
   hammers (which is itself the re-fire guard — a unit is never "owned" so
   there's no `has_building[]`-style dedup to fall back on). Turn-loop side
   needed its own pass too (`turn_run_colony_unit_construction`, called
   right after `turn_run_colony_production` in `turn_processor_advance`) —
   `turn_produce_one_colony` has no units-pool parameter, so it can't reach
   the new function directly the way it reaches
   `colonies_try_complete_building`. Verified end-to-end against
   `dutch-reports.SAV`'s Fort Orange (bip=42, Armory-having): buildable
   list includes it, force-filling hammers to 192 and calling the new
   completion function spawned a real "Artillery" unit at the colony's
   tile under the colony's nation, tools stock dropped by 40, hammers
   reset to 0; a colony without an Armory chain correctly can't select it.
3. **BUY popup** (cost confirm → gold/tools deduction) turned out to
   already exist and work for real buildings
   (`game_request_buy_construction_confirm` / `game_do_buy_construction` /
   `colonies_buy_construction`, wired through `GAME_MAP_CONFIRM_BUY_
   CONSTRUCTION`) — the "still won't act on it correctly" gap the old
   Artillery comment flagged was specifically about *unit* projects, fixed
   alongside item 2 above: `colonies_buy_construction()` now recognizes a
   unit-type project (`colonies_unit_build_info`) and tops hammers/tools up
   to the completion threshold instead of calling
   `colonies_try_complete_building` (which would always fail on a unit code
   — `colonies_building_type` returns NULL for it); `game_do_buy_
   construction` then calls `colonies_try_complete_unit_construction`
   itself right after (it has a `ColonizeUnitPool` `colonies_buy_
   construction` doesn't take), so BUY actually spawns the Artillery in the
   same click for a unit project, same as a real building completing
   immediately.
4. **"(Requires N Tools)" text**: moved up 6px (`py+46` sat 1px past
   `COLONY_PANEL_CONTENT_H`'s bottom edge, clipped by the box) and
   centered horizontally (`font_text_width`-based). Color: grey when the
   colony's tools stock already covers the cost, white when short. Tried
   grey=palette index 7 first — `font.c`'s `draw_ff_glyph` hardcodes
   `color==7` to the exact same "unbold white" AA blend as `color==15`
   (`FF_COLOR_MAP`, comment literally says "ink 15 or 7"), so 7 rendered
   indistinguishable from white; switched to index 8 (plain solid-ink dark
   grey, not special-cased) which reads as genuinely different from the
   white insufficient-tools state.
5. **Hammer counters: four rows of one-fourth the total need each,
   filling row 0 first then row 1 etc. — a deliberate UI improvement, not
   a DOS-accurate change** (same category as the Production tab's surplus-
   split departure noted above: this port is not trying to match DOS pixel-
   for-pixel in this one construction-progress display). Each row's
   capacity is `need/4` with the remainder spread across the first rows
   (so all four capacities always sum to exactly `need`); a row only draws
   once every row before it is full. Verified against two real cases:
   Fort Orange (Artillery, 64/192 hammers) shows a full row 0 (48/48) plus
   a partial row 1 (16/48), rows 2-3 empty; Quebec (College, 156/160
   hammers) shows all four rows nearly full (40/40/40/36). The old single
   packed-row style is still what a barely-started project looks like
   (only row 0 has anything) — same visual, just now conceptually one of
   four quarters instead of the whole bar.

## Final match quality

All six renders (`build/render_colony` against `dutch-reports.SAV`) are a
tight visual match to their goldens after the fixes above: title bar, area-
view field/settlement badges, People band, Production tab, Units tab title,
and Construction tab all match numerically and stylistically. The one
remaining large visual gap is the buildings-section layout positions (first
item above) — content-correct, position-wrong. Everything else left open is
a small, isolated, independently-flagged detail.

## 2026-08-26 fix: Wagon Train construction, hammer-row spacing/z-order

1. **Wagon Train buildable anywhere.** Same `colonies_unit_build_info` /
   `COLONIZE_UNIT_BUILD_*` raw-code scheme as Artillery. NAMES.TXT's
   `@UNIT` table orders Artillery then Wagon Train right after the real
   `@BUILDING` table's last index (41, Iron Works) — Artillery is raw code
   42 (golden-confirmed already), so Wagon Train is 43 by the same file-
   order logic. Cost (40 hammers, 0 tools) is the well-known DOS value, not
   independently re-derived (no save with a Wagon Train mid-construction
   was available to golden-check against, same caveat as Artillery's
   hammers figure). No building gate: `colonies_set_construction` and
   `colonies_list_buildable` admit it unconditionally. Verified via harness
   across all 17 colonies in `dutch-reports.SAV`: listed as buildable in
   every one, `set_construction` succeeds, spawns correctly through the
   existing generic `colonies_try_complete_unit_construction` /
   `turn_run_colony_unit_construction` path (no changes needed there — both
   were already written generically off `colonies_unit_build_info`).
2. **Hammer-row area 3px lower.** Player-reported: row 0 was overlapping
   the BUY/CHANGE buttons above it. `area_y` moved from `py+16` to `py+19`.
3. **Hammer count number drawn on top, not behind.** It was drawn before
   the icon-row loop, so row 0's icons (which can start right at the
   pane's left edge, under the number) painted over it. Moved the number
   draw to after the loop.
4. **Incomplete rows no longer stretch to fill the pane.** The per-icon x
   position was `px + (i * span) / (filled - 1)` — denominator was the
   *current* fill count, so every new hammer re-spread the whole row across
   the full pane width (a bar redrawing itself wider each tick, not a
   progress bar). Denominator changed to `row_capacity - 1` (the row's
   fixed total slot count), so positions are stable: hammers now pack in
   from the left and only reach the right edge once that row is actually
   full. Pixel-verified on New Amsterdam (32/192 Artillery, row 0 capacity
   48): filled pixels run from the pane's left edge to ~65% across, not to
   the far edge.

## 2026-08-26 fix: hammer-row numbers now consistent across all 4 rows

Player-reported: numbers should be all-or-nothing across rows, not mixed.
Previously only one number was drawn — the running total, always pinned at
row 0's position regardless of what row 0 actually held (misleading when
row 0 was already full and the total exceeded it, as with Fort Orange's
64/192 Artillery: row 0 read "64" while only holding 48). Replaced with a
single density check for the whole bar (`max_row_capacity * iw > pane_w`,
i.e. would that many hammer icons overlap past legibility) computed once
from the fullest row; if dense, every row with `filled > 0` shows its own
count as a number instead of icons — if not dense, every row stays
icons-only, no numbers anywhere. Verified: Fort Orange's 64/192 Artillery
(row capacity 48, dense) now shows "48" then "16", one number per non-empty
row; a 40-hammer Wagon Train project (row capacity 10, 10×8px=80px fits the
92px pane, not dense) stays pure icons.

## 2026-08-26 fix: dense-row numbers were replacing icons, not overlaying them

Player-reported regression from the previous fix: the dense branch skipped
icon drawing entirely (`continue` right after the number), so a project
dense enough to need numbers lost its hammer icons altogether. Fixed:
icons now always draw for every row regardless of density; the per-row
number (when dense) is drawn in a second pass afterward, on top, same
z-order reasoning as the earlier "number in front of icons" fix.

## 2026-08-26 fix: overflow hammer rows, CHANGE popup hammer adjustment, BUY popups on refusal

1. **Hammers stored beyond the project's requirement fill all four rows.**
   Already correct by construction (`show` clamps to `need`, and the four
   row capacities sum to exactly `need`) — verified with a 250/192 override
   (Fort Orange, Artillery): all four rows read "48". No code change.
2. **CHANGE popup hammers now show what's still needed, not the raw
   requirement.** `colony_screen_draw_construction_popup` took `colony` but
   never used it (`(void)colony`). Now subtracts the colony's banked
   `hammers` from each listed option's requirement (min 0) — tools are
   never adjusted this way, since (per point 3 below) DOS never lets gold
   or banked hammers substitute for missing tools. Verified against
   NAMES.TXT raw costs at hammers=32 (Fort Orange): Fort 120→88, Magazine
   120→88, Schoolhouse 64→32, Warehouse Expansion 80→48, Weaver's/
   Tobacconist's Shop 64→32, Fur Trading Post 56→24, Church 64→32,
   Artillery 192→160, Wagon Train 40→8 — all match raw−32 exactly.
3. **BUY now always raises a popup, matching what DOS actually does —
   not a hammers+tools sum.** Checked GAME.TXT directly: DOS has *two*
   pairs of message keys for this button, and this port's interactive BUY
   path had wired neither for the failure cases (status-bar text only,
   silently). `@NEEDTOOLS`/`@NEEDTOOLS0` — tools shortfall blocks Buy
   outright, gold is never a substitute (same key turn.c's EOT "hammers
   ready, tools short" notice already used); `@BUYME0`/`@BUYME1` — gold
   cost is hammers-only (unchanged formula), `@BUYME0` is the informational
   (OK-only) sibling of the existing `@BUYME1` Yes/No confirm, used when
   gold is short instead of a silent refusal. `game_request_buy_construction_confirm`
   now raises the matching popup on every path (tools-short → NEEDTOOLS/0,
   gold-short → BUYME0, affordable → BUYME1 as before).

## 2026-08-26 correction: BUY is one uniform Yes/No, tools-price summed into gold cost

Player correction to the previous entry's GAME.TXT read: `@NEEDTOOLS`/
`@NEEDTOOLS0` is not what the interactive Buy button uses — that pair
stays exactly where it was (turn.c's EOT "hammers ready, tools short"
notice). Buy itself is uniform regardless of a tools shortfall: one
`@BUYME1` Yes/No ("Complete it." / "Never mind.") when affordable, or the
informational `@BUYME0` sibling when not — never a different popup shape
for the tools-short case. Cost is hammers-remaining (1 gold each,
unchanged) *plus* tools-remaining priced at the colony's current Europe
buy/ask price (`europe.cargo[COLONIZE_CARGO_TOOLS].ask`), summed into one
number. Confirming ("Complete it") now tops the colony's tools stock up
to the requirement (paying the gold for it) before completing, instead of
refusing outright when tools were short. `game_do_buy_construction` and
`game_request_buy_construction_confirm` both updated to match; the
tools-short-blocks-Buy-outright branch (info popup, no purchase path) is
removed from both.

## 2026-08-26 fix: BUY popup wrong-palette background, completion confirmation

1. **BUY popup background colors wrong.** The colony-screen `ai_popups` render
   fix (previous entry) used the map screen's wood tile (`menu_opentile` /
   `map_panel.wood_tile`) — a sprite sheet remapped for the *map's* palette,
   not the colony screen's. Since this framebuffer gets expanded through
   the colony screen's own palette, those indices painted the wrong RGB.
   Switched to `game->colony_screen.wood_tile` (already remapped for that
   palette) when `game->in_colony`.
2. **"Only tops up hammers/tools, builds next turn" — was already instant,
   just silent.** Harness-verified (`colonies_buy_construction` →
   `colonies_try_complete_building`/`colonies_try_complete_unit_construction`):
   `has_building[]`/spawn and `hammers=0`/tools-deducted all happen
   synchronously inside the same click, same as a natural EOT completion.
   What was missing: EOT completion raises turn.c's `@BUILT` popup
   ("X colony produces Y."); BUY only set a quiet status-bar line, easy to
   read as "nothing happened, must finish next turn". `game_do_buy_construction`
   now raises the same `@BUILT` popup on success.
3. **Sidebar click-to-End-Turn + flashing prompt (View Pieces mode).** New:
   once `turn_select_next_unit` comes up empty (no unit needs orders) and
   `game_options.end_of_turn` is on, the port had no way to actually confirm
   the end of turn at all — `game_wait_next_unit` just kept re-setting the
   "End of Turn" status text forever, since it only reaches `game_do_end_turn`
   when that option is *off* (auto-end). `map_panel_render` gained an
   `end_turn_flash_on` param (caller-resolved bool, ~2.5Hz blink) drawing
   "End Turn" in the sidebar's unit-info slot when no unit is selected; a
   left-click anywhere in the sidebar (that isn't a minimap hit) while the
   prompt is active now calls `game_do_end_turn` directly — the click itself
   is the confirmation the option demands.

## 2026-08-27 correction: BUY defers completion to next turn; real gold formula; End Turn prompt fixed

1. **BUY is not instant — player-corrected, reverted the previous "instant
   complete" behavior.** `colonies_buy_construction` no longer calls
   `colonies_try_complete_building` / spawns a unit; it only tops hammers to
   the threshold and tools to the requirement (matching DOS's own
   `FUN_2f2b_5e44`, which does the same — verified by reading its clean
   decompile, no completion/spawn call anywhere in that function). New
   `turn_run_colony_building_completion` (sibling to the existing
   `turn_run_colony_unit_construction`) runs unconditionally every turn's
   EOT SETUP and completes any colony already sitting at/above threshold —
   covers a BUY-topped project next turn, and also fixes a latent gap where
   `turn_produce_one_colony`'s inline completion check only fires on a tick
   that *adds* new hammers (misses an idle-Carpenter or Autumn-frozen
   colony already at threshold). Harness-verified: buy tops Fort to
   120H/100T without setting `has_building`; a follow-up call to the new
   pass then completes it and zeroes hammers.
2. **Real gold formula — was "severely underestimated".** Read
   `FUN_2f2b_5e44`'s clean decompile
   (`original_sources_decompiled/viceroy_unpacked.c:52683`) directly:
   `hammers_deficit × 13`, plus `tools_deficit × (per-nation table byte +
   4)` when tools are short, the whole sum **doubled** if the colony hasn't
   banked any hammers at all yet (`colony->hammers == 0`) — a steep premium
   for rushing an unstarted project. The ×13 rate and doubling rule are
   read straight off the disassembly (high confidence). The per-nation
   tools-price table byte itself (`nation[id] + 0x13c×idx − 0x779e`, an
   unresolved external thunk) couldn't be pinned to a named field with
   confidence in the time available — approximated as `difficulty + 4`
   (this port's existing 0-8 difficulty byte), same shape/magnitude as the
   confirmed term, clearly flagged as approximate in code comments. Also
   fixed `hammers_purchased` (Col1 +0x98) to accumulate the hammers
   *deficit* (matches DOS: `local_c = hammers_need − hammers`), not the
   gold spent — those two only used to be numerically equal back when the
   rate was a flat 1:1.
3. **Sidebar "End Turn" prompt not appearing.** The previous fix gated it on
   `turn_human_units_exhausted`, which only checks `moves_left > 0` — a
   colony with Fortified/Sentried units (moves_left>0, but never actually
   offered for selection) made it report "not exhausted" forever, so the
   prompt never showed even in a genuine View Pieces "nothing left to
   control" state. Replaced with a non-mutating scan
   (`game_units_pending_orders`) mirroring `turn_select_next_unit` +
   its guard loop's standing-order skip (`units_orders_skip_turn`) exactly,
   without turn_select_next_unit's side effect of changing the unit
   selection.

## 2026-08-27 fix: End Turn prompt moved to sidebar bottom, flashes white/black

Player-requested: 1) pinned to the bottom of the sidebar instead of inline
with the flowing unit-info text; 2) flashes white(15)/black(0), always
drawn while active — not a show/hide blink like before. `map_panel_render`
split the one `end_turn_flash_on` bool into `end_turn_active` (show at all)
+ `end_turn_blink_white` (color phase), drawn as its own block pinned to
`framebuffer->height - line_h - 2`, after all other sidebar content.
