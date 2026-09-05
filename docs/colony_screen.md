# Porting the Colony screen (multipurpose view) to golden fidelity

How `src/core/colony_screen.c`/`.h` was aligned to six DOS reference
screenshots — New Amsterdam and Recife (Dutch) × three `ColonyMultiMode`
tabs — from `original_saves/report-screen-goldens/dutch-reports.SAV`.
Follows the methodology in [report_screens.md](report_screens.md); read that
first. This file is compressed to verdicts + traps (2026-09-05); the full
session narratives are in git history.

## Tool

`build/render_colony <data_dir> <save.SAV> <colony_name> <multi_mode> <out.ppm> [rects]`
(`tools/render_colony_main.c`) — loads a Col1 save, bridges it, calls
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

### SoL bonus belongs in every badge

Area-view field-tile badges and settlement (per-building) badges both used
to pass `sol_bonus=0` "by design" — a design note written before any Colony
golden existed. Golden proves both wrong: every badge is off by exactly the
SoL bonus. Both paths now fold `colony_prod_sol_bonus_field` +
`colony_flags` in; manufacturing/Town Hall/Church/Carpenter badges read
`view->preview.craft_gross[]` / FF-folded crosses/bells totals, not
`colony_prod_building_display_output`'s smaller local estimate. All field
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
  min 0); tools never adjusted.
- **BUY (`FUN_2f2b_5e44`, player-corrected twice):** one uniform `@BUYME1`
  Yes/No when affordable, `@BUYME0` info when not — never a
  tools-short-specific popup (`@NEEDTOOLS*` stays turn.c's EOT notice).
  Cost = `hammers_deficit × 13` + `tools_deficit × (per-nation byte + 4)`
  (table byte unresolved; approximated `difficulty + 4`, flagged in code),
  **doubled when `colony->hammers == 0`**. BUY only tops up
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

- Fortification strip is colonist figures only — Artillery excluded
  (`colony_screen_unit_is_artillery()`) at fence draw + hit-test; the
  Units tab and People-band outside row correctly keep it.
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
(`colony_screen_open/draw_custom_house`): every cargo but Food (15 rows —
the Col1 bitfield has all 16, the autosell denylist applies at EOT sell
time, not in the list), DOS-style bullet ring (GAME.TXT `@CUSTOM` has an
`@checkbox` directive; the pixel font has no usable `[`/`]` or circle
glyphs), uniform dark green text, row click toggles
`colonies_toggle_custom_house_cargo()`, stays open. Colonists cannot be
assigned to Custom House / Printing Press / Newspaper (no `@JOB` slot) —
guard in `colonies_assign_workplace`.

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
`turn_select_next_unit` incl. standing-order skips
(`turn_human_units_exhausted`'s `moves_left` test wrongly counted
Fortified/Sentried units as pending).

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

Found while porting the Warehouse Expansion badge (bugs.md 383). All static
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
| `DS:0x23c`/`0x242` | 3,12 | 20,8 | 25,22 | 5,5 | 0,0 | overlay dx,dy |
| `DS:0x248` | 17 | 21 | 25 | 65 | 0 | |
| `DS:0x24e`/`0x254` | −5,−3 | 0,1 | 0,1 | 0,1 | 0,1 | worker-strip dx,dy |
| `DS:0x25a` | 20 | 22 | 30 | 20 | 20 | strip width |
| `DS:0x260` | 45 | 44 | 43 | 0 | 46 | empty-slot placeholder sprite |

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

### The base seed, and why DOS layouts aren't in the save

`DS:0x8d80` comes from the BIOS tick at `0040:006C` at program start —
wall-clock. Real DOS re-rolls every layout each launch; no save reproduces
a screenshot's layout. The port pins the base instead. Exactly **one**
15-bit base, `25281`, reproduces both golden screenshots at once (18
independent category→slot constraints) — uniqueness over the whole 32768
space is the proof the category table, pools, mapping, shuffle order and
RNG are all right. Both colonies render at golden positions with no
overrides. (Byproduct: the EXE's blacksmith x=7 beat the hand-measured 4.)

## Left unresolved

- **Building badge *types* need a from-scratch recheck**: Town Hall and
  Printing Press both showed "82" (Town Hall's is bells, confirmed; Press
  resolving to the same bells value looks like a badge-type assignment
  bug), and Carpenter showed an unexpected "16".
- **People band Food badge extra element**: golden shows a thin gold/olive
  horizontal bar under the number (native x=2..40, y≈166) — not
  identified.
- **Unexplained "2"** between the Food and Crosses meters in the golden's
  People band (no black box) — matches nothing the port draws.
- **Units tab style gaps**: roster ordering (Artillery vs Dragoon)
  reversed vs golden; the golden's active mode-selector indicator style
  unidentified; Recife's golden has a white square outline on one ocean
  minimap tile (possibly a captured-moment cursor).
- **BUY/CHANGE hotkey-letter coloring**: port highlights the `~` hotkey
  letter; golden shows uniform text. Not changed — shared `ui_button.c`
  affects other screens without golden coverage.
- **`building_production.md` badge table** had unverified pre-golden
  claims (SoL exclusion corrected 2026-08-27); other rows worth a
  skim-and-correct pass.

## Final match quality

All six renders match their goldens numerically and stylistically;
building positions are now DOS-exact via the recovered tables. Remaining
open items are the isolated details above.
