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
int map_terrain_sprite(uint8_t terrain_byte);

#endif
