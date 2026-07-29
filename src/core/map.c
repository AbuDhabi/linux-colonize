#include "core/map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "data/viceroy_tables.h"
#include "platform/diagnostics.h"

/*
 * .MP terrain byte (FreeCol ColonizationMapLoader):
 *   bits 0-2: cleared land type 0-7
 *   bit 3 (8): forest flag (with bits 0-2 -> forest terrain index 8-15)
 *   bit 4 (16): hill/mountain base flag (with bits 0-2)
 *   bits 5-7: hill / river / mountain overlays
 *
 * Coastlines: best-effort 4-quadrant PHYS0 overlays on ocean tiles only.
 * PARKED — wrong vs DOS, cosmetic only. Do not extend without DOS ground truth.
 * See docs/decomp_inventory.md "Parked: coastlines".
 */

#define PHYS0_MAJOR_RIVER_FIRST 1
#define PHYS0_MAJOR_RIVER_COUNT 15
#define PHYS0_MINOR_RIVER_FIRST 17
#define PHYS0_MINOR_RIVER_COUNT 15
#define PHYS0_MOUNTAIN_FIRST 32
#define PHYS0_MOUNTAIN_COUNT 16
#define PHYS0_HILL_FIRST 48
#define PHYS0_HILL_COUNT 16
#define PHYS0_MOUNTAIN_ISOLATED 36
#define PHYS0_BOREAL_TRANSITION 40
#define PHYS0_TROPICAL_TIMBER 99
/* 4-quadrant 8x8 coast fragments — PARKED heuristic; see docs/decomp_inventory.md */
#define PHYS0_COAST_NW_BASE 108
#define PHYS0_COAST_NE_BASE 116
#define PHYS0_COAST_SW_BASE 124
#define PHYS0_COAST_SE_BASE 132
#define PHYS0_COAST_QUAD_PX 8  /* pixels per quadrant side */
#define COAST_QUADS 4

#define MAP_TUNDRA_ROW 0
#define MAP_SCRUB_FOREST_INDEX 9
#define MAP_OCEAN_INDEX 25
#define MAP_HIGH_SEAS_INDEX 26

static int map_decode_terrain_index(uint8_t terrain_byte) {
  const int low3 = (int)(terrain_byte & 7u);
  const int forest_bits = (int)(terrain_byte & 0x18u);

  if (forest_bits == 0x10) {
    return low3;
  }
  if (forest_bits == 0x08) {
    return 8 + low3;
  }
  return (int)(terrain_byte & 0x1fu);
}

static bool map_has_bit3_forest(uint8_t terrain_byte) {
  return (terrain_byte & 0x18u) == 0x08u;
}

static int map_bit3_forest_index(uint8_t terrain_byte) {
  return 8 + (int)(terrain_byte & 7u);
}

static int map_terrain_index_to_sprite(int terrain_index) {
  if (terrain_index >= 0 && terrain_index <= 7) {
    return terrain_index;
  }
  if (terrain_index >= 8 && terrain_index <= 23) {
    return 8;
  }
  if (terrain_index == 24) {
    return 9;
  }
  if (terrain_index == 25) {
    return 10;
  }
  if (terrain_index == 26) {
    return 11;
  }
  return 0;
}

static int map_cleared_base_sprite(uint8_t terrain_byte) {
  const int low3 = (int)(terrain_byte & 7u);
  if (low3 == 0) {
    return 4;
  }
  return low3;
}

static bool map_is_ocean_index(int terrain_index) {
  return terrain_index == MAP_OCEAN_INDEX || terrain_index == MAP_HIGH_SEAS_INDEX;
}

static bool map_is_land_for_coast(int terrain_index) {
  return !map_is_ocean_index(terrain_index);
}

static bool overlay_is_hill(uint8_t overlay, uint8_t terrain_byte) {
  if (overlay != 1 && overlay != 3) {
    return false;
  }
  if ((terrain_byte & 0x10u) != 0) {
    return (terrain_byte & 7u) != 0;
  }
  return true;
}

static bool overlay_is_mountain(uint8_t overlay, uint8_t terrain_byte) {
  if (overlay == 5 || overlay == 7) {
    return true;
  }
  if ((overlay == 1 || overlay == 3) && (terrain_byte & 0x10u) != 0 && (terrain_byte & 7u) == 0) {
    return true;
  }
  return false;
}

static bool overlay_is_minor_river(uint8_t overlay) {
  return overlay == 2 || overlay == 3;
}

static bool overlay_is_major_river(uint8_t overlay) {
  return overlay == 6 || overlay == 7;
}

static bool map_hill_related_tile_dir(uint8_t tile_byte, uint8_t self_byte, int dir) {
  const uint8_t overlay = (uint8_t)(tile_byte >> 5);

  if (overlay_is_hill(overlay, tile_byte)) {
    /* Hill chains extend south and east (AMER2 24,19). */
    return dir == 1 || dir == 2;
  }

  if ((tile_byte & 0x10u) != 0 && (tile_byte & 7u) == (self_byte & 7u) && overlay == 0) {
    const int terrain_index = map_decode_terrain_index(tile_byte);
    if (!map_is_ocean_index(terrain_index) && terrain_index < 24) {
      /* Cleared ridge tiles connect west and east (AMER2 24,20). */
      return dir == 1 || dir == 3;
    }
  }
  return false;
}

static uint8_t map_cardinal_mask(
  const ColonizeWorldMap* map,
  int x,
  int y,
  bool (*matches)(uint8_t tile_byte, uint8_t self_byte, int dir),
  uint8_t self_byte
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
    const uint8_t neighbor = map_get_terrain(map, nx, ny);
    if (matches(neighbor, self_byte, dir)) {
      mask |= (uint8_t)(1u << dir);
    }
  }
  return mask;
}

static bool hill_neighbor_dir(uint8_t tile_byte, uint8_t self_byte, int dir) {
  return map_hill_related_tile_dir(tile_byte, self_byte, dir);
}

static bool mountain_neighbor(uint8_t tile_byte, uint8_t self_byte, int dir) {
  (void)self_byte;
  (void)dir;
  return overlay_is_mountain((uint8_t)(tile_byte >> 5), tile_byte);
}

static bool minor_river_neighbor(uint8_t tile_byte, uint8_t self_byte, int dir) {
  (void)self_byte;
  (void)dir;
  return overlay_is_minor_river((uint8_t)(tile_byte >> 5));
}

static bool major_river_neighbor(uint8_t tile_byte, uint8_t self_byte, int dir) {
  (void)self_byte;
  (void)dir;
  return overlay_is_major_river((uint8_t)(tile_byte >> 5));
}

static bool map_is_land_at(const ColonizeWorldMap* map, int x, int y) {
  if (!map || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return false; /* off-map counts as water, not land */
  }
  return map_is_land_for_coast(map_decode_terrain_index(map_get_terrain(map, x, y)));
}

static bool map_is_water_at(const ColonizeWorldMap* map, int x, int y) {
  if (!map || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return true; /* off-map counts as water */
  }
  return map_is_ocean_index(map_decode_terrain_index(map_get_terrain(map, x, y)));
}

bool map_tile_is_water(const ColonizeWorldMap* map, int x, int y) {
  return map_is_water_at(map, x, y);
}

bool map_tile_is_land(const ColonizeWorldMap* map, int x, int y) {
  return map_is_land_at(map, x, y);
}

typedef struct CoastOverlay {
  int sprite;
  int ox;
  int oy;
} CoastOverlay;

/*
 * 4-quadrant coast system (derived from live VICEROY.EXE RAM analysis):
 * PHYS0 sprites 108-139 are 8x8 coast fragments.
 * For each ocean tile, up to 4 quadrant overlays are emitted.
 * Each quadrant variant = 3-bit mask of which relevant neighbours are land.
 * Variant 0 = no art needed for that quadrant.
 */
static int map_phys0_coast_collect(const ColonizeWorldMap* map, int x, int y, CoastOverlay* out, int max_out) {
  const uint8_t terrain_byte = map_get_terrain(map, x, y);
  if (!map_is_ocean_index(map_decode_terrain_index(terrain_byte))) {
    return 0;
  }

  const int n  = map_is_land_at(map, x,     y - 1) ? 1 : 0;
  const int e  = map_is_land_at(map, x + 1, y)     ? 1 : 0;
  const int s  = map_is_land_at(map, x,     y + 1) ? 1 : 0;
  const int w  = map_is_land_at(map, x - 1, y)     ? 1 : 0;
  const int nw = map_is_land_at(map, x - 1, y - 1) ? 1 : 0;
  const int ne = map_is_land_at(map, x + 1, y - 1) ? 1 : 0;
  const int sw = map_is_land_at(map, x - 1, y + 1) ? 1 : 0;
  const int se = map_is_land_at(map, x + 1, y + 1) ? 1 : 0;

  int count = 0;

  /* NW quadrant: bit0=N, bit1=W, bit2=NW */
  {
    const int v = n | (w << 1) | (nw << 2);
    if (v && count < max_out) {
      out[count++] = (CoastOverlay){PHYS0_COAST_NW_BASE + v, 0, 0};
    }
  }
  /* NE quadrant: bit0=N, bit1=E, bit2=NE */
  {
    const int v = n | (e << 1) | (ne << 2);
    if (v && count < max_out) {
      out[count++] = (CoastOverlay){PHYS0_COAST_NE_BASE + v, PHYS0_COAST_QUAD_PX, 0};
    }
  }
  /* SW quadrant: bit0=S, bit1=W, bit2=SW */
  {
    const int v = s | (w << 1) | (sw << 2);
    if (v && count < max_out) {
      out[count++] = (CoastOverlay){PHYS0_COAST_SW_BASE + v, 0, PHYS0_COAST_QUAD_PX};
    }
  }
  /* SE quadrant: bit0=E, bit1=S, bit2=SE */
  {
    const int v = e | (s << 1) | (se << 2);
    if (v && count < max_out) {
      out[count++] = (CoastOverlay){PHYS0_COAST_SE_BASE + v, PHYS0_COAST_QUAD_PX, PHYS0_COAST_QUAD_PX};
    }
  }

  return count;
}

static int map_phys0_coast_layer_count(const ColonizeWorldMap* map, int x, int y) {
  CoastOverlay unused[COAST_QUADS];
  return map_phys0_coast_collect(map, x, y, unused, COAST_QUADS);
}

static CoastOverlay map_phys0_coast_layer_at(const ColonizeWorldMap* map, int x, int y, int layer) {
  CoastOverlay layers[COAST_QUADS];
  const int count = map_phys0_coast_collect(map, x, y, layers, COAST_QUADS);
  if (layer < 0 || layer >= count) {
    return (CoastOverlay){-1, 0, 0};
  }
  return layers[layer];
}

static int phys0_connectivity_sprite(int first, int count, uint8_t mask) {
  return first + (int)(mask % (uint8_t)count);
}

static int phys0_mountain_sprite(uint8_t mask) {
  if (mask == 0) {
    return PHYS0_MOUNTAIN_ISOLATED;
  }
  return phys0_connectivity_sprite(PHYS0_MOUNTAIN_FIRST, PHYS0_MOUNTAIN_COUNT, mask);
}

static int phys0_bit3_overlay_sprite(int forest_index) {
  switch (forest_index) {
    case 8:
      return PHYS0_BOREAL_TRANSITION;
    case 13:
      return PHYS0_TROPICAL_TIMBER;
    default:
      return -1;
  }
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
  return map_terrain_index_to_sprite(map_decode_terrain_index(terrain_byte));
}

int map_terrain_sprite_at(const ColonizeWorldMap* map, int x, int y) {
  if (!map) {
    return 0;
  }
  const uint8_t terrain_byte = map_get_terrain(map, x, y);
  const int terrain_index = map_decode_terrain_index(terrain_byte);

  if (y == MAP_TUNDRA_ROW && !map_is_ocean_index(terrain_index) && terrain_index < 25) {
    return 0;
  }

  if (map_has_bit3_forest(terrain_byte)) {
    const int forest_index = map_bit3_forest_index(terrain_byte);
    if (forest_index == MAP_SCRUB_FOREST_INDEX) {
      return 8;
    }
    if (phys0_bit3_overlay_sprite(forest_index) >= 0) {
      return map_cleared_base_sprite(terrain_byte);
    }
  }

  return map_terrain_index_to_sprite(terrain_index);
}

int map_phys0_forest_sprite_at(const ColonizeWorldMap* map, int x, int y) {
  if (!map) {
    return -1;
  }
  const uint8_t terrain_byte = map_get_terrain(map, x, y);
  const uint8_t overlay = map_terrain_overlay(terrain_byte);
  const int terrain_index = map_decode_terrain_index(terrain_byte);

  if (map_is_ocean_index(terrain_index)) {
    return -1;
  }
  if (overlay != 0 && overlay != 4) {
    return -1;
  }

  if (map_has_bit3_forest(terrain_byte)) {
    return phys0_bit3_overlay_sprite(map_bit3_forest_index(terrain_byte));
  }

  if (y == MAP_TUNDRA_ROW) {
    return (int)viceroy_feature_sprite_bases_b[3]; /* 65 mixed-forest / tundra canopy */
  }
  return -1;
}

int map_phys0_overlay_count(const ColonizeWorldMap* map, int x, int y) {
  if (!map) {
    return 0;
  }
  const uint8_t terrain_byte = map_get_terrain(map, x, y);
  const uint8_t overlay = map_terrain_overlay(terrain_byte);
  int count = map_phys0_coast_layer_count(map, x, y);

  if (overlay == 0 || overlay == 4) {
    return count;
  }

  if (overlay_is_hill(overlay, terrain_byte) || overlay_is_mountain(overlay, terrain_byte)) {
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

  const uint8_t terrain_byte = map_get_terrain(map, x, y);
  const uint8_t overlay = map_terrain_overlay(terrain_byte);
  const int coast_layers = map_phys0_coast_layer_count(map, x, y);
  if (layer < coast_layers) {
    return map_phys0_coast_layer_at(map, x, y, layer).sprite;
  }
  int feature_layer = coast_layers;

  if (overlay == 0 || overlay == 4) {
    return -1;
  }

  if (overlay_is_hill(overlay, terrain_byte) || overlay_is_mountain(overlay, terrain_byte)) {
    if (layer == feature_layer) {
      const uint8_t mask = overlay_is_mountain(overlay, terrain_byte)
        ? map_cardinal_mask(map, x, y, mountain_neighbor, terrain_byte)
        : map_cardinal_mask(map, x, y, hill_neighbor_dir, terrain_byte);
      if (overlay_is_mountain(overlay, terrain_byte)) {
        return phys0_mountain_sprite(mask);
      }
      return phys0_connectivity_sprite(PHYS0_HILL_FIRST, PHYS0_HILL_COUNT, mask);
    }
    ++feature_layer;
  }

  if (overlay_is_minor_river(overlay) || overlay_is_major_river(overlay)) {
    if (layer == feature_layer) {
      if (overlay_is_major_river(overlay)) {
        const uint8_t mask = map_cardinal_mask(map, x, y, major_river_neighbor, terrain_byte);
        return phys0_connectivity_sprite(PHYS0_MAJOR_RIVER_FIRST, PHYS0_MAJOR_RIVER_COUNT, mask);
      }
      const uint8_t mask = map_cardinal_mask(map, x, y, minor_river_neighbor, terrain_byte);
      return phys0_connectivity_sprite(PHYS0_MINOR_RIVER_FIRST, PHYS0_MINOR_RIVER_COUNT, mask);
    }
  }

  return -1;
}

void map_phys0_overlay_offset_at(
  const ColonizeWorldMap* map,
  int x,
  int y,
  int layer,
  int* out_ox,
  int* out_oy
) {
  if (out_ox) {
    *out_ox = 0;
  }
  if (out_oy) {
    *out_oy = 0;
  }
  if (!map || layer < 0) {
    return;
  }
  const int coast_layers = map_phys0_coast_layer_count(map, x, y);
  if (layer < coast_layers) {
    const CoastOverlay coast = map_phys0_coast_layer_at(map, x, y, layer);
    if (out_ox) {
      *out_ox = coast.ox;
    }
    if (out_oy) {
      *out_oy = coast.oy;
    }
  }
}

int map_phys0_overlay_sprite(const ColonizeWorldMap* map, int x, int y) {
  return map_phys0_overlay_sprite_at(map, x, y, 0);
}

int map_phys0_forest_sprite(const ColonizeWorldMap* map, int x, int y) {
  return map_phys0_forest_sprite_at(map, x, y);
}

int map_phys0_feature_sprite(const ColonizeWorldMap* map, int x, int y) {
  return map_phys0_overlay_sprite(map, x, y);
}

int map_terrain_sprite(uint8_t terrain_byte) {
  return map_terrain_base_sprite(terrain_byte);
}
