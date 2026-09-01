#ifndef COLONIZE_MAP_H
#define COLONIZE_MAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COLONIZE_MAP_HEADER_SIZE 6

/*
 * Ocean-tile coast decoration from MAPEDIT.EXE FUN_1a47_0932 / FUN_1a47_01ae:
 * 8-neighbour land mask → either one 16×16 corner (150–153) or four 8×8
 * fragments (108 + 4*quad_mask + q). MAPEDIT IDs are 1-based; values here are
 * 0-based sheet indices. Default on — fidelity matches MAPEDIT; set to 0 only
 * to debug without coast art.
 */
#ifndef MAP_COAST_OVERLAYS_ENABLED
#define MAP_COAST_OVERLAYS_ENABLED 1
#endif

/*
 * Ocean-tile river estuaries from MAPEDIT.EXE FUN_1a47_0932:
 * terrain & 0xc0 on ocean → up to four 16×16 PHYS0 mouths (140–143 major,
 * 144–147 minor) when the matching cardinal neighbour is land with bit 0x40.
 * Default on (same as coasts).
 */
#ifndef MAP_ESTUARY_OVERLAYS_ENABLED
#define MAP_ESTUARY_OVERLAYS_ENABLED 1
#endif

/* Runtime tile improvements (synced to Col1 mask road/plowed on save/load). */
#define MAP_IMPROVE_ROAD 0x01u
#define MAP_IMPROVE_PLOWED 0x02u
/*
 * Col1 map.mask / DOS layer2 occupancy bits (FUN_137f_0314 / UNITFLAG,
 * FUN_137f_0358 / COLONYFLAG). Must stay in sync with unit/colony/tribe tiles.
 */
#define MAP_OCCUPANCY_HAS_UNIT 0x01u
#define MAP_OCCUPANCY_HAS_CITY 0x02u
/* Col1 mask density bits mirrored in layer2 (FUN_137f_015e / FUN_281f_068c). */
#define MAP_LAYER2_SUPPRESS 0x04u /* prime suppress / silver deplete */
#define MAP_LAYER2_PURCHASED 0x10u /* tribal land purchased */
#define MAP_LAYER2_PACIFIC 0x20u /* western ocean strip (mapgen) */
/* Layer2 stand-in: procedural LCR consumed — NOT Col1 road (mask 0x08). */
#define MAP_LAYER2_RUMOUR_CLEARED 0x08u
/*
 * Col1 mask bit 0x40 = plowed (same bit col1_bridge maps to MAP_IMPROVE_PLOWED).
 * DOS FUN_1000_88d6's `layer2 & 0x48` = road | plowed ("improved tile").
 * Was misnamed MAP_LAYER2_FA_ROAD (a road mirror) until 2026-08-27.
 */
#define MAP_LAYER2_PLOWED 0x40u

typedef struct ColonizeWorldMap {
  uint8_t width;
  uint8_t height;
  uint8_t* terrain;
  uint8_t* layer2;
  uint8_t* layer3;
  uint8_t* improve; /* per-tile flags: MAP_IMPROVE_* */
  /*
   * Per-tile exploration (Col1 `map.seen` layout): bit (0x10 << nation) for
   * European powers 0..3. layer3 remains continent/owner — never fog.
   */
  uint8_t* seen;
  size_t tile_count;
  /*
   * FUN_684c_08c0 LAB_684c_1b4c: per-nation western rim of eastern high seas
   * (landfall goto). Valid after map_generate; cleared by map_alloc/map_free.
   */
  uint8_t euro_landfall_x[4];
  uint8_t euro_landfall_y[4];
  uint8_t euro_landfalls_ok;
  /* FUN_684c_08c0 first rng_range(1,0x7fff) → DS:0x190 / Col1 prime_resource_seed. */
  uint16_t prime_resource_seed;
} ColonizeWorldMap;

bool map_load_mp(const char* path, ColonizeWorldMap* out_map, char* err, size_t err_size);
/* Allocate empty layers (terrain/layer2/layer3/seen zeroed). Replaces any prior buffers. */
bool map_alloc(ColonizeWorldMap* out_map, uint8_t width, uint8_t height, char* err, size_t err_size);
void map_free(ColonizeWorldMap* map);

/*
 * FUN_137f_000a: true for the playable/visible interior (excludes the 1-tile rim).
 * Standard Col1 maps are 58×72 stored; visible area is 56×70.
 */
bool map_coords_inset(const ColonizeWorldMap* map, int x, int y);
/* Clamp *x / *y into the inset interior (no-op if map is too small). */
void map_clamp_coords_inset(const ColonizeWorldMap* map, int* x, int* y);

/* Col1 visibility bit for European nation 0..3. */
#define MAP_SEEN_NATION_BIT(nation) ((uint8_t)(0x10u << ((nation) & 3)))

bool map_tile_seen_by(const ColonizeWorldMap* map, int x, int y, int nation_id);
void map_reveal_tile(ColonizeWorldMap* map, int x, int y, int nation_id);
void map_reveal_radius(ColonizeWorldMap* map, int x, int y, int nation_id, int radius);
/*
 * FUN_13f1_0158 unit sight reveal: every tile with |dx|<2 and |dy|<2 is
 * revealed; the outer ring (radius ≥ 2) only reveals tiles of the unit's
 * own domain — water for ships, land on the unit's own continent for land
 * units (13e4_0074 water test + 137f_02a0 continent id).
 */
void map_reveal_sight(
  ColonizeWorldMap* map, int x, int y, int nation_id, int radius, bool is_ship
);
/*
 * Same walk as map_reveal_sight, but calls `fn(ctx, tx, ty, outer)` for every
 * tile it reveals (after the seen bit is set). `outer` is FUN_13f1_0158's
 * local_c: 0 inside the |dx|,|dy|<2 core, 1 on the domain-gated ring. Only
 * inset tiles (FUN_137f_000a) are ever revealed; a rim centre reveals nothing.
 */
typedef void (*MapRevealTileFn)(void* ctx, int x, int y, bool outer);
void map_reveal_sight_each(
  ColonizeWorldMap* map,
  int x,
  int y,
  int nation_id,
  int radius,
  bool is_ship,
  MapRevealTileFn fn,
  void* ctx
);
/*
 * FUN_1427_0bfe: nation currently has a unit or a colony on one of the 8
 * neighbours of (x,y) — occupancy bit + layer3 owner nibble, tile itself
 * excluded. This is DOS "live sight" (unit vis bits, coastal fort targets).
 */
bool map_nation_watches_tile(const ColonizeWorldMap* map, int x, int y, int nation_id);
void map_reveal_all(ColonizeWorldMap* map, int nation_id);
/* Copy Col1 seen[] into map->seen (same byte layout). */
void map_seen_from_col1(ColonizeWorldMap* map, const uint8_t* col1_seen, size_t count);
void map_seen_to_col1(const ColonizeWorldMap* map, uint8_t* col1_seen, size_t count);

/* Set or clear one occupancy bit on map->layer2 (no-op if OOB / no layer2). */
void map_occupancy_set_layer2(ColonizeWorldMap* map, int x, int y, uint8_t bit, bool on);

/*
 * Fog edge on a *seen* tile: PHYS0 104+q (N/E/S/W) colour-0 fringe toward an
 * unseen cardinal neighbour. Returns sprite index or -1.
 */
int map_fog_edge_mask_sprite_at(
  const ColonizeWorldMap* map,
  int x,
  int y,
  int nation_id,
  int index
);
/* Number of fog-edge cardinals (0..4) for a seen tile. */
int map_fog_edge_count(const ColonizeWorldMap* map, int x, int y, int nation_id);
/* VICEROY FUN_6ba1_06e0: every fog-edge mask gets the neighbour's terrain
 * dithered into its colour-0 holes — seen tile toward unseen neighbour. */
int map_fog_edge_fill_sprite_at(
  const ColonizeWorldMap* map,
  int x,
  int y,
  int nation_id,
  int index
);
/* Unseen tile toward seen neighbours (FUN_6ba1_0938 unseen path): mask 104+q
 * plus the seen neighbour's terrain; land fog tile resolves an ocean
 * neighbour via its W/S/E/N cardinals or skips the edge. */
int map_fog_reveal_edge_count(const ColonizeWorldMap* map, int x, int y, int nation_id);
int map_fog_reveal_edge_mask_sprite_at(
  const ColonizeWorldMap* map,
  int x,
  int y,
  int nation_id,
  int index
);
int map_fog_reveal_edge_fill_sprite_at(
  const ColonizeWorldMap* map,
  int x,
  int y,
  int nation_id,
  int index
);

uint8_t map_get_terrain(const ColonizeWorldMap* map, int x, int y);
uint8_t map_get_layer3(const ColonizeWorldMap* map, int x, int y);
/*
 * FUN_281f_0722 / FUN_137f_01ca — continent id = layer3 low nibble.
 * Cite: accessors.c continent_id; ai.c ai_continent_id.
 */
int map_continent_id_at(const ColonizeWorldMap* map, int x, int y);
/*
 * Owner (Euro nation 0..3 or tribe id) of whatever is physically standing on
 * (x,y) right now: a settlement (HAS_CITY) if one is there, else an
 * occupying unit (HAS_UNIT), else -1. FUN_1000_88c2 / FUN_137f_0428
 * `tile_tribe_or_presence` (original_sources_annotated/ai/accessors.c) —
 * does NOT cover a colony's wider worked-tile claim, only literal
 * occupancy. Cite: euro_unit_act.md T1.8 2026-08-22 (0015bc hard-reject).
 */
int map_tile_tribe_or_presence(const ColonizeWorldMap* map, int x, int y);
uint8_t map_terrain_overlay(uint8_t terrain_byte);
int map_terrain_base_sprite(uint8_t terrain_byte);
int map_terrain_sprite_at(const ColonizeWorldMap* map, int x, int y);
int map_phys0_forest_sprite_at(const ColonizeWorldMap* map, int x, int y);
/* PHYS0 149 when tile is plowed (runtime improve); -1 otherwise. */
int map_phys0_plow_sprite_at(const ColonizeWorldMap* map, int x, int y);
/*
 * Road overlays (FUN_6ba1_0938): isolated PHYS0 80, else multi-blit 81+d per
 * connected 8-neighbor (N..NW). Prefer layer count/sprite; sprite_at = layer 0.
 */
int map_phys0_road_layer_count(const ColonizeWorldMap* map, int x, int y);
int map_phys0_road_layer_sprite_at(const ColonizeWorldMap* map, int x, int y, int index);
int map_phys0_road_sprite_at(const ColonizeWorldMap* map, int x, int y);
/* MAPEDIT coast layer count (0 if open ocean / land). */
int map_phys0_coast_layer_count(const ColonizeWorldMap* map, int x, int y);
/*
 * MAPEDIT land underlayer for coastal ocean (FUN_1a47_05b2 after FUN_1a47_01ae).
 * Last cardinal land neighbour's TERRAIN sprite, or this tile's ocean sprite if only
 * diagonal land. Returns -1 when the tile is not a coastal ocean composite.
 */
int map_coast_underlayer_sprite_at(const ColonizeWorldMap* map, int x, int y);

/* MAPEDIT land-land edge blends (FUN_1a47_06da): PHYS0 104+q then neighbour TERRAIN fill. */
int map_land_transition_count(const ColonizeWorldMap* map, int x, int y);
int map_land_transition_mask_sprite_at(const ColonizeWorldMap* map, int x, int y, int index);
int map_land_transition_fill_terrain_at(const ColonizeWorldMap* map, int x, int y, int index);

int map_phys0_overlay_count(const ColonizeWorldMap* map, int x, int y);
int map_phys0_overlay_sprite_at(const ColonizeWorldMap* map, int x, int y, int layer);
/*
 * Land-feature identity of one map_phys0_overlay_sprite_at layer, for the
 * VIEW ~Hidden Terrain reveal (peels layers without duplicating the count/
 * order logic above). WATER covers coast + estuary layers — never peeled.
 */
typedef enum ColonizeMapOverlayKind {
  MAP_OVERLAY_KIND_WATER = 0,
  MAP_OVERLAY_KIND_MOUNTAIN,
  MAP_OVERLAY_KIND_HILL,
  MAP_OVERLAY_KIND_RIVER,
  MAP_OVERLAY_KIND_RESOURCE,
  MAP_OVERLAY_KIND_RUMOUR
} ColonizeMapOverlayKind;
ColonizeMapOverlayKind map_phys0_overlay_kind_at(const ColonizeWorldMap* map, int x, int y, int layer);
/* Pixel offset within the 16×16 tile for 8×8 coast fragments; 0,0 for full tiles. */
void map_phys0_overlay_offset_at(
  const ColonizeWorldMap* map,
  int x,
  int y,
  int layer,
  int* out_ox,
  int* out_oy
);
int map_phys0_overlay_sprite(const ColonizeWorldMap* map, int x, int y);
int map_phys0_forest_sprite(const ColonizeWorldMap* map, int x, int y);
int map_phys0_feature_sprite(const ColonizeWorldMap* map, int x, int y);
int map_terrain_sprite(uint8_t terrain_byte);

bool map_tile_is_water(const ColonizeWorldMap* map, int x, int y);
bool map_tile_is_land(const ColonizeWorldMap* map, int x, int y);
/* Land tile with at least one adjacent (8-neighbor) water tile — docks eligible. */
bool map_tile_is_coastal(const ColonizeWorldMap* map, int x, int y);
/* Terrain index 26 — high seas / sea lane (Europe route). */
bool map_tile_is_high_seas(const ColonizeWorldMap* map, int x, int y);

/*
 * Colonizopedia terrain index (0–28) for a map tile.
 * Mountains → 27, hills → 28; otherwise FreeCol bits 0–4 (0–26).
 */
int map_pedia_terrain_index_at(const ColonizeWorldMap* map, int x, int y);

/* Special resource type 0..13, or -1 if none (MAPEDIT procedural). */
int map_resource_type_at(const ColonizeWorldMap* map, int x, int y);
/*
 * Like map_resource_type_at, but still returns the special when layer2 marks
 * settlement ownership (sprites stay hidden; yields still use the resource).
 */
int map_resource_type_for_yield(const ColonizeWorldMap* map, int x, int y);
bool map_tile_has_rumour(const ColonizeWorldMap* map, int x, int y);
/* Mark procedural lost-city rumour at (x,y) as explored/consumed. Returns false if none. */
bool map_clear_rumour(ColonizeWorldMap* map, int x, int y);
/* True when terrain byte has a river (major or minor). */
bool map_tile_has_river(const ColonizeWorldMap* map, int x, int y);
bool map_tile_has_major_river(const ColonizeWorldMap* map, int x, int y);

bool map_tile_has_road(const ColonizeWorldMap* map, int x, int y);
/* MAP_OCCUPANCY_HAS_CITY on layer2 — a colony (any nation's) sits here. */
bool map_tile_has_city(const ColonizeWorldMap* map, int x, int y);
bool map_tile_is_plowed(const ColonizeWorldMap* map, int x, int y);
void map_tile_set_road(ColonizeWorldMap* map, int x, int y, bool on);
void map_tile_set_plowed(ColonizeWorldMap* map, int x, int y, bool on);
/* Clear forest canopy to base land type; preserves river/hill overlay bits. */
bool map_tile_clear_forest(ColonizeWorldMap* map, int x, int y);
/*
 * True for Scrub Forest (forest type 1 of 8) — clears to Desert. VIEW ~Hidden
 * Terrain phase 3 draws Desert here instead of the scrub-ground quirk sprite
 * MAPEDIT normally shows under scrub canopy.
 */
bool map_tile_is_scrub_forest(const ColonizeWorldMap* map, int x, int y);
/*
 * DOS FUN_19b7_0006 / 465b terrain class: hill bit → 28, mountain → 27,
 * else terrain & 0x1f.
 */
int map_dos_terr_class_at(const ColonizeWorldMap* map, int x, int y);
/*
 * DOS DS:0x2f76 terr_cost byte (stride 0x10). Brave spent = byte*3; human/Euro
 * map_move_cost_* uses byte at NAMES movement scale. Cite: move_spent.c.
 */
int map_dos_terr_cost_byte(int terr_class);
/*
 * DOS DS:0x2f77 founding-site score byte (same stride-16 record, offset +1).
 * FUN_521d_06ae base score. Cite: euro_goals.c; brave Memory dump.
 */
int map_dos_terr_found_score_byte(int terr_class);
/*
 * DOS DS:0x2f78 Pioneer clear/plow/road work-turns threshold byte (same
 * stride-16 record, offset +2). Real turns needed = byte+2, halved for
 * Hardy Pioneer. FUN_479b_01a6/0526. Cite: live capture, 2026-08-20.
 */
int map_dos_terr_pioneer_threshold_byte(int terr_class);
/*
 * DOS DS:0x2f7a colonist work-plot labor/travel penalty byte (same
 * stride-16 record, offset +4). Consumed by FUN_15eb_28c8 (colonist
 * work-plot job scoring, RE complete but not ported — see
 * docs/ai_port_plan.md T1.17 and original_sources_annotated/turn/
 * colonist_work_plot_28c8.md). Cite: terrain_yields.md `+0x4` row.
 */
int map_dos_terr_labor_penalty_byte(int terr_class);
/*
 * DOS DS:0x2f80 Pioneer clear/plow completion lumber-reward scale byte
 * (offset +8). Real reward = byte*20<<hardy_pioneer if colony has a
 * Lumber Mill, else flat 20<<hardy_pioneer (mill check is a floor, not
 * a gate). FUN_479b_01a6. Cite: live capture, 2026-08-20.
 */
int map_dos_terr_lumber_reward_byte(int terr_class);
/*
 * Land movement cost: terr_cost[class] (NAMES scale). Prefer map_move_cost_step
 * for actual moves (DOS FA road-pair + cardinal river pair → cost 1). Sea → 1.
 */
int map_move_cost_at(const ColonizeWorldMap* map, int x, int y);
/*
 * DOS FUN_465b_0000 cost head in thirds: terr_cost[class(to)]*3; road/colony
 * (layer2 & 0x0a) on both tiles or minor river on both + cardinal step → 1;
 * a tribe/settlement-owned destination caps it at 3. Non-land `to` → 3.
 */
int map_move_spent_thirds(
  const ColonizeWorldMap* map,
  int from_x,
  int from_y,
  int to_x,
  int to_y
);
/* DOS 465b-shaped step cost from→to (road both / river both+cardinal → 1). */
int map_move_cost_step(
  const ColonizeWorldMap* map,
  int from_x,
  int from_y,
  int to_x,
  int to_y
);

#endif
