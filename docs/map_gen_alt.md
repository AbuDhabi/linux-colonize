# Alternate map generator

STATUS: port-only divergence, first cut 2026-11-11; tunables expected to move.

`settings.json` `divergences.alternate_map_generator` (default off, docs/settings.md)
routes NEW WORLD / CUSTOMIZE map generation (`map_generate`) to
`map_generate_alternate` in `src/core/map_gen_alt.c`. DOS has nothing like it, so
none of this is DOS-faithful; the goal is maps that read like real geography
instead of the original's blob noise. Off, `map_generate` is byte-for-byte the
DOS pipeline (goldens unaffected).

## Inputs

Same `MapGenParams` and the same shared campaign RNG as `map_generate`, so a seed
replays and tribe placement continues from the post-gen stream.

| Customize axis | Effect |
|----------------|--------|
| Land Mass (0..2, NEW WORLD 0..3) | share of plate area started as land: 33 / 42 / 52 / 60 % (`k_land_permille_by_mass`) |
| Land Form (0..2, 3) | plate count: 48 / 28 / 16 / 10 (`k_plates_by_form`): archipelago = many small plates |
| Temperature (0..2, 3) | Cold = 2 / 1 / 0 / 0 rows at each active edge (`k_cold_rows`); Hot..Cool share the rest (`k_band_widths`): Warm widens Hot / Warm, Cool widens Cool |
| Climate (0..2, 3) | moisture an ocean tile hands the wind: 60 / 100 / 140 / 180 (`k_moisture_by_climate`) |
| `forest_extra` | unused |

## Pipeline

The rim never takes part: 2 columns west / east (water, later the sea lanes) and
1 row north / south (water during generation, arctic after the finalize pass), so
generated land reaches the row next to the arctic edge rows.
Elevation is an `int` (land starts at +16, water at -16); tiles below 0 are water.

1. **Plates.** `plate_count` seeds grow by random frontier picks until they tile
   the active area. Plates are picked as land, in random order, while that brings
   the land area closer to the Land Mass target. Each plate gets a drift vector,
   -2..2 per axis.
2. **Tectonics** (`TECTONIC_ITERS` = 6). Every plate shifts by its drift (speed 2
   every iteration, speed 1 half the time), carrying its elevation. Where two
   plates land on one tile the higher crust keeps it and gains `COLLISION_UPLIFT`
   (the lower crust is pushed under): coastal and inland ranges, island arcs. A tile
   nobody moves onto becomes new low crust (`RIFT_ELEV`): rifts and new seas.
   Crust pushed into the rim is lost. The net change is spread evenly back over the
   active area, so total elevation is the same after every iteration.
   Rigid shifts leave rift and plate edges as straight lines along the drift axes,
   so the crust is then warped: each tile takes the elevation found at a spot
   displaced by a smooth random field (three value-noise octaves, lattice
   `WARP_CELL` = 8, 4, 3 tiles, `WARP_AMP` = 4, 1.6, 1 tiles), and the net change is
   spread back again (zero-sum).
3. **Erosion** (`EROSION_ITERS` = 4). Each 4-neighbour pair moves 1/8 of its
   difference downhill; an exact integer transfer, so zero-sum too. Then the four
   columns at the west and east active edges are worn down to `k_side_cap`
   (-4, 4, 12, 24 from the edge in), and what they lose is spread over the tiles
   inland of them. Per row and side, smooth noise widens the always-sea part by
   0..`SIDE_REACH_MAX` (4) columns, so the worn coast is not a straight line. Land tapers off instead of ending in a cut-off cliff, and the
   column next to each sea lane is always ordinary sea.
4. **Winds.** Three-cell circulation by latitude (equator at mid-height, rims at
   +-90 degrees): trade winds (easterly, toward the equator) below 30, westerlies
   (poleward) 30..60, polar easterlies above. Air is drawn onshore along the
   gradient of a blurred land share (land heats faster: monsoon and sea breeze,
   `WIND_ONSHORE`), and slopes deflect part of the uphill component, more the
   steeper (`WIND_DEFLECT`). Two 3x3 blurs, then unit length.
5. **Rainfall.** Steady-state moisture advection (`RAIN_ITERS` sweeps): water tiles
   refill the air to the climate's moisture; each land tile takes what arrives
   from one tile upwind and drops a share as rain: `RAIN_BASE_DEP`, plus
   `RAIN_OROGRAPHIC` per elevation unit the wind climbs, halved where it
   descends. Ranges get a wet windward and a dry lee side (rain shadow). Land
   touching water (8-neighbour) with rain 0 gets 1, so no desert lies on a shore.
6. **Temperature.** Horizontal bands Hot (2) .. Cold (-2) from the equator to each
   rim (Cold only the `k_cold_rows` rows at the edge; arctic is useless land), then `TEMP_SMEAR_ITERS` passes mixing each tile half-and-half with the
   air 2 tiles upwind (3 for the north-south component), plus up to 0.3 band of
   jitter so the band edges are ragged. The smear never takes Cold past its rows.
7. **Terrain.** Land by band: Cold arctic, Cool tundra, Temperate plains, Warm
   grassland, Hot savannah. Relief (tile minus its lowest land 8-neighbour; a
   shore is not a cliff) `>= MOUNTAIN_RELIEF` is a mountain, `>= HILL_RELIEF` a
   hill (never on arctic). Rain turns into terrain after the rivers (step 8).
8. **Rivers.** A priority flood from every water tile gives each land tile a
   downstream 4-neighbour along a path that never climbs (pits drain as if
   filled; lakes are ordinary sea). Routing adds coarse value noise
   (`ROUTE_NOISE` over `ROUTE_CELL` tiles; routing only) so flat plate interiors
   form valleys. Rain accumulates downstream; `RIVER_MINOR_FLOW` makes a minor
   river, `RIVER_MAJOR_FLOW` a major one, so wet basins get more, bigger rivers
   with tributaries. Rivers shorter than `RIVER_MIN_LEN` at the mouth, and branches
   that would run 4-adjacent to another river (they draw as one blob), are dropped.
   A river tile loses its hill / mountain (DOS never stores both), arctic carries
   none, and the water tile at the mouth gets the river bit (estuary). Each river
   tile then gains 1 rain (major river 2). With that bonus in, rain becomes
   terrain: no rain is desert; rain more than one standard deviation below the land
   mean dries (plains -> tundra, grassland -> prairie, savannah -> grassland); more
   than one above moistens (savannah -> swamp, grassland -> marsh, plains ->
   prairie, tundra -> plains). So a river through a dry basin is never desert.
9. **Forest** on `FOREST_PERCENT` (90) % of flat non-arctic land; desert becomes
   scrub forest.
10. **Shared tail.** High seas: per row, open water walked west from the east edge
    (east half only), minus tiles within 2 of land, kept only while contiguous with
    the edge (the Euro landfall walk needs that). Then `map_gen_finalize_border_and_forest`,
    `map_gen_assign_continents` (+ Pacific / offshore suppress) and
    `map_gen_assign_euro_landfalls`, as `map_generate` does.

Specials and Lost City Rumours need nothing extra: like DOS they hash from
`prime_resource_seed`, drawn first exactly as `map_generate` does.

## Checks

- `unit_map_gen` `test_alternate_generator_invariants`: 81 axis combinations:
  routing, replay, land share bounds, water rim plus the sea column inside the
  west / east rim, arctic top / bottom rows, valid classes, no river on a
  hill, every river tile linked to river or sea, landfalls present.
- `unit_new_game` `alternate_map_village_counts`: a full `ai_init_new_game` on an
  alternate map places every tribe's capital.
