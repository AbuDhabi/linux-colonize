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

typedef struct ColonizeWorldMap {
  uint8_t width;
  uint8_t height;
  uint8_t* terrain;
  uint8_t* layer2;
  uint8_t* layer3;
  size_t tile_count;
} ColonizeWorldMap;

bool map_load_mp(const char* path, ColonizeWorldMap* out_map, char* err, size_t err_size);
/* Allocate empty layers (terrain/layer2/layer3 zeroed). Replaces any prior buffers. */
bool map_alloc(ColonizeWorldMap* out_map, uint8_t width, uint8_t height, char* err, size_t err_size);
void map_free(ColonizeWorldMap* map);

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

#endif
