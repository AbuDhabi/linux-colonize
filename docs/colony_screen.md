# Porting the Colony screen (multipurpose view) to golden fidelity

How the `colony_screen*` family (`colony_screen{,_draw,_buildings,_panels,_popups}.c`,
seams in `colony_screen_internal.h`) was aligned to six DOS reference
screenshots — New Amsterdam and Recife (Dutch) × three `ColonyMultiMode`
tabs — from `original_saves/report-screen-goldens/dutch-reports.SAV`.
Follows the methodology in [report_screens.md](report_screens.md); read that
first. This file is compressed to verdicts + traps (2026-09-05); the full
session narratives are in git history.

## Tool

`build/render_colony <data_dir> <save.SAV> <colony_name> <multi_mode> <out.ppm> [rects]`
(`tools/render_colony_main.c`) — loads a Col1 save and LABELS.TXT, bridges it, calls
`colony_screen_render()` directly (no SDL/xvfb). `multi_mode`: 0 Production,
1 Units, 2 Construction. Optional 6th arg = debug building rects. Golden
workflow (2x goldens, divide coordinates by 2, `-filter point` crops) is
identical to [report_screens.md](report_screens.md).

Worktree note: `COLONIZE/` is gitignored — symlink it in from the main
checkout (`ln -s <main-checkout>/COLONIZE COLONIZE`).

## Verified facts and fixes (goldens)

### Top bar

DOS (`FUN_2f2b_0fce`) draws one centered green (`WOODPANL.PIK` idx 68,
RGB (85,150,52)) string: `"<Name>.  <Season>, <Year>.  Gold: <N>$"` —
periods after name and year, comma after season, double space after periods.
Built locally in `colony_screen_draw_top_bar` (other screens keep the plain
`turn_format_date()`). `COLONY_TOP_BAR_H` = 7 (separator native y=7);
`COLONY_MIDDLE_Y` etc. derive from it.

An armed DOS status line (`DS:0x2d54`, `FUN_1009_02cc`) paints over this same
top 7 px strip: the `U` unload line ("Unloading 39 Furs in Quebec", gold ink)
and the red refusal (@MISC 22 "No cargo on board!"). The port feeds them through
the `ai_popup` status ring: `game_service_colony_bar` copies the line in front
into `ColonyScreenView.bar_text` / `bar_ink` each frame and
`colony_screen_draw_top_bar` draws it instead of the title line (port_bugs.md
#1182). Nothing blocks, as on the Europe screen.

### SoL bonus belongs in every badge

Area-view field-tile badges and settlement (per-building) badges both used
to pass `sol_bonus=0` "by design" — a design note written before any Colony
golden existed. Golden proves both wrong: every badge is off by exactly the
SoL bonus. Both paths now fold `colony_prod_sol_bonus_field` +
`colony_flags` in; manufacturing/Town Hall/Church/Carpenter badges read
`view->preview.craft_gross[]` / FF-folded crosses/bells totals, not
`colony_prod_building_display_output_sol`'s smaller local estimate. All field
numbers on both goldens match exactly. **Lesson:** a doc claim "matches
golden" written before that golden existed is unverified.
`colony_screen_blit_buildings()` also simply had no `font` param (passed
NULL), so no badge number ever drew at all — verify *content*, not just
position.

### Production tab shows gross per tier, not net

DOS shows each tier's own gross output (Ore 28 even though the Blacksmith
consumes 24 of it). Added `field_gross[]` + `craft_gross[]` to
`ColonizeColonyPreview` (accumulated in-loop — a post-loop `memcpy` of
`goods[]` double-counts town commons). Food is excluded from this pane
(shown on the People band). Town-commons secondary yield shows only on the
minimap center badge.

**Cell grouping (player-specified, golden-checked):** plain number;
grey/red short number; produced-but-short = white + red pair; surplus =
used + stored white pair with spacer. Lumber→Hammers is special-cased
(`colony_prod_colony_hammers` is not a craft recipe). **Deliberate
divergence from DOS:** the surplus split applies to every cargo (DOS shows
Ore/Tools as plain numbers) — player asked for it as a UI improvement. This
and the 4-row hammer bar are the only intentional non-DOS displays on this
screen.

### People band

Line-up is `FUN_2f2b_17d0` verbatim (`colony_screen_people_layout`): y=0x8e,
x from 2; colonists, then only the DS:0x8d72 fence units (`FUN_15eb_08e6`,
DS:0x30e[type] >= 0, so no Artillery / Treasure); pitch starts at 2 and
shrinks until the row is < 0x60 wide, with 4px extra after the last colonist.
Hit-test twin: `FUN_2f2b_41c0`.

Food meter reads `p->goods[FOOD]` (post-horse-breeding), not
`p->food_produced` (pre-breeding) — golden showed the exact breed-amount
gap. Shortfall branch keeps `food_produced` (wants raw pre-consumption).

### Badge widget (final verdict, after one reverted misreading)

Resource-count badges blit `amount` real copies of the icon, evenly spread
and mostly overlapping (`colony_screen_draw_icon_strip` style) — what looks
like a painted black pill in goldens is overlapping black-bordered icons,
and it scales with amount. The number is drawn unconditionally.
**Trap:** an intermediate pass concluded "one static icon + painted black
rect" from the numbered goldens alone; numberless reference captures
(`*_numberless.png`, "always show numbers" off) disproved it — the toggle
affects only the area-view field-tile yield numbers. Don't backport
`reports.c`'s proportional icon bars here or this style there. The one
genuine per-unit proportional tally on this screen is the Construction
tab's hammers bar (own block). Production-tab badge ink is white (15), not
10.

### Construction tab

- Title/number ink: `WOODPANL.PIK` idx 57 dark blue, centered (same as
  Units title).
- **`building_in_production` decode (real save bug, `col1_bridge.c`):** raw
  code IS the `@BUILDING` file-order index directly. A pre-golden special
  case remapped raw 6 to "Stockade"; Recife's golden reads "Docks" (index
  6) — special case removed both directions. It silently mis-tracked any
  Docks project as Stockade, including completion.
- **Buildable units past the building table:** Artillery = raw 42
  (192H/40T; 40T golden-confirmed; gated on Armory/Magazine/Arsenal),
  Wagon Train = 43 (40H/0T, ungated). `colonies_unit_build_info` is the
  source of truth; completion via
  `colonies_try_complete_unit_construction` (spawns a real unit —
  buildings never spawn, so it's a separate function) driven by
  `turn_run_colony_unit_construction`. NAMES `@UNIT` cost column is the
  Europe purchase price, not the hammers cost.
- **Hammer bar:** four rows of `need/4` each (remainder on first rows),
  fill left-to-right, stable icon positions (`row_capacity - 1`
  denominator — a `filled - 1` denominator re-spreads the bar every tick).
  Dense rows (icons would overlap past legibility) overlay a per-row count
  number on top of the icons — never instead of them, and all-or-nothing
  across rows. Deliberate UI improvement, not DOS-accurate.
- **CHANGE popup** shows remaining cost (requirement − banked hammers,
  min 0); tools never adjusted. Row text is `FUN_2f2b_5a68`'s:
  `<NAME> |   (N Hammers)(M Tools)`, the name upper-cased (FUN_1d1d_0d64), no
  space between the groups (the DOS screenshot's gap is the glyphs' own
  bearing, pixel-equal to the port's `)(`), the tools
  group only when tools are due, and `(` @MISC 127 `)` for a building the
  colony already owns (port_bugs.md #1202; user 2026-10-23 "switch to DOS",
  superseding #436's `N Hammers M Tools`). Rows go through the dialog writer in the box
  style (@COLORS 68; the current project's row starts in the hilite ink 149,
  `FUN_2f2b_5bd2` raw 52607-52610). Past 22 rows it is exactly two pages of 16:
  page 0 ends with "(More)", page 1 starts with the back "(More)" (port_bugs.md
  #1195). The bar (port_bugs.md #1215, `FUN_2f2b_5bd2`): the list always opens on
  page 0; a page that lists the current project bars it and draws it in the hilite
  ink; a page without it, first showing or turned, does that to the "(More)" row
  toward it (page 0 `08ec(0x65)` / `033c(0x65, 1)`, page 1 `08ec(100)`). DOS passes
  the More ids 0x63 / 0x62 + 2 there, ids no row has, so it shows no bar (Enter does
  nothing), and since it never resets `local_8` a turned page bars its first row
  instead (dos_bugs.md DB-63, DB-60). A live list pages once the built current project keeps its row
  (FUN_15eb_3650 tail, port_bugs.md #1218; unit_stage_seams
  test_colony_construction_two_pages_live). Picking that "(COMPLETE)" row keeps the project and
  clears +0x1c bit 0x80 with no message (FUN_2f2b_5bd2 raw 52665-52667; @ALREADYHAVE stays the
  EOT notice), and the "(COMPLETE)" test is the DOS +0x84 bit (FUN_281f_09fc,
  `colonies_dos_owns_row`), which a built Warehouse Expansion never has: its row reads its cost
  (port_bugs.md #1223).
- **"(F1 for Help)"** (`FUN_2f2b_5bd2` raises DS:0x1f66, raw 52632): F1 takes the barred
  row, a right-click the row under the pointer (`FUN_6f74_2580` sets DS:0x1f68). "(More)"
  still turns the page; a building row opens its Colonizopedia article (`FUN_291f_0902`), a
  unit row its unit article (`FUN_291f_0942`, `@UNIT` row via `units_build_code_to_index`),
  the clear row nothing; the list is rebuilt on the same page and comes back when the
  article closes (raw 52648-52658). Port: `game_colony_construction_help`
  (`game_loop_update.c`). The jobs and leave-as lists (`FUN_2f2b_348c`, raw 50823) work the
  same way but open the skill article of the row's `@JOB` (`FUN_291f_08de`; a leave-as role
  is `@JOB` 0x13 + role, the stay row the colonist's current job), "Clear Specialty" just
  re-shows, and the list is rebuilt on page 0 with its default bar (raw 50893-50901):
  `game_colony_list_help`.
- **BUY / CHANGE buttons** are `FUN_2f2b_21da` -> `FUN_2f2b_208c` literally:
  at (0xd8, 0x8a) / (0x10e, 0x8a), label width + 5 by 9, WOODPANL 0x30 bottom
  and right drawn first, then 0x39 top and left, the label at (+3, +2) in 0x0f
  with no hotkey ink. 0 differing pixels in both 30x9 button rects of
  `new_amsterdam_construction.png` and `recife_construction.png` (was 90 / 139
  with the old RGB-matched blues, port_bugs.md #1124). The dead-queue CHANGE blink
  (white fill, black text) is port-only.
- **BUY (`FUN_2f2b_5e44`, player-corrected twice):** one uniform `@BUYME1`
  Yes/No when affordable, `@BUYME0` info when not — never a
  tools-short-specific popup (`@NEEDTOOLS*` stays turn.c's EOT notice).
  Cost = `hammers_deficit × 13` + `tools_deficit × (nation's live Europe
  Tools price + 4)`, **doubled when `colony->hammers == 0`**. The price byte
  is nation record +0x5a; the old `difficulty + 4` approximation is retired
  (carpenter audit, port_bugs.md #917) — formula owner is
  [colony.md](colony.md) `colonies_construction_gold_cost`. BUY only tops up
  hammers/tools — completion happens next turn via
  `turn_run_colony_building_completion` (DOS's 5e44 has no completion
  call either). That pass also fixed a latent gap: the inline completion
  check only fired on ticks that added hammers. `hammers_purchased`
  (Col1 +0x98) accumulates the hammers *deficit*, not gold.
- BUY popup on the colony screen must use `colony_screen.wood_tile`
  (colony palette), not the map's.

### Units tab

"Units Present" title = `LABELS.TXT @CMISC` index 1, centered, idx 57.
`colony_screen_render` takes a `labels` param (NULL falls back to the
literal); layout shared between draw and hit-test via
`colony_screen_multi_units_layout()` so click regions can't drift.

### Fence strip / minimap / misc

- Fortification strip admits only on-tile units with a profession slot
  (`DS:0x30e[type] >= 0`, FUN_2f2b_11b2 raw 47899-47906). Treasure and
  Artillery are excluded from fence draw and hit-test; the Units tab and
  People-band outside row still keep them.
- Assigning a fence colonist directly to an empty area tile derives the
  default from that destination: Fisherman on water, Farmer on land. The
  temporary job chosen while admitting the outside unit is not retained.
- The docked-unit orders popup uses `@SHIPOPTIONS` for ships and Wagon Trains,
  and `@UNITOPTIONS` for ordinary land units. Artillery therefore receives
  the land `Fortify` label rather than `Anchor in harbor`.
- Building badge name match: `strstr("Carpenter")` missed "Lumber Mill" —
  upgrades rename buildings; match both.
- Minimap has a 1px black border exactly on the 73×73 grid box.
- Parchment fill extent is `COLONY_PARCH_FILL_W/H`, computed from the
  section/separator constants (the old hardcoded 202×114 went stale when
  the top bar shrank).
- Dock-corner placeholder (#45 trees+shore) draws in **every** colony,
  inland included — it's generic unbuilt filler, not a water cue
  (player-verified vs DOS; `coastal` gate removed).

### Cargo strip (bottom)

Icons at `COLONY_CARGO_STRIP_Y + 1`, numbers at `COLONY_CARGO_NUM_Y + 2`.
Digit colors are two independently-colored runs: a 3-digit stock's hundreds
digit always gold (idx 148), remaining digits green (10) when the cargo is
toggled on in the colony's Custom House mask
(`europe_custom_house_cargo_enabled()`) else navy (61). Verified against
all 16 New Amsterdam cargoes.

### Custom House popup

Click on the building opens a per-cargo checklist
(`colony_screen_open/draw_custom_house`): all 16 cargos, Food first
(`FUN_2f2b_4424` loops `local_8 < 0x10`; the autosell denylist applies at EOT
sell time, not in the list), `@checkbox` rows `] Sugar` / `[ Sugar` (FONTTINY's
`]` / `[` are the filled / hollow circles), one uniform ink (player request:
not the old bright green) which is the box style's @COLORS 68 (port_bugs.md
#1200), row click toggles
`colonies_toggle_custom_house_cargo()`, stays open. It is a `FUN_6f74_2580`
@checkbox box with a selection bar (port_bugs.md #1213): row 0 at open, Up / Down
wrap, Space / F1 / a click flip the barred row, Enter / Esc / an outside click
close it, other keys do nothing ([popups_dialog_engine.md](popups_dialog_engine.md) "Custom House keys"). Colonists cannot be
assigned to Custom House / Printing Press / Newspaper (no `@JOB` slot) —
guard in `colonies_assign_workplace`.

### Jobs / leave-as popups (`FUN_2f2b_348c`)

Row text is DOS's: @JOB column 0, the `|   ` column tab, then for jobs below
0x13 (not 0x12) ` = ` + [the yield on the colonist's own plot + `/`] + the
estimate + the @CARGO name, right-aligned to `frame_x + w - 13`. FONTTINY has no
`=` glyph, so the ` = ` shows as two spaces (port_bugs.md #1225, popups_dialog_engine.md).
Param 0 lists @JOB 0..0x18: after the jobs come the leave-as rows 0x13..0x18 (same text and
gating as leave-as mode below, short-stock rows greyed and inert), so with the starting houses
the 16 job rows fill page 0 and the leave-as rows land on page 1: "(More)" flips between the
job list and the leave-colony list, and a leave-as pick there is the leave-as popup's pick
(port_bugs.md #1224; `game_colony_commit_job` -> `game_colony_request_eject`). The estimate is
`ai_euro_jobs_menu_estimate`: a field job is the yield of the plot
`FUN_15eb_28c8(slot, job)` would pick, an indoor job `FUN_15eb_1d4c` (Statesman
plus Jefferson / Paine / press). The specialty's row is drawn in the hilite ink,
Clear Specialty is page 0's first row, the default row is the current
occupation, and more than 16 job rows make two pages (port_bugs.md #1195,
#1196). The prose has no space before the "(": DS:0x5e is `(`.

Opening it (param 0): a click on the selected colonist's plot (`FUN_2f2b_3fa6`),
a click on the selected worker in a building (`FUN_2f2b_44d4` raw 51280-51286;
the building strip hit-tests as `COLONY_HIT_COLONIST`, and a press-release on
the same worker that was already selected is the click), or Enter on the
selected colonist (`FUN_2f2b_6372`). A colonist who works no plot opens it with
`jobs_tile_index` -1. A field pick keeps a plot worker on his plot; a colonist
working indoors is seated on the plot `ai_euro_jobs_menu_plot` elects
(`FUN_15eb_28c8(slot, job)`, raw 50860-50863), and when none scores he keeps the
field job with no plot until the next `colonies_recompute_plots_w`. An indoor
pick goes through `colonies_assign_workplace` (port_bugs.md #1204). In DOS
opening it is not free (348c's build-up seats stamp each elected plot and zero the
job counter, Escape or not, port_bugs.md #1205); the port's opening changes nothing
(docs/dos_bugs.md DB-54).

Leave-as mode (param 1, the fence drop): page 0 starts with the colonist's
current occupation (bare @JOB name, the default row; picking it keeps him,
`COLONY_EJECT_ROLE_STAY`), then the roles 0x13..0x18 with the tab and, per
`FUN_15eb_0d8e` cargo, ` (50 Muskets)` / ` (50 Horses)` / ` (N Tools)` with
N = clamp(stock / 20 * 20, 20, 100) after a fence Pioneer's own tools went in;
a fence unit's own role row has no gear part and is the default
(port_bugs.md #1203).

### Shared sprite blitting: `unit_chrome_blit` 3-mode component

DOS shadow convention is 2px-left black silhouette
(`UNIT_CHROME_SHADOW_DX`). Instead of per-screen ad-hoc blits, every
unit/colonist sprite draw uses `unit_chrome_blit(...)` with one of
`UNIT_CHROME_PLAIN_SPRITE` / `SPRITE_WITH_SHADOW` / `SPRITE_ORDERS`
(ORDERS = the old `unit_chrome_blit_unit_colored` path, shared impl).
Colony screen strips/People band/minimap workers and `reports.c`'s three
hand-rolled shadow pairs all migrated; renders unchanged.

### End Turn prompt (sidebar)

When no unit needs orders and `game_options.end_of_turn` is on: "End Turn"
pinned to the sidebar bottom, flashing white(15)/black(0)
(`end_turn_active` + `end_turn_blink_white` params); any sidebar click
that isn't a minimap hit confirms via `game_do_end_turn`. Pending check is
`game_units_pending_orders` — a non-mutating mirror of
`turn_select_next_unit` incl. standing-order skips (the older
exhausted-units test looked only at `moves`, so it wrongly counted
Fortified/Sentried units as pending).

### Building hover label + right-click pedia

`FUN_2f2b_44d4` has two arms behind the same slot scan. The one this port
already had is the assign/click arm; the other fires when `DS:0x7f6 == 0`
(pointer move, no drag): it caches the hovered building in `DS:0xb9a`
(`DS:0xb9b` = a label is up, so it can be erased), and calls `FUN_2f2b_05ee`,
which concatenates the building's name string (table at `DS:0x8f82`, stride
12), measures it (`FUN_281f_0204`), clamps `x = slot_x + [0x230+cls]/2 -
(w+1)/2` into the settlement panel and draws it in ink 15.

The Ghidra output loses `FUN_2f2b_05ee`'s geometry; the ndisasm body
(`OVL03_L0000:05ee`) has it:

```
bw = [cls+0x230]; bh = [cls+0x236]; w1 = strwidth(name) + 1; h = 7;
lx = x + bw/2 - w1/2;
ly = y;                                    /* caller passes slot_y + 8 */
if (cls == 2 || cls == 4 || building < 0) ly += bh/2 - h/2;
lx = clamp(lx, 0, 0xc7 - w1);              /* 0xc7 = 199 */
fill(lx, ly, w1, h); text(lx + 1, ly + 1, name, ink 15);
```

So the label sits on the hovered building's **top edge** — not the panel's top
row — and only the two wide size classes (2 and 4) drop it to the middle of
the box. The caller's `+8` is this port's `COLONY_VIEWPORT_Y` origin.

Ported as `ColonyScreenView.hover_building_row` (set each frame in
`game_update_colony_screen` from `colony_screen_hit_test`, cleared while any
sub-dialog or drag is up) plus a tail block in `colony_screen_blit_buildings`.
Only the backdrop fill colour is approximate — DOS's is a windowed save-under
blit through `DS:0x2da8..0x2dae`, not re-measured against a golden.

Right-click on a built building opens its Colonizopedia article
(`PEDIA_CAT_BUILDING`; `@BUILDING` row order *is* the pedia building index)
and returns to the colony screen on exit, matching the area-view tile's
right-click terrain article. Unbuilt slots are not hoverable or right-clickable
— the slot scan runs past them (port_bugs.md #430).

### F1 on the bare screen (DS:0x32e focus)

`FUN_2f2b_6372` raw 55622-55650 (asm 2f2b:6408) switches F1 on `DS:0x32e`, the panel
the last press landed on (`ColonyScreenView.focus`, never reset between colonies),
opens one article and redraws the colony (`thunk_FUN_291f_0684`):

| focus | set by (press) | article |
|---|---|---|
| 0 area | plot, `FUN_2f2b_47bc` raw 51373-51376 (cursor `DS:0x330/0x332` = `area_dx/dy` + 2, static 2,2) | terrain of the cursor cell unless its `DS:0x8df0` plot byte is 0x10 (`FUN_13e4_003a`, `FUN_291f_0428`) |
| 1 colonists | another colonist or fence unit, `FUN_2f2b_44d4` raw 51248-51260 | skill of the occupation (`FUN_15eb_0e52`; a fence unit's profession), 0x1c read as 0x13 (`FUN_291f_08de`) |
| 2 ships | ship (`FUN_2f2b_548e` raw 52222) or hold box (`FUN_2f2b_56ce` raw 52300) | with `DS:0x33c` ships in port: the selected ship's unit article (`FUN_291f_0942`) |
| 3 Units pane | `FUN_2f2b_59a0` raw 52469 | with `DS:0x8d72` units on the tile: the selected unit's |
| 4 warehouse | cargo, `FUN_2f2b_60dc` raw 52848 | cargo article of `DS:0x33a` (`FUN_291f_0934`) |

Leaving the Units tab turns focus 3 into 1 (raw 51475, 55565, 55594). The right-click
terrain article shares the 0x10 gate (`FUN_2f2b_47bc` raw 51399). The construction /
jobs / leave-as lists keep their own F1. Port: `game_colony_screen_help`
(`game_loop_update.c`); DOS's Tab focus cycling (raw 55535-55545) and the per-panel
cursor keys are not ported, so only presses move the focus.

### Debug: Building Rects

`MAP_MENU_ACTION_DEBUG_BUILDING_RECTS` (DEBUG pulldown, `debug.building_rects`,
port-only) outlines every building sprite's bounds in violet (13).
Placement is clickable, so bounds matter beyond looks.

## Building placement — history compressed

A long placeholder arc (measured per-colony override tables, salted-seed
RNG pools, reserved corners, cross-group overlap rejection, HIDDEN slots,
layout reuse across colonies — four player-caught follow-up fixes) is fully
**superseded** by the real DOS tables below; details in git history.
Lessons worth keeping:

- The "tables are unrecoverable without live memory" conclusion was wrong —
  they are plain initialised DS data at EXE file offset `121248 + addr`.
  Check that before declaring anything live-only.
- `FUN_SSSS_OOOO` names are not literal segment:offset for overlay code —
  `FUN_2f2b_0434` is `OVL03_L0000+0x434` (see `tools/address_mapping.csv`);
  a breakpoint on the literal address never fires, and this game's overlay
  manager doesn't use `INT 21h AH=4Bh`.
- Box-based overlap rejection structurally can't match DOS sprite packing
  (transparent margins); don't re-attempt without pixel masks.

## The real DOS slot tables (recovered and ported 2026-09-04)

Found while porting the Warehouse Expansion badge (port_bugs.md #377). All static
DS data in `VICEROY.EXE` at file offset `121248 + addr`.

`FUN_2f2b_171c` draws the buildings section: **15 slots**, each at
`DS:0x266 + slot*4` as an `(x, y)` word pair, drawn at `(x, y + 8)`;
`DS:-0x717e[slot]` = building index, `DS:-0x729e[slot]` = size class; each
slot via `FUN_2f2b_14d4(building, x, y+8, class)`.

| slot | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 |
|------|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| x | 56 | 145 | 173 | 8 | 37 | 67 | 96 | 6 | 128 | 10 | 15 | 87 | 66 | 123 | 123 |
| y (pre +8) | 5 | 7 | 10 | 33 | 37 | 46 | 45 | 6 | 45 | 68 | 94 | 3 | 79 | 98 | 47 |

Slot 13 → `(123,106)` fence/stockade corner, slot 14 → `(123,55)` dock
corner — exactly the two positions the old template matcher recovered.

Per size class (NAMES.TXT `@BUILDING` column 4, 0..4), byte arrays of 5:

| class | 0 | 1 | 2 | 3 | 4 | what |
|-------|---|---|---|---|---|------|
| `DS:0x230` | 23 | 44 | 53 | 73 | 75 | box width |
| `DS:0x236` | 27 | 22 | 37 | 18 | 48 | box height |
| `DS:0x23c`/`0x242` | 3,12 | 20,8 | 25,22 | 5,5 | 0,0 | DOS overlay dx,dy |
| `DS:0x248` | 17 | 21 | 25 | 65 | 0 | |
| `DS:0x24e`/`0x254` | −5,−3 | 0,1 | 0,1 | 0,1 | 0,1 | production-badge dx,dy (`FUN_2f2b_14d4`; workers use 0x23c/0x242) |
| `DS:0x25a` | 20 | 22 | 30 | 20 | 20 | production-badge width = min_w |
| `DS:0x260` | 45 | 44 | 43 | 0 | 46 | empty-slot placeholder sprite |

The port's fence-unit rectangle keeps DOS's class-3 horizontal anchor (+5)
but uses y+3 for the top-left screen rect: raw y+5 would put the bottom row of
the 18px strip on the settlement's bottom separator.

Class 3 (73×18) / class 4 (75×48) match `COLONY_FENCE_W/H` /
`COLONY_COAST_W/H` exactly — that pins the row order. The building sprite
blits at the slot coordinate with **no** class offset (`FUN_281f_0254`,
sheet `DS:0x2da8`).

### Warehouse Expansion / Capitol level badge

`FUN_2f2b_14d4` tail: building `0x0f` reads colony `+0x95`
(`warehouse_level`), `0x1e` reads `+0x96` (`capitol_level`); value > 1
prints as white (0x0f) decimal at
`(x + width[class]/2 − 1, y + height[class]/2 − 3)`. Ported
(`k_class_box`). The Capitol half can never fire — DOS refuses the
building ([building_production.md](building_production.md)).

`+0x95`/`+0x96` are level counters (0..2) and the **only** record of the
upper tier — no building-word bit exists for Warehouse/Capitol Expansion.
`col1_bridge.c` used to `max()` the counter into the raw mask (stored 3,
read back as a 400-slot warehouse) — corrected; see
[save_format_map.md](save_format_map.md).

### Categories, positions and the shuffle (`FUN_2f2b_0434`)

42 `@BUILDING` rows group into **15 categories** via `FUN_75c2_144c`
(42 `FUN_75c2_13dc(category, next, prereq)` calls):

| cat | class | chain |
|-----|-------|-------|
| 0 | 3 | Stockade → Fort → Fortress *(fence corner)* |
| 1 | 1 | Armory → Magazine → Arsenal |
| 2 | 4 | Docks → Drydock → Shipyard *(dock corner)* |
| 3 | 2 | Town Hall ×3 → Capitol → Capitol Expansion |
| 4 | 1 | Schoolhouse → College → University |
| 5 | 1 | Warehouse → Warehouse Expansion → Stable |
| 6 | 0 | Custom House |
| 7 | 0 | Printing Press → Newspaper |
| 8 | 0 | Weaver's House → Shop → Textile Mill |
| 9 | 0 | Tobacconist's ×3 |
| 10 | 0 | Rum ×3 |
| 11 | 0 | Fur ×3 |
| 12 | 1 | Carpenter's Shop → Lumber Mill |
| 13 | 2 | Church → Cathedral |
| 14 | 0 | Blacksmith's ×3 |

Category class = first member's size. Counts are 7/4/2/1/1 = the 15 slots,
and `DS:0x266` terminates with `(-1,-1)` after entry 14 — two independent
completeness confirmations.

Structural surprises, both in the port: **fortification and docks are
ordinary categories** (0 and 2, sole members of their classes — that's why
their corners look fixed; the old hand-written dock anchor was 2px off).
**Stable shares the warehouse slot** (`14d4`'s `0x0f || 0x11` branch:
stable-only sprite `0x2f`, combined `0x30`; Warehouse Expansion is never
drawn as itself — its `BUILDING.SS` slot holds pre-stockade fence art; the
badge reports it).

Placement: categories claim positions within their class **in category
order**; the only randomness is the shuffle — per slot 0..14, draw a random
free position in that slot's class, retry on collision. Seed =
`(colony.y << 8) + colony.x + DS:0x8d80`, srand-masked to 15 bits
(`FUN_15eb_1476`).

### The base seed (it *is* in the save — port_bugs.md #564)

`DS:0x8d80` comes from the BIOS tick at `0040:006C` at program start
(`FUN_75c2_2d46`), but it is **saved and restored**: post_map tail @608
(`FUN_2a1f_0c9c` write / `FUN_2a1f_0cb4` read), i.e. `post_map.boot_timer`.
So a layout is stable for the life of a game and a save *does* reproduce a
screenshot's layout. The port reads it: `colony_screen_set_layout_seed` gets
`post_map.boot_timer` on load (game_loop_saveload.c) and on new game, where
`ai_setup_col1_template` stamps the campaign seed as the port's stand-in for the
launch tick (port_bugs.md #578, #1266). The constant `25281`
(`COLONY_DOS_LAYOUT_SEED`) is only the fallback for callers without a save
(tests, tools). Exactly **one**
15-bit base, `25281`, reproduces both golden screenshots at once (18
independent category→slot constraints) — uniqueness over the whole 32768
space is the proof the category table, pools, mapping, shuffle order and
RNG are all right. Both colonies render at golden positions with no
overrides — and that pin is not arbitrary at all: the screenshot saves
(`original_saves/colony-prod-tests/COLONY00-dutch2-*`) carry
`boot_timer = 844481`, and 844481 mod 32768 = **25281**. (Byproduct: the EXE's blacksmith x=7 beat the hand-measured 4.)

## Golden re-check (2026-10-26)

All six views re-rendered with `render_colony` and compared with the goldens.
`render_colony` had drawn a phantom "Stockade" hover label (it never called
`colony_screen_reset_ui`, so `hover_building_row` stayed 0); fixed. A
2026-09-05 build renders the same as today, so nothing below is a regression.

**Closed** (the old "Left unresolved" list):

- **Badge types:** every building badge's number and icon type matches both
  goldens (New Amsterdam 10 / 10 / 19 / 24 / 16 / 82, Recife 13). The
  Carpenter's "16" is the colony's hammers; the Production tab reads 16 too.
- **Food meter "gold bar":** the fish row of DOS's fish/grain split
  (port_bugs.md #607); the port draws it.
- **The "2" between Food and Crosses:** DOS's net-food counter (#28); the port
  draws it.
- **`building_production.md` badge table:** rewritten since (worker capacity,
  SoL folded in, bells on Town Hall only).

**Fixed 2026-10-26** (were port_bugs.md #1274-#1281, all decomp-confirmed
and re-checked against the goldens):

1. Units tab / fence order is the DOS tile chain from the head, latest
   arrival first (`FUN_2f2b_1e46`, `colony_screen_refresh_outside`).
2. Tab buttons: ink-0x3f outline plus sprite; the selected one swaps the two
   offsets, which is the pressed look (`FUN_2f2b_2b66`; hit test
   `FUN_2f2b_4a1c`, buttons at (303, 132 + 15i), 14x13).
3. "Exit" (@MISC 210) at (306, 179), ink 15 (`FUN_2f2b_28d6`).
4. Transport header: "No Ships In Port" (@MISC 11), or "Loading" (@MISC 23)
   + ": " + the selected holder's type name, centred over x 121 w 84 at
   y 132, ink 57 (`FUN_2f2b_24b2`). First ship icon at x 130.
5. People band: each of the SoL and Tory figures is drawn only when its
   percentage is nonzero; the Tory head count rounds first; both share an
   ink of 15 / 4 / 12 at the unrest thresholds (`FUN_2f2b_17d0`).
6. Building badges go through `reports_draw_dos_icon_bar` (`FUN_1097_0174`)
   with the class window from DS:0x24e/0x254/0x25a above, flags 0, so the
   run is centred and compact (`FUN_2f2b_14d4`).
7. Construction title and "(Requires N Tools)" are centred with
   `FUN_281f_0100`'s rule at y 132 / 170; the tools line is built from
   @MISC 43 and @CARGO 14 and drawn in ink 7 when stock covers N, else 15
   (`FUN_2f2b_22b6`).
8. The white Recife outline is the area view's blinking keyboard cursor
   (`FUN_2f2b_0d89`, phase flipped every 0x14 ticks of the 60.877 Hz
   DS:0x92e8 clock, ~328 ms, by `FUN_2f2b_6cd4`). The area grid itself was one pixel up-left; tiles
   now start at (224, 32) with the black frame outside them.

Second pass, same day (were port_bugs.md #1282-#1286):

9. Focus blink on every panel (`FUN_2f2b_2d1c`): the focused panel's
   selection box is white in the on phase (People band: none in the off
   phase; warehouse: yellow), white or yellow while a press on that panel is
   held, green otherwise; every press, release or key restarts the phase on.
   The warehouse slot (DS:0x33a, Food by default) always has a box; `=`/`+`
   use Food when nothing is picked. People-band box is (x-1, y+1)-(x+w, y+h).
10. Tabs switch on release; releasing on the active tab goes back to the
    previous one (DS:0x338, set by mouse switches and 1/2/3/M); B switches to
    Construction (`FUN_2f2b_4a1c`, `FUN_2f2b_6372`).
11. Stable: the warehouse slot gets a horses badge of the breeding potential
    `ceil(horses/25)*2` (DS:0x8dd8) when the Stable is built.
12. Bottom separator at y 128 (both middle sections 120 rows), parchment from
    x 0, People meter row at y 163 / 118 wide, docked holders 4 full icons at
    x 0x82 + 18i (ships lifted 1-2 px) plus 16 miniatures at y 139, with
    `FUN_2f2b_548e`'s gap-free hit test.
13. Europe screen box headers (`FUN_38fd_1382` / `127c` / `0f8c`): centred
    @MISC 9 / @MISC 10 + country / "No Ships In Port" or
    "Loading:  <type>" (Merchantman on its own line), ink 0x45.

**Still different:** the sand backdrop dither and parts of the area view
differ pixel by pixel (about 68k-77k of 256k pixels per 2x shot), D4 polish.

Deliberate, player-requested: the Production pane layout (surplus split,
4-row hammer bar).
