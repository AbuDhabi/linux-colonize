#ifndef COLONIZE_MAP_H
#define COLONIZE_MAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COLONIZE_MAP_HEADER_SIZE 6

typedef struct ColonizeWorldMap {
  uint8_t width;
  uint8_t height;
  uint8_t* terrain;
  uint8_t* layer2;
  uint8_t* layer3;
  size_t tile_count;
} ColonizeWorldMap;

bool map_load_mp(const char* path, ColonizeWorldMap* out_map, char* err, size_t err_size);
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

#endif
