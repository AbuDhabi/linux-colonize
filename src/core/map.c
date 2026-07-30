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
 * PARKED — wrong vs DOS, cosmetic only, disabled via MAP_COAST_OVERLAYS_ENABLED.
 * Implementation kept in map_phys0_coast_collect() for later recovery.
 * See docs/decomp_inventory.md "Parked: coastlines and estuaries".
 *
 * River estuaries on ocean (overlay on index 25): PARKED via MAP_ESTUARY_OVERLAYS_ENABLED.
 * phys0_estuary_sprite() kept for recovery; default is TERRAIN-only on those tiles.
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
#define PHYS0_TUNDRA_CANOPY viceroy_feature_sprite_bases_b[3] /* 65 */

#if MAP_COAST_OVERLAYS_ENABLED
/* 4-quadrant 8x8 coast fragments — parked heuristic; see docs/decomp_inventory.md */
#define PHYS0_COAST_NW_BASE 108
#define PHYS0_COAST_NE_BASE 116
#define PHYS0_COAST_SW_BASE 124
#define PHYS0_COAST_SE_BASE 132
#define PHYS0_COAST_QUAD_PX 8  /* pixels per quadrant side */
#define COAST_QUADS 4
#endif

#define MAP_TUNDRA_ROW 0
#define MAP_OCEAN_INDEX 25
#define MAP_HIGH_SEAS_INDEX 26

static int map_decode_terrain_index(uint8_t terrain_byte) {
  /* FreeCol ColonizationMapLoader: bits 0-4 are terrain index 0-26. */
  return (int)(terrain_byte & 0x1fu);
}

static bool map_is_forest_index(int terrain_index) {
  return terrain_index >= 8 && terrain_index <= 23;
}

static int map_forest_type(int terrain_index) {
  return terrain_index & 7;
}

static int map_cleared_base_for_forest_type(int forest_type) {
  /* PEDIA: forest type N clears to land type N (boreal → tundra). */
  return forest_type;
}

static int map_terrain_index_to_sprite(int terrain_index) {
  if (terrain_index >= 0 && terrain_index <= 7) {
    return terrain_index;
  }
  if (map_is_forest_index(terrain_index)) {
    const int forest_type = map_forest_type(terrain_index);
    if (forest_type == 1) {
      return 8; /* scrub forest */
    }
    return map_cleared_base_for_forest_type(forest_type);
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

static int phys0_forest_overlay_sprite(int forest_type) {
  return viceroy_forest_phys0_sprite(forest_type);
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

static bool overlay_is_any_river(uint8_t overlay) {
  return overlay_is_minor_river(overlay) || overlay_is_major_river(overlay);
}

static bool minor_river_neighbor_only(uint8_t tile_byte, uint8_t self_byte, int dir) {
  (void)self_byte;
  (void)dir;
  return overlay_is_minor_river((uint8_t)(tile_byte >> 5));
}

static bool any_river_neighbor(uint8_t tile_byte, uint8_t self_byte, int dir) {
  (void)self_byte;
  (void)dir;
  return overlay_is_any_river((uint8_t)(tile_byte >> 5));
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

static bool major_river_neighbor(uint8_t tile_byte, uint8_t self_byte, int dir) {
  (void)self_byte;
  (void)dir;
  return overlay_is_major_river((uint8_t)(tile_byte >> 5));
}

static bool map_is_ocean_river_tile(uint8_t terrain_byte) {
  return map_is_ocean_index(map_decode_terrain_index(terrain_byte))
    && overlay_is_any_river(map_terrain_overlay(terrain_byte));
}

#if MAP_ESTUARY_OVERLAYS_ENABLED
#define PHYS0_COAST_FRAG_FIRST 108
#define PHYS0_COAST_FRAG_LAST 139
#define PHYS0_FRAGMENT_8_PX 8

static bool land_river_neighbor(uint8_t tile_byte, uint8_t self_byte, int dir) {
  (void)self_byte;
  (void)dir;
  if (map_is_ocean_index(map_decode_terrain_index(tile_byte))) {
    return false;
  }
  return overlay_is_any_river((uint8_t)(tile_byte >> 5));
}

static void phys0_coast_fragment_offset(int sprite, int* out_ox, int* out_oy) {
  if (out_ox) {
    *out_ox = 0;
  }
  if (out_oy) {
    *out_oy = 0;
  }
  if (sprite >= 108 && sprite <= 115) {
    return;
  }
  if (sprite >= 116 && sprite <= 123) {
    if (out_ox) {
      *out_ox = PHYS0_FRAGMENT_8_PX;
    }
    return;
  }
  if (sprite >= 124 && sprite <= 131) {
    if (out_oy) {
      *out_oy = PHYS0_FRAGMENT_8_PX;
    }
    return;
  }
  if (sprite >= 132 && sprite <= 139) {
    if (out_ox) {
      *out_ox = PHYS0_FRAGMENT_8_PX;
    }
    if (out_oy) {
      *out_oy = PHYS0_FRAGMENT_8_PX;
    }
  }
}

static int phys0_estuary_sprite(uint8_t overlay, uint8_t land_river_mask) {
  /*
   * Ocean river mouths: overlay nibble + land-side river connectivity.
   * PARKED heuristic — canonical indices from DOS RAM 0x0328f0 on AMER2.
   */
  static const int minor_estuary_by_mask[16] = {
    137, 134, 132, -1,
    137, -1, -1, -1,
    137, -1, 149, -1,
    60, -1, -1, -1,
  };
  static const int major_estuary_by_mask[16] = {
    -1, 135, 149, -1,
    128, -1, -1, -1,
    68, -1, -1, -1,
    -1, -1, -1, -1,
  };
  const uint8_t mask = land_river_mask & 0x0f;

  if (overlay_is_major_river(overlay)) {
    return major_estuary_by_mask[mask];
  }
  if (overlay_is_minor_river(overlay)) {
    return minor_estuary_by_mask[mask];
  }
  return -1;
}

static int map_estuary_phys0_sprite(const ColonizeWorldMap* map, int x, int y) {
  const uint8_t terrain_byte = map_get_terrain(map, x, y);
  if (!map_is_ocean_river_tile(terrain_byte)) {
    return -1;
  }
  const uint8_t overlay = map_terrain_overlay(terrain_byte);
  const uint8_t land_mask =
    map_cardinal_mask(map, x, y, land_river_neighbor, terrain_byte);
  return phys0_estuary_sprite(overlay, land_mask);
}
#endif /* MAP_ESTUARY_OVERLAYS_ENABLED */

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

bool map_tile_is_coastal(const ColonizeWorldMap* map, int x, int y) {
  if (!map_tile_is_land(map, x, y)) {
    return false;
  }
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0) {
        continue;
      }
      if (map_tile_is_water(map, x + dx, y + dy)) {
        return true;
      }
    }
  }
  return false;
}

bool map_tile_is_high_seas(const ColonizeWorldMap* map, int x, int y) {
  if (!map || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return false;
  }
  return map_decode_terrain_index(map_get_terrain(map, x, y)) == MAP_HIGH_SEAS_INDEX;
}

#if MAP_COAST_OVERLAYS_ENABLED
typedef struct CoastOverlay {
  int sprite;
  int ox;
  int oy;
} CoastOverlay;
#endif

#if MAP_COAST_OVERLAYS_ENABLED
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

static CoastOverlay map_phys0_coast_layer_at(const ColonizeWorldMap* map, int x, int y, int layer) {
  CoastOverlay layers[COAST_QUADS];
  const int count = map_phys0_coast_collect(map, x, y, layers, COAST_QUADS);
  if (layer < 0 || layer >= count) {
    return (CoastOverlay){-1, 0, 0};
  }
  return layers[layer];
}
#endif /* MAP_COAST_OVERLAYS_ENABLED */

static int map_phys0_coast_layer_count(const ColonizeWorldMap* map, int x, int y) {
#if MAP_COAST_OVERLAYS_ENABLED
  CoastOverlay unused[COAST_QUADS];
  return map_phys0_coast_collect(map, x, y, unused, COAST_QUADS);
#else
  (void)map;
  (void)x;
  (void)y;
  return 0;
#endif
}

static int phys0_connectivity_sprite(int first, int count, uint8_t mask) {
  return first + (int)(mask % (uint8_t)count);
}

static bool map_has_special_mountain_marker(const ColonizeWorldMap* map, int x, int y) {
  /* AMER2 has one arctic tile tagged in layer 3 that DOS draws with mountain art. */
  return map_get_layer3(map, x, y) == 0x0eu;
}

static int river_mask_popcount(uint8_t mask) {
  int count = 0;
  for (int bit = 0; bit < 4; ++bit) {
    if ((mask & (uint8_t)(1u << bit)) != 0) {
      ++count;
    }
  }
  return count;
}

static int phys0_river_sprite(bool major, uint8_t minor_mask, uint8_t major_mask, uint8_t any_mask) {
  /*
   * Cardinal connectivity -> PHYS0 river sprite.
   * Bits: N=1, E=2, S=4, W=8.
   * Minor tiles use any_mask (minor + major neighbours). Major tiles use major_mask
   * unless only one major link and minor neighbours exist — then any_mask (AMER2 21,18).
   */
  static const int minor_by_mask[16] = {
    -1, 24, 17, 25,
    20, 28, 21, 30,
    18, 26, 19, 27,
    22, 27, 29, 31,
  };
  static const int major_by_mask[16] = {
    -1, 4, 1, 9,
    2, 14, 5, 14,
    2, 10, 3, 11,
    7, 11, 13, 15,
  };

  if (!major) {
    return minor_by_mask[any_mask & 0x0f];
  }
  if (river_mask_popcount(major_mask) >= 2) {
    return major_by_mask[major_mask & 0x0f];
  }
  if (minor_mask != 0) {
    return minor_by_mask[any_mask & 0x0f];
  }
  return major_by_mask[major_mask & 0x0f];
}

static int phys0_mountain_sprite(uint8_t mask) {
  if (mask == 0) {
    return PHYS0_MOUNTAIN_ISOLATED;
  }
  return phys0_connectivity_sprite(PHYS0_MOUNTAIN_FIRST, PHYS0_MOUNTAIN_COUNT, mask);
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

  if (map_is_forest_index(terrain_index)) {
    const int forest_type = map_forest_type(terrain_index);
    if (forest_type == 1) {
      return 8; /* scrub: TERRAIN-only */
    }
    return map_cleared_base_for_forest_type(forest_type);
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

  if (map_is_forest_index(terrain_index)) {
    return phys0_forest_overlay_sprite(map_forest_type(terrain_index));
  }

  if (y == MAP_TUNDRA_ROW) {
    return (int)PHYS0_TUNDRA_CANOPY;
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

  if (map_has_special_mountain_marker(map, x, y)) {
    ++count;
  }
  if (overlay == 0 || overlay == 4) {
    return count;
  }

  if (overlay_is_hill(overlay, terrain_byte) || overlay_is_mountain(overlay, terrain_byte)) {
    ++count;
  }
  if (overlay_is_minor_river(overlay) || overlay_is_major_river(overlay)) {
    if (map_is_ocean_river_tile(terrain_byte)) {
#if MAP_ESTUARY_OVERLAYS_ENABLED
      if (map_estuary_phys0_sprite(map, x, y) >= 0) {
        ++count;
      }
#endif
    } else {
      ++count;
    }
  }
  return count;
}

int map_phys0_overlay_sprite_at(const ColonizeWorldMap* map, int x, int y, int layer) {
  if (!map || layer < 0) {
    return -1;
  }

  const uint8_t terrain_byte = map_get_terrain(map, x, y);
  const uint8_t overlay = map_terrain_overlay(terrain_byte);
#if MAP_COAST_OVERLAYS_ENABLED
  const int coast_layers = map_phys0_coast_layer_count(map, x, y);
  if (layer < coast_layers) {
    return map_phys0_coast_layer_at(map, x, y, layer).sprite;
  }
  int feature_layer = coast_layers;
#else
  int feature_layer = 0;
#endif

  if (map_has_special_mountain_marker(map, x, y)) {
    if (layer == feature_layer) {
      return PHYS0_MOUNTAIN_ISOLATED;
    }
    ++feature_layer;
  }

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
      if (map_is_ocean_river_tile(terrain_byte)) {
#if MAP_ESTUARY_OVERLAYS_ENABLED
        return map_estuary_phys0_sprite(map, x, y);
#else
        return -1;
#endif
      }
      const uint8_t minor_mask =
        map_cardinal_mask(map, x, y, minor_river_neighbor_only, terrain_byte);
      const uint8_t major_mask =
        map_cardinal_mask(map, x, y, major_river_neighbor, terrain_byte);
      const uint8_t any_mask =
        map_cardinal_mask(map, x, y, any_river_neighbor, terrain_byte);
      return phys0_river_sprite(
        overlay_is_major_river(overlay),
        minor_mask,
        major_mask,
        any_mask
      );
    }
    ++feature_layer;
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
#if MAP_COAST_OVERLAYS_ENABLED
  const int coast_layers = map_phys0_coast_layer_count(map, x, y);
  if (layer < coast_layers) {
    const CoastOverlay coast = map_phys0_coast_layer_at(map, x, y, layer);
    if (out_ox) {
      *out_ox = coast.ox;
    }
    if (out_oy) {
      *out_oy = coast.oy;
    }
    return;
  }
  int feature_layer = coast_layers;
#else
  int feature_layer = 0;
#endif

  const uint8_t terrain_byte = map_get_terrain(map, x, y);
  const uint8_t overlay = map_terrain_overlay(terrain_byte);

  if (map_has_special_mountain_marker(map, x, y)) {
    ++feature_layer;
  }
  if (overlay == 0 || overlay == 4) {
    return;
  }
  if (overlay_is_hill(overlay, terrain_byte) || overlay_is_mountain(overlay, terrain_byte)) {
    ++feature_layer;
  }
#if MAP_ESTUARY_OVERLAYS_ENABLED
  if ((overlay_is_minor_river(overlay) || overlay_is_major_river(overlay))
      && map_is_ocean_river_tile(terrain_byte)) {
    if (layer == feature_layer) {
      const int sprite = map_estuary_phys0_sprite(map, x, y);
      if (sprite >= PHYS0_COAST_FRAG_FIRST && sprite <= PHYS0_COAST_FRAG_LAST) {
        phys0_coast_fragment_offset(sprite, out_ox, out_oy);
      }
    }
  }
#endif
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

int map_pedia_terrain_index_at(const ColonizeWorldMap* map, int x, int y) {
  if (!map || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return 0;
  }
  const uint8_t terrain_byte = map_get_terrain(map, x, y);
  const uint8_t overlay = map_terrain_overlay(terrain_byte);
  if (overlay_is_mountain(overlay, terrain_byte) || map_has_special_mountain_marker(map, x, y)) {
    return 27;
  }
  if (overlay_is_hill(overlay, terrain_byte)) {
    return 28;
  }
  int index = map_decode_terrain_index(terrain_byte);
  if (index < 0) {
    index = 0;
  }
  if (index > 26) {
    index = 26;
  }
  return index;
}
