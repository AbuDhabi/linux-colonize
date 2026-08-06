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
/* Clamp *x/*y into the inset interior (no-op if map is too small). */
void map_clamp_coords_inset(const ColonizeWorldMap* map, int* x, int* y);

/* Col1 visibility bit for European nation 0..3. */
#define MAP_SEEN_NATION_BIT(nation) ((uint8_t)(0x10u << ((nation) & 3)))

bool map_tile_seen_by(const ColonizeWorldMap* map, int x, int y, int nation_id);
void map_reveal_tile(ColonizeWorldMap* map, int x, int y, int nation_id);
void map_reveal_radius(ColonizeWorldMap* map, int x, int y, int nation_id, int radius);
void map_reveal_all(ColonizeWorldMap* map, int nation_id);
/* Copy Col1 seen[] into map->seen (same byte layout). */
void map_seen_from_col1(ColonizeWorldMap* map, const uint8_t* col1_seen, size_t count);
void map_seen_to_col1(const ColonizeWorldMap* map, uint8_t* col1_seen, size_t count);

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

uint8_t map_get_terrain(const ColonizeWorldMap* map, int x, int y);
uint8_t map_get_layer3(const ColonizeWorldMap* map, int x, int y);
uint8_t map_terrain_overlay(uint8_t terrain_byte);
int map_terrain_base_sprite(uint8_t terrain_byte);
int map_terrain_sprite_at(const ColonizeWorldMap* map, int x, int y);
int map_phys0_forest_sprite_at(const ColonizeWorldMap* map, int x, int y);
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
/* True when terrain byte has a river (major or minor). */
bool map_tile_has_river(const ColonizeWorldMap* map, int x, int y);
bool map_tile_has_major_river(const ColonizeWorldMap* map, int x, int y);

bool map_tile_has_road(const ColonizeWorldMap* map, int x, int y);
bool map_tile_is_plowed(const ColonizeWorldMap* map, int x, int y);
void map_tile_set_road(ColonizeWorldMap* map, int x, int y, bool on);
void map_tile_set_plowed(ColonizeWorldMap* map, int x, int y, bool on);
/* Clear forest canopy to base land type; preserves river/hill overlay bits. */
bool map_tile_clear_forest(ColonizeWorldMap* map, int x, int y);
/* Land movement cost stub (pedia terrain); road/river halves (min 1). Sea = 1. */
int map_move_cost_at(const ColonizeWorldMap* map, int x, int y);

#endif
