#include "core/map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/diagnostics.h"

/*
 * Terrain byte layout (FreeCol / original .MP format):
 *   bits 0-4: terrain index 0-26
 *   bits 5-7: overlay (0=none, 1=hill, 2=minor river, 5=mountain, 6=major river, ...)
 *
 * PHYS0.SS sprite ranges (from in-game atlas, ~ key debug screen):
 *   1-15   major rivers      17-31  minor rivers
 *   32-47  mountains         48-63  hills
 *   64-79  mixed forests     80-88  roads
 *   89+    resources, fog, ocean/coast overlays, etc.
 *
 * TERRAIN.SS: sprites 0-11 = tundra … high seas (see docs/assets.md).
 */

#define PHYS0_MAJOR_RIVER_FIRST 1
#define PHYS0_MAJOR_RIVER_COUNT 15
#define PHYS0_MINOR_RIVER_FIRST 17
#define PHYS0_MINOR_RIVER_COUNT 15
#define PHYS0_MOUNTAIN_FIRST 32
#define PHYS0_MOUNTAIN_COUNT 16
#define PHYS0_HILL_FIRST 48
#define PHYS0_HILL_COUNT 16

#define TERRAIN_SPRITE_ARCTIC 9
#define TERRAIN_SPRITE_OCEAN 10
#define TERRAIN_SPRITE_HIGH_SEAS 11

static bool overlay_is_hill(uint8_t overlay) {
  return overlay == 1 || overlay == 3;
}

static bool overlay_is_minor_river(uint8_t overlay) {
  return overlay == 2 || overlay == 3;
}

static bool overlay_is_major_river(uint8_t overlay) {
  return overlay == 6 || overlay == 7;
}

static bool overlay_is_mountain(uint8_t overlay) {
  return overlay == 5 || overlay == 7;
}

static uint8_t map_overlay_at(const ColonizeWorldMap* map, int x, int y) {
  return map_terrain_overlay(map_get_terrain(map, x, y));
}

static uint8_t map_cardinal_mask(
  const ColonizeWorldMap* map,
  int x,
  int y,
  bool (*matches)(uint8_t overlay)
) {
  uint8_t mask = 0;
  static const int dx[4] = {0, 1, 0, -1};
  static const int dy[4] = {-1, 0, 1, 0};
  for (int dir = 0; dir < 4; ++dir) {
    const int nx = x + dx[dir];
    const int ny = y + dy[dir];
    if (nx < 0 || ny < 0 || nx >= map->width || ny >= map->height) {
      continue;
    }
    if (matches(map_overlay_at(map, nx, ny))) {
      mask |= (uint8_t)(1u << dir);
    }
  }
  return mask;
}

static int phys0_connectivity_sprite(int first, int count, uint8_t mask) {
  return first + (int)(mask % (uint8_t)count);
}

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

int map_terrain_base_sprite(uint8_t terrain_byte) {
  const int terrain = terrain_byte & 0x1f;
  if (terrain <= 11) {
    return terrain;
  }
  if (terrain == 24) {
    return TERRAIN_SPRITE_ARCTIC;
  }
  if (terrain == 25) {
    return TERRAIN_SPRITE_OCEAN;
  }
  if (terrain == 26) {
    return TERRAIN_SPRITE_HIGH_SEAS;
  }
  /* Land indices 12-23 reuse base land sprites until forest/cleared variants are modeled. */
  return terrain % 12;
}

int map_phys0_overlay_count(const ColonizeWorldMap* map, int x, int y) {
  if (!map) {
    return 0;
  }
  const uint8_t overlay = map_overlay_at(map, x, y);
  if (overlay == 0 || overlay == 4) {
    return 0;
  }

  int count = 0;
  if (overlay_is_hill(overlay) || overlay_is_mountain(overlay)) {
    ++count;
  }
  if (overlay_is_minor_river(overlay) || overlay_is_major_river(overlay)) {
    ++count;
  }
  return count;
}

int map_phys0_overlay_sprite_at(const ColonizeWorldMap* map, int x, int y, int layer) {
  if (!map || layer < 0) {
    return -1;
  }

  const uint8_t overlay = map_overlay_at(map, x, y);
  if (overlay == 0 || overlay == 4) {
    return -1;
  }

  int feature_layer = 0;
  if (overlay_is_hill(overlay) || overlay_is_mountain(overlay)) {
    if (layer == feature_layer) {
      const uint8_t mask = overlay_is_mountain(overlay)
        ? map_cardinal_mask(map, x, y, overlay_is_mountain)
        : map_cardinal_mask(map, x, y, overlay_is_hill);
      if (overlay_is_mountain(overlay)) {
        return phys0_connectivity_sprite(PHYS0_MOUNTAIN_FIRST, PHYS0_MOUNTAIN_COUNT, mask);
      }
      return phys0_connectivity_sprite(PHYS0_HILL_FIRST, PHYS0_HILL_COUNT, mask);
    }
    ++feature_layer;
  }

  if (overlay_is_minor_river(overlay) || overlay_is_major_river(overlay)) {
    if (layer == feature_layer) {
      if (overlay_is_major_river(overlay)) {
        const uint8_t mask = map_cardinal_mask(map, x, y, overlay_is_major_river);
        return phys0_connectivity_sprite(PHYS0_MAJOR_RIVER_FIRST, PHYS0_MAJOR_RIVER_COUNT, mask);
      }
      const uint8_t mask = map_cardinal_mask(map, x, y, overlay_is_minor_river);
      return phys0_connectivity_sprite(PHYS0_MINOR_RIVER_FIRST, PHYS0_MINOR_RIVER_COUNT, mask);
    }
  }

  return -1;
}

int map_phys0_overlay_sprite(const ColonizeWorldMap* map, int x, int y) {
  return map_phys0_overlay_sprite_at(map, x, y, 0);
}

int map_phys0_forest_sprite(const ColonizeWorldMap* map, int x, int y) {
  (void)map;
  (void)x;
  (void)y;
  /* Mixed forest overlays use PHYS0 sprites 64-79; needs runtime tile state. */
  return -1;
}

int map_phys0_feature_sprite(const ColonizeWorldMap* map, int x, int y) {
  return map_phys0_overlay_sprite(map, x, y);
}

int map_terrain_sprite(uint8_t terrain_byte) {
  return map_terrain_base_sprite(terrain_byte);
}
