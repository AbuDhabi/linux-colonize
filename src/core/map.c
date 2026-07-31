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
 * Ocean coast / estuary PHYS0: MAPEDIT.EXE FUN_1a47_0932 (no RTLink).
 * Coast: FUN_1a47_01ae land-neighbour mask → fragments 109+4*m+q or corners 150–153.
 * Estuary: ocean terrain & 0xc0 → mouths 141–148 toward land river neighbours.
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

#if MAP_COAST_OVERLAYS_ENABLED || MAP_ESTUARY_OVERLAYS_ENABLED
#define PHYS0_COAST_FRAG_BASE 109 /* MAPEDIT: 0x6d + 4*mask + q */
#define PHYS0_COAST_CORNER_BASE 150 /* PHYS0 150–153: NW/NE/SW/SE land */
#define PHYS0_ESTUARY_MAJOR_BASE 141 /* MAPEDIT: 0x8d */
#define PHYS0_ESTUARY_MINOR_BASE 145 /* MAPEDIT: 0x8d + 4 */
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

static bool map_is_land_at(const ColonizeWorldMap* map, int x, int y);

/* MAPEDIT 8-neighbour walk: N,NE,E,SE,S,SW,W,NW (DS 0x6c0 / 0x6ca). */
static const int mapedit_neigh8_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int mapedit_neigh8_dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

/* MAPEDIT estuary / 4-corner walk: N,E,S,W (DS 0x6b4 / 0x6ba). */
static const int mapedit_card_dx[4] = {0, 1, 0, -1};
static const int mapedit_card_dy[4] = {-1, 0, 1, 0};

#if MAP_COAST_OVERLAYS_ENABLED || MAP_ESTUARY_OVERLAYS_ENABLED
typedef struct CoastOverlay {
  int sprite;
  int ox;
  int oy;
} CoastOverlay;

/*
 * FUN_1a47_01ae: build 8-bit land mask and per-quadrant 3-bit masks.
 * Cardinal land sets bit2 on quad[d>>1] and bit0 on the next quad;
 * diagonal land sets bit1 on the facing quad.
 * Quadrant pixel origins (ASM 0x714/0x715): NW,NE,SE,SW.
 */
static void mapedit_coast_masks(
  const ColonizeWorldMap* map,
  int x,
  int y,
  uint8_t* out_mask8,
  uint8_t out_quads[4]
) {
  uint8_t mask8 = 0;
  out_quads[0] = out_quads[1] = out_quads[2] = out_quads[3] = 0;
  for (int d = 0; d < 8; ++d) {
    if (!map_is_land_at(map, x + mapedit_neigh8_dx[d], y + mapedit_neigh8_dy[d])) {
      continue;
    }
    mask8 = (uint8_t)(mask8 | (uint8_t)(1u << d));
    if ((d & 1) == 0) {
      const int q = d >> 1;
      out_quads[q] = (uint8_t)(out_quads[q] | 4u);
      out_quads[(q + 1) & 3] = (uint8_t)(out_quads[(q + 1) & 3] | 1u);
    } else {
      const int q = ((d + 1) & 6) >> 1;
      out_quads[q] = (uint8_t)(out_quads[q] | 2u);
    }
  }
  if (out_mask8) {
    *out_mask8 = mask8;
  }
}

static void mapedit_coast_quad_offset(int q, int* out_ox, int* out_oy) {
  /* ox = ((q+1)&2)<<2; oy = (q&~1)<<2  → (0,0),(8,0),(8,8),(0,8) */
  if (out_ox) {
    *out_ox = (((q + 1) & 2) << 2);
  }
  if (out_oy) {
    *out_oy = ((q & ~1) << 2);
  }
}
#endif

#if MAP_COAST_OVERLAYS_ENABLED
static int map_phys0_coast_collect(const ColonizeWorldMap* map, int x, int y, CoastOverlay* out, int max_out) {
  const uint8_t terrain_byte = map_get_terrain(map, x, y);
  if (!map_is_ocean_index(map_decode_terrain_index(terrain_byte))) {
    return 0;
  }

  uint8_t mask8 = 0;
  uint8_t quads[4];
  mapedit_coast_masks(map, x, y, &mask8, quads);
  if (mask8 == 0) {
    return 0; /* open ocean — MAPEDIT skips coast art when FUN_1a47_01ae returns 0 */
  }

  int corner = -1;
  if ((mask8 & 0xddu) == 0xc1u) {
    corner = 0;
  }
  if ((mask8 & 0x77u) == 0x07u) {
    corner = 1;
  }
  if ((mask8 & 0x77u) == 0x70u) {
    corner = 2;
  }
  if ((mask8 & 0xddu) == 0x1cu) {
    corner = 3;
  }

  if (corner >= 0) {
    if (max_out < 1) {
      return 0;
    }
    /*
     * Corner id 0..3 = land toward NW/NE/SW/SE (2×2 with this ocean).
     * PHYS0.SS art is 150–153 in that order (transparent on the land side).
     * MAPEDIT writes 0x97+id (151–154); that is off-by-one vs this sheet.
     */
    out[0] = (CoastOverlay){PHYS0_COAST_CORNER_BASE + corner, 0, 0};
    return 1;
  }

  int count = 0;
  for (int q = 0; q < COAST_QUADS && count < max_out; ++q) {
    int ox = 0;
    int oy = 0;
    mapedit_coast_quad_offset(q, &ox, &oy);
    out[count++] = (CoastOverlay){
      PHYS0_COAST_FRAG_BASE + (int)quads[q] * 4 + q,
      ox,
      oy
    };
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

#if MAP_ESTUARY_OVERLAYS_ENABLED
static bool map_terrain_has_river_bit(uint8_t terrain_byte) {
  /* MAPEDIT neighbour test: terrain & 0x40 (FreeCol river overlays 2/3/6/7). */
  return (terrain_byte & 0x40u) != 0;
}

static int map_phys0_estuary_collect(const ColonizeWorldMap* map, int x, int y, CoastOverlay* out, int max_out) {
  const uint8_t terrain_byte = map_get_terrain(map, x, y);
  if (!map_is_ocean_index(map_decode_terrain_index(terrain_byte))) {
    return 0;
  }
  /* MAPEDIT: (original_terrain & 0xc0) != 0 */
  if ((terrain_byte & 0xc0u) == 0) {
    return 0;
  }

  const int base = ((terrain_byte & 0x80u) != 0) ? PHYS0_ESTUARY_MAJOR_BASE : PHYS0_ESTUARY_MINOR_BASE;
  int count = 0;
  for (int q = 0; q < 4 && count < max_out; ++q) {
    const int nx = x + mapedit_card_dx[q];
    const int ny = y + mapedit_card_dy[q];
    if (nx < 0 || ny < 0 || nx >= map->width || ny >= map->height) {
      continue;
    }
    const uint8_t neighbor = map_get_terrain(map, nx, ny);
    if (!map_terrain_has_river_bit(neighbor)) {
      continue;
    }
    if (!map_is_land_for_coast(map_decode_terrain_index(neighbor))) {
      continue;
    }
    /* 16×16 estuary art; MAPEDIT leaves 0x714/0x715 at 0 after coast. */
    out[count++] = (CoastOverlay){base + q, 0, 0};
  }
  return count;
}

static CoastOverlay map_phys0_estuary_layer_at(const ColonizeWorldMap* map, int x, int y, int layer) {
  CoastOverlay layers[COAST_QUADS];
  const int count = map_phys0_estuary_collect(map, x, y, layers, COAST_QUADS);
  if (layer < 0 || layer >= count) {
    return (CoastOverlay){-1, 0, 0};
  }
  return layers[layer];
}
#endif /* MAP_ESTUARY_OVERLAYS_ENABLED */

static int map_phys0_coast_layer_count_internal(const ColonizeWorldMap* map, int x, int y) {
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

static int map_phys0_estuary_layer_count(const ColonizeWorldMap* map, int x, int y) {
#if MAP_ESTUARY_OVERLAYS_ENABLED
  CoastOverlay unused[COAST_QUADS];
  return map_phys0_estuary_collect(map, x, y, unused, COAST_QUADS);
#else
  (void)map;
  (void)x;
  (void)y;
  return 0;
#endif
}

int map_phys0_coast_layer_count(const ColonizeWorldMap* map, int x, int y) {
  if (!map) {
    return 0;
  }
  return map_phys0_coast_layer_count_internal(map, x, y);
}

int map_coast_underlayer_sprite_at(const ColonizeWorldMap* map, int x, int y) {
  if (!map || map_phys0_coast_layer_count_internal(map, x, y) <= 0) {
    return -1;
  }
  const uint8_t terrain_byte = map_get_terrain(map, x, y);
  if (!map_is_ocean_index(map_decode_terrain_index(terrain_byte))) {
    return -1;
  }

  /* FUN_1a47_01ae: last cardinal land (N,E,S,W) updates the underlayer type. */
  int land_x = -1;
  int land_y = -1;
  for (int d = 0; d < 8; d += 2) {
    const int nx = x + mapedit_neigh8_dx[d];
    const int ny = y + mapedit_neigh8_dy[d];
    if (map_is_land_at(map, nx, ny)) {
      land_x = nx;
      land_y = ny;
    }
  }
  if (land_x >= 0) {
    return map_terrain_sprite_at(map, land_x, land_y);
  }
  /* Diagonal-only coast: MAPEDIT keeps ocean as the 05b2 underlayer. */
  return map_terrain_sprite_at(map, x, y);
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

bool map_alloc(ColonizeWorldMap* out_map, uint8_t width, uint8_t height, char* err, size_t err_size) {
  if (!out_map || width == 0 || height == 0) {
    if (err && err_size) {
      snprintf(err, err_size, "map_alloc bad args");
    }
    return false;
  }
  map_free(out_map);
  const size_t tile_count = (size_t)width * (size_t)height;
  uint8_t* terrain = calloc(tile_count, 1);
  uint8_t* layer2 = calloc(tile_count, 1);
  uint8_t* layer3 = calloc(tile_count, 1);
  if (!terrain || !layer2 || !layer3) {
    free(terrain);
    free(layer2);
    free(layer3);
    if (err && err_size) {
      snprintf(err, err_size, "oom in map_alloc");
    }
    return false;
  }
  out_map->width = width;
  out_map->height = height;
  out_map->terrain = terrain;
  out_map->layer2 = layer2;
  out_map->layer3 = layer3;
  out_map->tile_count = tile_count;
  if (err && err_size) {
    err[0] = '\0';
  }
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

  /* Inland hills / mountains / rivers (not ocean mouths). */
  if (overlay != 0 && overlay != 4) {
    if (overlay_is_hill(overlay, terrain_byte) || overlay_is_mountain(overlay, terrain_byte)) {
      ++count;
    }
    if ((overlay_is_minor_river(overlay) || overlay_is_major_river(overlay))
        && !map_is_ocean_index(map_decode_terrain_index(terrain_byte))) {
      ++count;
    }
  }

  /* MAPEDIT: estuary after coast on ocean tiles with terrain & 0xc0. */
  count += map_phys0_estuary_layer_count(map, x, y);
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
#if MAP_COAST_OVERLAYS_ENABLED
    return map_phys0_coast_layer_at(map, x, y, layer).sprite;
#else
    return -1;
#endif
  }
  int feature_layer = coast_layers;

  if (map_has_special_mountain_marker(map, x, y)) {
    if (layer == feature_layer) {
      return PHYS0_MOUNTAIN_ISOLATED;
    }
    ++feature_layer;
  }

  if (overlay != 0 && overlay != 4) {
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

    if ((overlay_is_minor_river(overlay) || overlay_is_major_river(overlay))
        && !map_is_ocean_index(map_decode_terrain_index(terrain_byte))) {
      if (layer == feature_layer) {
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
  }

  const int estuary_layers = map_phys0_estuary_layer_count(map, x, y);
  const int estuary_index = layer - feature_layer;
  if (estuary_index >= 0 && estuary_index < estuary_layers) {
#if MAP_ESTUARY_OVERLAYS_ENABLED
    return map_phys0_estuary_layer_at(map, x, y, estuary_index).sprite;
#else
    return -1;
#endif
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

  const uint8_t terrain_byte = map_get_terrain(map, x, y);
  const uint8_t overlay = map_terrain_overlay(terrain_byte);
  const int coast_layers = map_phys0_coast_layer_count(map, x, y);
  if (layer < coast_layers) {
#if MAP_COAST_OVERLAYS_ENABLED
    const CoastOverlay coast = map_phys0_coast_layer_at(map, x, y, layer);
    if (out_ox) {
      *out_ox = coast.ox;
    }
    if (out_oy) {
      *out_oy = coast.oy;
    }
#endif
    return;
  }
  int feature_layer = coast_layers;

  if (map_has_special_mountain_marker(map, x, y)) {
    ++feature_layer;
  }
  if (overlay != 0 && overlay != 4) {
    if (overlay_is_hill(overlay, terrain_byte) || overlay_is_mountain(overlay, terrain_byte)) {
      ++feature_layer;
    }
    if ((overlay_is_minor_river(overlay) || overlay_is_major_river(overlay))
        && !map_is_ocean_index(map_decode_terrain_index(terrain_byte))) {
      ++feature_layer;
    }
  }

  const int estuary_layers = map_phys0_estuary_layer_count(map, x, y);
  const int estuary_index = layer - feature_layer;
  if (estuary_index >= 0 && estuary_index < estuary_layers) {
#if MAP_ESTUARY_OVERLAYS_ENABLED
    const CoastOverlay est = map_phys0_estuary_layer_at(map, x, y, estuary_index);
    if (out_ox) {
      *out_ox = est.ox;
    }
    if (out_oy) {
      *out_oy = est.oy;
    }
#endif
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
