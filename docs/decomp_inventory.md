# Decompiled Surface Inventory

For a navigable index of decomp sources, `COLONIZE/` data files, and DOSBox memory
dumps, see [original_index.md](original_index.md). Manual feature coverage vs the
Linux port: [manual_gap.md](manual_gap.md). European / Indian AI FUN_* inventory and
1:1 transcription roadmap: [ai_transcription.md](ai_transcription.md).

This repository keeps Ghidra exports of `VICEROY.EXE` / `MAPEDIT.EXE` under
[`original_sources_decompiled/`](../original_sources_decompiled/) for reverse-engineering
reference. They are not buildable with a modern Linux compiler and retain DOS
memory-model / runtime artifacts.

| File | Source | Notes |
|------|--------|-------|
| `original_sources_decompiled/viceroy_unpacked.c` / `.asm` | Unpacked EXE | ~125k / ~305k lines; overlay-resident code is present |
| `original_sources_decompiled/mapedit.c` | `MAPEDIT.EXE` | Static map feature art |

Prefer **`viceroy_unpacked.*`** when chasing map-view or overlay call chains.

## High-Level Metrics

- Unpacked VICEROY export: large (overlay bodies + many segments)
- Common synthetic symbols:
  - Globals: `DAT_xxxx_xxxx`
  - Labels: `LAB_xxxx_xxxx`
  - Switch labels: `caseD_*`

## Function Clusters by Segment Prefix

The segment prefix in function names (`FUN_ssss_oooo`) provides a practical
first-pass clustering mechanism:

- `FUN_15eb_*`: high-density logic cluster
- `FUN_1d1d_*`: high-density logic + platform-adjacent routines
- `FUN_1427_*`: mid-size cluster
- `FUN_104b_*`, `FUN_1009_*`: smaller utility/control-flow clusters

This clustering should be preserved in initial source splitting to reduce risk.

## DOS/Hardware-Coupled Surfaces

Observed direct I/O and hardware assumptions in the decomp exports:

- VGA DAC and retrace interaction:
  - `out(0x3c8, ...)`
  - `out(..., 0x3c9)`
  - `in(0x3da)`
- PIT timer programming:
  - `out(0x43, 0x36)`
  - `out(0x40, ...)`
- Conventional VGA memory segment assumptions:
  - references involving `0xa000`
- BIOS tick/global data style references:
  - patterns around `DAT_0000_046c` and low-memory globals

These routines belong behind a Linux platform API and must not remain as raw
port I/O in the native build.

## Proposed Boundary: Core vs Platform

### Core Candidate (kept behavior-first)

- Game state updates
- Turn progression and simulation
- Economic/unit/map logic
- Scenario/rules logic

### Platform Candidate (replace with SDL2/Linux services)

- Palette and framebuffer presentation
- Keyboard/mouse polling and event translation
- Time/tick services
- File path, save location, and case normalization
- Audio output (SDL callback + FluidSynth; see `src/core/sound.c`)

## Bring-Up Strategy Notes

- Do not refactor gameplay logic first.
- Introduce wrappers matching expected legacy behavior.
- Route every platform call through explicit interfaces so unresolved behavior
  can be logged and implemented incrementally.

## Current Linux Bring-Up Status

- SDL2 shell, diagnostics log, save/load: original `COLONY##.SAV` structs +
  byte-identical I/O in `src/core/col1_save.{h,c}`; runtime bridge in
  `src/core/col1_bridge.c` (see `docs/savegame.md`); verified against
  `original_saves/COLONY00.SAV` / `COLONY01.SAV`. **Codec ≠ complete field
  semantics** — opaque-byte RE track: [save_format_map.md](save_format_map.md)
  (P0–P4 done for proven peels: atlas, post_map/stuff split, community renames,
  connectivity rebuild + 00f2 cache parity, stuff census + DS-named late chunks,
  colony specialty/AI/timers/`tiles[20]`, head WoI bits + map_mode + zoom,
  indian `euro_diplo` / contact / accum, nation return-xy / diplo stand-ins.
  **P5 naming + P6 template interop done** — mask density, blank census,
  colony levels/specialty, `vis_mask`; late `unknown_ds_*` stay export-OK zero.
  Mid-campaign census = DOS-parity preserve (no freshen).)

- `GAME.TXT` / palette / MADSPACK+FAB / `.PIK` decode: done for menu background
- Decomp exports (`original_sources_decompiled/viceroy_unpacked.c`,
  `original_sources_decompiled/mapedit.c`) are not compiled into the binary; DOS
  typedef stubs live in `src/platform/dos_compat/dos_types.h` for incremental extraction
- Map compositor lookup tables from `VICEROY.EXE` are extracted to `src/data/viceroy_tables.{h,c}`
  (see `docs/viceroy_tables.md`); **static map feature art** follows `MAPEDIT.EXE` instead
- World map view (**fidelity OK vs MAPEDIT**): terrain, land transitions, forest/hill/mountain/river
  connectivity, coasts, estuaries, special resources, rumours — see below and `docs/assets.md`
- Coast / estuary: enabled (`MAP_COAST_OVERLAYS_ENABLED` / `MAP_ESTUARY_OVERLAYS_ENABLED` default 1)
- **Music playback: enabled** — GSOUND bytecode decode + FluidSynth (`COLONIZE_SOUND_PLAYBACK_ENABLED 1`; see `docs/assets.md`)
- Europe screen bring-up: `EUROPE.PIK` + market quotes / dock recruit from `NAMES.TXT`
  (press **E** from the map; phase 5 hold buy/sell + tax, goods persist on **H**/**S**;
  see `src/core/europe.c`)
- Colony screen bring-up: DOS six-view layout — settlement (sprite hit-rects; workers/outside
  on Note 1 selectable strips at building bottom-center / fence; unified colonist selection
  admit/eject via fence; profession sticks across gear/location; working sprites Hardy **#58** /
  Veteran **#59**; thin construction banner), area (1.5× 24px tiles, Note 1 yield strips; fisherman → fish **#57**),   people (SoL/Tory; colonists + fence units on one row; food/crosses/bells
  strips; fish before grain food), transport (class name, hold **#122** empties), multifunction (house **#67**,
  Production Note 1, Units bottom, Construction BUY/CHANGE + hammer rows); warehouse strip
  unchanged; preview via `colony_preview.c`; see `src/core/colony_screen.c`,
  `src/core/colony_yield.c`, `src/core/colony_craft.c`
- Units bring-up: `@UNIT` types from `NAMES.TXT`, map icons from `ICONS.SS`,
  starter Pioneer + Caravel, select/move (terrain/road/river MP costs), deploy dock
  immigrants (**D**), board/unload (**O**/**U**), ship landfall unload / colony dock
  disembark, tile stack popup (wake sentry cargo then select), sail ship to/from Europe
  with passengers (**H** on high seas / **S** in Europe); Pioneer plow/road (**P**/**R**,
  carried tools, WorldMap `improve` flags synced with Col1 mask; yield bonuses in
  `colony_yield.c`; see `src/core/units.c`, `src/core/map.c`, `src/core/europe.c`,
  `src/core/unit_stack.c`)
- Map menu bar: `MENU.TXT` pull-downs with mouse hit-testing (`src/core/map_menu.c`);
  left-click selects unit/tile/colony, right-click selects tile; pan-only while a unit is
  selected; blinking white tile outline in tile-select mode; `CURSOR.SS` #0 is the OS
  pointer over the 320×200 frame on all screens; Colonizopedia category lists from `PEDIA.TXT`
  (`src/core/pedia.c`); pull-down divider after Terrain Types; trade menu entries stubbed
- **New-game wizard** (`src/core/new_game.c`): `@BEGINMENU` → NEW WORLD / AMERICA /
  CUSTOMIZE → difficulty (`DIFFICUL.PIK`) → nation (`NATIONS.PIK`) → leader name /
  `@NATION{n}A/B` on `WOODPANL.PIK` → king audience → `LEVN0001`–`0010` sail → map.
  NEW WORLD / CUSTOMIZE use `map_generate` (`MapGenParams`); AMERICA loads `.MP`.
  Hall of Fame still a stub. Wizard captions use unbold green `FONTINTR` with
  black drop-shadow; nation pick remaps England fill onto `NATIONS.PIK` red.
- Shared wood **popup window** chrome (`src/core/popup.c`): black + mid brown + raised
  bevel from `@COLORS` border0/1/2; title `@BEGINMENU` is the first consumer (`OPENTILE.SS`);
  **GAME → Pick Music** uses the same chrome with `WOODTILE.SS` (`src/core/pick_music.c`)
- Main-map right panel + scrolling 1:1 minimap (`src/core/map_panel.c`): viewport **15×12**
  tiles (`x=0..239`); wood strip `x=240..319` (`WOODTILE.SS`) with black left rule and
  minimap-section separator; AMER2 minimap window **56×39** (click-to-center, dark-orange border);
  `@INFO` unit/date/gold (`NAMEPLAT.SS` / `FONTTINY.FF`); menu bar shares wood + tiny green
  / yellow hotkeys; nation box at `(315,197)` unchanged; not using `WOODPAN2` / `WOODFRAM`
- Report / adviser screens: F2–F10 + REPORTS menu (`src/core/reports.c`);
  F1 Terrain Information → Colonizopedia at cursor; F8=`REPORT8.PIK`; F10=`WOODPANL.PIK`;
  F2–F9 filled from Col1 save + runtime pools (crosses, FF, labor, trade, warehouses,
  ships, rivals, tribes); F10 Colonization Score from manual schedule
  (`reports_compute_score`)
- Colonizopedia: woodcut list screen (`WOODPANL.PIK`) with green entry links in up to
  3 columns, then cargo/unit/terrain/job/building/father/misc articles with
  ICONS / BUILDING / CC-NN / TERRAIN previews
- Turn progression (`src/core/turn.c`): `@TIMECHANGE` calendar, colony production,
  nation crosses/bells hooks, EN→FR→SP→DU Euro AI + Indian AI + King/REF,
  Wait-for-next-unit, End of Turn option, autosave hooks (slots 9 / 8),
  turn-owner indicator (`FUN_1984_00aa`: 5×3 at 315,197; shown only during AI/Indian
  EOT phases; `@COUNTRY` / `@TRIBES` colors)
- Music (`src/core/sound.c`): GSOUND.COL voice bytecode → MIDI events (~60 Hz ticks);
  FluidSynth with SC-55-preferring SoundFont search; ED chords, F3 volume envelope,
  BB pitch-bend RPN; BGM + event (`0x40..`) tables; Pick Music preview + title/map BGM
  via `COLONIZE_SOUND_PLAYBACK_ENABLED`; `COLDIG.BIN` SFX still deferred. Interpreter
  notes: `original_sources_annotated/sound/gsound_interpreter.md`.

## End-of-turn recovery checklist

Full orchestration map (Linux `TURN_PROC_*` ↔ DOS `FUN_130d_0290` /
`FUN_3844_*`, Layer D extracts): [turn_between_players.md](turn_between_players.md)
· [`original_sources_annotated/turn/between_turns.md`](../original_sources_annotated/turn/between_turns.md).

Ordered pipeline recovered for the Linux port:

1. **Human ends turn** — Space / ORDERS → No Orders (`LABELS.TXT` “End of Turn”)
2. **Advance calendar** — `head.year` / `autumn` / `turn` (`@TIMECHANGE` in `GAME.TXT`):
   one turn/year until 1600; thereafter Spring then Autumn each year
3. **Colony production** — field harvest from map-ring `tiles[0..7]` (`NAMES.TXT` yields) − food
   consume 2/colonist; lumberjack → lumber (carpenter invents 1 lumber if none);
   settlement craft (`colony_craft.c`: raw→goods by workplace); hammers toward
   `building_in_production` (Colony Space = free production + UI deltas;
   `README.TXT` “free turn”)
4. **Nation ticks** — liberty bells + crosses; crosses ≥ needed → dock immigrant;
   founding-father election via `founding_fathers_tick` (manual-aligned effects;
   Sepulveda/Cortes hooks still open — see [ai_transcription.md](ai_transcription.md))
5. **European AI** — EN→FR→SP→DU via `player.control` (0 human / 1 AI / 2 withdrawn);
   `ai_euro_nation_turn` (`src/core/ai.c` → `ai_euro.c`): reseed from VR_SEED timer word, tick AI crosses,
   `6d8e`-shaped ship/land passes; **T2 early path** (seed-100 TURN1→7 via `smoke_ai_turns`;
   landfall coastal staging + `ai_euro_found_tile_from_landfall`).
   Full-dispatch planner partial; deep land/ocean `20e6` still open — see [ai_transcription.md](ai_transcription.md).
6. **Indians** — village growth (`FUN_4d56_152e`-style), mid-turn Brave pulse + residual
   overlays (t1 empty; ~50 on t2–t6); named init burns `ai_native_post_first_brave_burns`.
   (`FUN_4d56_1816` / quiet `20e6`); meet/trade/raids via `ai_contact_*` (structural;
   deep `2820`/`4528` PARKED).
7. **King** — partial structural (`ai_king_nation_turn`: tax / declare / REF / war; R6;
   audience/confirm/merc via `ai_popup`)
8. **Refresh human MP** + select next unit with moves (“Continue turn.”)

**New-game AI actors** (`ai_init_new_game`): Col1 template (human control 0 / gold 1000;
AI control 1 / gold 0; `nation_relation[]=-1`); human and three rival fleets on eastern
high seas at turn 0 (Caravel / Dutch Merchantman with Pioneer+Soldier; skills from
difficulty/nation; landfall `goto`);
AMERICA villages from `TRIBE.TXT` + Brave per village; NEW WORLD / CUSTOMIZE procedural
villages (cap ~84). Human starter `nation_id` matches chosen power.

**Parked (later):** deep Euro `20e6` / T3 planner (**mapped** —
[`move_scoring_land.md`](../original_sources_annotated/ai/move_scoring_land.md) /
[`move_scoring_ship.md`](../original_sources_annotated/ai/move_scoring_ship.md));
deep Indian `2820`/`4528` + VGA meet chrome (**mapped** —
[`indian_trade_2820.md`](../original_sources_annotated/ai/indian_trade_2820.md) /
[`indian_settlement_4528.md`](../original_sources_annotated/ai/indian_settlement_4528.md));
deep King/REF (`10f0` economy, letter cinematic, exact `0x5382`). Early-AI T2 gate is green
(`test-saves-ai/TURN1`…`TURN7`). Roadmap: [ai_transcription.md](ai_transcription.md).
Year-end `0442` UI: [`year_end_chrome.md`](../original_sources_annotated/turn/year_end_chrome.md).

Evidence:

| Source | Finding |
|--------|---------|
| `GAME.TXT` `@TIMECHANGE` | Biannual seasons from 1600 |
| `original_saves/COLONY00/01.SAV` | turn 0→2 ≈ year 1492→1494 (1 year/turn); AI fleets leave Europe; AI crosses advance |
| `README.TXT` | Dutch turn ends European order; colony Space = free production |
| `LABELS.TXT` | “End of Turn” / “Continue turn.” |
| `NAMES.TXT` `@COUNTRY` / `@TRIBES` | Turn-owner box colors (DS:0x848 / 0x84c) |
| `COLONIZE/TRIBE.TXT` | AMERICA village seeds |
| `FUN_6a09_0006` / `FUN_4d56_152e` / `FUN_521d_6d8e` | Tribe place / growth / Euro AI dispatcher |
| `FUN_1984_00aa` / `FUN_281f_0590` | 5×3 fill at (0x13b, 0xc5) overlaid on screen |
| `original_sources_decompiled/viceroy_unpacked.asm` | `TIMECHANGE` / `MULTINEXT` / `SEASONS` string table only (no FUN_* XREF yet) |

AI colony production for non-human Europeans already runs through the shared
`turn_run_colony_production` loop (all active colonies). Older notes claiming a
nation skip were stale.

## Map generation (VICEROY)

Procedural NEW WORLD maps live in **VICEROY**, not MAPEDIT. Entry: `FUN_684c_08c0` (dispatched via `FUN_2a1f_083e`); land blobs `FUN_684c_02a8` / form thunks; continent labeling `FUN_67bf_0000`. Customize UI: `FUN_733a_0270` on `CUSTOMIZ.PIK` (4 columns × 3 rows; defaults all mid/`1`). Linux port: `src/core/map_gen.c` (`map_generate` / `MapGenParams`) + `NEW_GAME_PHASE_CUSTOMIZE` in `src/core/new_game.c`. See [assets.md](assets.md) “Map generation (NEW WORLD)”.

Continent flood-fill IDs (`FUN_67bf_0000`) are not written to layer2 in gen v1 (shipped AMER2 leaves layer2 zero); diagonal land cleanup (2×2 masks 6/9) is ported. RNG is exact DOS `FUN_1d1d_0e04` / `FUN_19ef_0032` (`src/core/dos_rng.c`): NEW WORLD draws customize axes (`range(0,3)`) then reseeds before `map_generate`. Tribe placement (`FUN_6a09`) **reseeds to `rng_seed`** at entry (matches DOS `6a09` / VR_SEED timer word) — it does **not** continue the post-mapgen stream or restore a post-axes LCG (stale “post-axes restore” docs were wrong for this path). Land mask, latitude/climate paint, forest wander, rivers, and arctic/HS tail bit-match seed 100 terrain. `FUN_6a09` capitals/satellites match SEED100; Braves spawn then take one post-`6a09` native pulse (`FUN_4d56_1816` path in `ai.c`) so coordinates/MP/`turns_worked` match the golden save (34 tribes / 46 units). Golden fidelity: `smoke_mapgen_seed100` vs `test-saves-mapgen/SEED100.SAV` (no seed-special runtime path).

## Map compositor (MAPEDIT)

Authoritative static map compositor: `COLONIZE/MAPEDIT.EXE` /
`original_sources_decompiled/mapedit.c` (`FUN_1a47_0932`, land mask `FUN_1a47_01ae`).
No RTLink; no fog-of-war / animation. Coast and estuary are **on by default**
(`MAP_COAST_OVERLAYS_ENABLED` / `MAP_ESTUARY_OVERLAYS_ENABLED` in `src/core/map.h`);
the flags are compile-time debug toggles, not parked features.

**Recovered and matching MAPEDIT on AMER2:** coasts, estuaries, land–land transitions, forest/hill/mountain/river connectivity, procedural resources, rumours. Details and PHYS0 ranges: [assets.md](assets.md).

### Coast decoration

On ocean / high-seas tiles with at least one land neighbour:

1. Build 8-bit land mask (N→NW clockwise) and four 3-bit quadrant masks (cardinal → bits 0/2 on adjacent quads; diagonal → bit 1).
2. Special full-tile corners when mask matches (id 0..3 = land NW/NE/SW/SE):
   PHYS0 **`150 + id`**. MAPEDIT encodes `0x97+id` (1-based IDs 151–154);
   convert with −1 for 0-based sheet indices 150–153.
3. Else four 8×8 fragments: MAPEDIT ID **`0x6d + 4*quad_mask + q`** → index
   **`108 + 4*quad_mask + q`** at pixel offsets NW/NE/SE/SW (`0`/`8`).

Draw order vs MAPEDIT: land TERRAIN underlayer (last cardinal neighbour) → coast PHYS0 →
masked ocean into palette-0 holes (`FUN_1a47_0676`) → resources / estuary. Fog of war is not drawn
(MAPEDIT skips it too).

### River estuaries

Ocean tile with `terrain & 0xc0`: for each cardinal neighbour that is land with `terrain & 0x40`, blit MAPEDIT ID **`0x8d+q`** / **`0x91+q`** → indices **140–147**. Inland rivers unchanged.

Fixtures: `amer2_coast_fixtures` / `amer2_river_estuary` in `tests/smoke/test_map.c`.

### Forest / hill / mountain / inland river connectivity

Same MAPEDIT cardinal mask as rivers (`FUN_1a47_030e` / `036e` / `0418`): **N=8, S=4, W=2, E=1**.

| Feature | Sprite (0-based) | Match |
|---------|------------------|-------|
| Forest (non-scrub) | `64 + mask` | any non-scrub forest neighbour |
| Mountain | `32 + mask` | `(n & 0xa0) == (self & 0xa0)` when `self & 0x20` |
| Hill | `48 + mask` | same bit test (hill when not also `& 0x80`) |
| Major / minor river | `0+mask` / `16+mask` (0 → 15) | `n & 0x40` |

Forest canopy via `map_phys0_forest_sprite_at`; hills/mountains/rivers/resources via overlay layers.

### Land transitions

`FUN_1a47_06da`: PHYS0 **104+q** colour-0 masks then neighbour TERRAIN fill (before forest). Ocean neighbours resolve through land cardinals (W/S/E/N).

### Resources / rumours

`FUN_12ab_0458` / `0540`: seed default **100**; type table DS **0x4de** / file **0x1794e** (MAPEDIT DS base **0x17470**); PHYS **89+type** / **103**. Ocean → fish (type 7). Mountain class **27** / hill **28**. Full type→terrain table in [assets.md](assets.md).

### Remaining gaps

- Fog-of-war: dedicated `map.seen` is Partial (reveal + black paint); MAPEDIT static view still has no fog by design
- Roads
- Coast animation frames; per-tile texture-variation overlays from DOS RAM buffers
- Resource seed from live game RNG (static map view uses MAPEDIT default seed 100)

Prior VICEROY RAM-buffer / quadrant coast heuristics are **superseded** by MAPEDIT (see [viceroy_tables.md](viceroy_tables.md)).

