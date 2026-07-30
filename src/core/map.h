#ifndef COLONIZE_MAP_H
#define COLONIZE_MAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COLONIZE_MAP_HEADER_SIZE 6

/*
 * Ocean-tile coast decoration (4-quadrant PHYS0 108–139 heuristic).
 * PARKED — wrong vs DOS and disabled by default. Set to 1 to re-enable drawing
 * and the coast smoke fixtures in tests/smoke/test_map.c.
 * See docs/decomp_inventory.md "Parked: coastlines and estuaries".
 */
#ifndef MAP_COAST_OVERLAYS_ENABLED
#define MAP_COAST_OVERLAYS_ENABLED 0
#endif

/*
 * Ocean-tile river estuaries (terrain 25 + river overlay → PHYS0 mouth art).
 * PARKED — disabled by default; set to 1 to re-enable phys0_estuary_sprite()
 * and amer2_river_estuary fixtures in tests/smoke/test_map.c.
 * See docs/decomp_inventory.md "Parked: coastlines and estuaries".
 */
#ifndef MAP_ESTUARY_OVERLAYS_ENABLED
#define MAP_ESTUARY_OVERLAYS_ENABLED 0
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
