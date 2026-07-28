#include "core/map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/diagnostics.h"

/*
 * Terrain byte layout (FreeCol / original .MP format):
 *   bits 0-2: base terrain (0-7)
 *   bits 3-4: forest / special encoding
 *   bits 0-4: terrain index 0-26 (tundra..high seas)
 *   bits 5-7: hill / river / mountain overlay
 *
 * TERRAIN.SS holds 12 base tile sprites. Forest variants reuse the cleared
 * terrain sprite until PHYS0.SS overlay rendering is wired up.
 */
static const uint8_t TERRAIN_SPRITE[27] = {
  0, 1, 2, 3, 4, 5, 6, 7,
  0, 1, 2, 3, 4, 5, 6, 7,
  0, 1, 2, 3, 4, 5, 6, 7,
  8, 9, 10
};

bool map_load_mp(const char* path, ColonizeWorldMap* out_map, char* err, size_t err_size) {
  if (!path || !out_map) {
    snprintf(err, err_size, "map_load_mp bad args");
    return false;
  }
  memset(out_map, 0, sizeof(*out_map));

  FILE* f = fopen(path, "rb");
  if (!f) {
    snprintf(err, err_size, "cannot open %s", path);
    return false;
  }

  uint8_t header[COLONIZE_MAP_HEADER_SIZE];
  if (fread(header, 1, sizeof(header), f) != sizeof(header)) {
    fclose(f);
    snprintf(err, err_size, "short header in %s", path);
    return false;
  }

  const uint8_t width = header[0];
  const uint8_t height = header[2];
  if (width == 0 || height == 0) {
    fclose(f);
    snprintf(err, err_size, "invalid map dimensions in %s", path);
    return false;
  }

  const size_t tile_count = (size_t)width * (size_t)height;
  uint8_t* terrain = calloc(tile_count, 1);
  uint8_t* layer2 = calloc(tile_count, 1);
  uint8_t* layer3 = calloc(tile_count, 1);
  if (!terrain || !layer2 || !layer3) {
    free(terrain);
    free(layer2);
    free(layer3);
    fclose(f);
    snprintf(err, err_size, "oom loading map %s", path);
    return false;
  }

  if (fread(terrain, 1, tile_count, f) != tile_count ||
      fread(layer2, 1, tile_count, f) != tile_count ||
      fread(layer3, 1, tile_count, f) != tile_count) {
    free(terrain);
    free(layer2);
    free(layer3);
    fclose(f);
    snprintf(err, err_size, "truncated map data in %s", path);
    return false;
  }
  fclose(f);

  out_map->width = width;
  out_map->height = height;
  out_map->terrain = terrain;
  out_map->layer2 = layer2;
  out_map->layer3 = layer3;
  out_map->tile_count = tile_count;

  diag_info("Loaded map %s (%ux%u, %zu tiles)", path, width, height, tile_count);
  return true;
}

void map_free(ColonizeWorldMap* map) {
  if (!map) {
    return;
  }
  free(map->terrain);
  free(map->layer2);
  free(map->layer3);
  memset(map, 0, sizeof(*map));
}

uint8_t map_get_terrain(const ColonizeWorldMap* map, int x, int y) {
  if (!map || !map->terrain || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return 0;
  }
  return map->terrain[y * map->width + x];
}

uint8_t map_get_layer3(const ColonizeWorldMap* map, int x, int y) {
  if (!map || !map->layer3 || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return 0;
  }
  return map->layer3[y * map->width + x];
}

uint8_t map_terrain_overlay(uint8_t terrain_byte) {
  return (uint8_t)(terrain_byte >> 5);
}

int map_terrain_sprite(uint8_t terrain_byte) {
  const int terrain = terrain_byte & 0x1f;
  if (terrain >= 0 && terrain < 27) {
    return TERRAIN_SPRITE[terrain];
  }
  return terrain % 12;
}
