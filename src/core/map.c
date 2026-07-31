#include "core/map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/diagnostics.h"

/*
 * .MP terrain byte (FreeCol ColonizationMapLoader):
 *   bits 0-2: cleared land type 0-7
 *   bit 3 (8): forest flag (with bits 0-2 -> forest terrain index 8-15)
 *   bit 4 (16): hill/mountain base flag (with bits 0-2)
 *   bits 5-7: hill / river / mountain overlays
 *
 * Ocean coast / estuary PHYS0: MAPEDIT.EXE FUN_1a47_0932 (no RTLink).
 * Coast: FUN_1a47_01ae → fragments / corners; estuary mouths toward land rivers.
 * Forest/hill/mountain/river: FUN_1a47_0418 / 036e / 030e; mask N=8,S=4,W=2,E=1.
 *
 * MAPEDIT passes 1-based PHYS0 IDs (1..nsprites). This port and the debug atlas use
 * 0-based indices; convert with mapedit_phys0_index(). Isolated forest/hill/mountain
 * art is therefore 64/48/32, not MAPEDIT immediates 0x41/0x31/0x21.
 */

/* 0-based sheet indices (= MAPEDIT ID − 1). Shared adjacency: sprite = base + mask. */
#define PHYS0_MAJOR_RIVER_BASE 0
#define PHYS0_MINOR_RIVER_BASE 16
#define PHYS0_MOUNTAIN_BASE 32
#define PHYS0_HILL_BASE 48
#define PHYS0_FOREST_BASE 64
#define PHYS0_MOUNTAIN_ISOLATED 36 /* layer-3 arctic marker only */
#define PHYS0_TUNDRA_CANOPY 64 /* isolated forest canopy on y=0 */

#if MAP_COAST_OVERLAYS_ENABLED || MAP_ESTUARY_OVERLAYS_ENABLED
#define PHYS0_COAST_FRAG_BASE 108 /* MAPEDIT 0x6d − 1 */
#define PHYS0_COAST_CORNER_BASE 150 /* MAPEDIT 0x97 − 1 → 150–153 */
#define PHYS0_ESTUARY_MAJOR_BASE 140 /* MAPEDIT 0x8d − 1 */
#define PHYS0_ESTUARY_MINOR_BASE 144 /* MAPEDIT 0x8d+4 − 1 */
#define COAST_QUADS 4
#endif

static int mapedit_phys0_index(int mapedit_id) {
  return mapedit_id - 1;
}

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

static bool map_is_ocean_index(int terrain_index) {
  return terrain_index == MAP_OCEAN_INDEX || terrain_index == MAP_HIGH_SEAS_INDEX;
}

static bool map_is_land_for_coast(int terrain_index) {
  return !map_is_ocean_index(terrain_index);
}

/* MAPEDIT: non-scrub forest (indices 8–23 except type 1). */
static bool map_byte_is_connective_forest(uint8_t terrain_byte) {
  const int terrain_index = map_decode_terrain_index(terrain_byte);
  return map_is_forest_index(terrain_index) && map_forest_type(terrain_index) != 1;
}

/* MAPEDIT hill/mountain: terrain & 0x20; mountain when & 0x80 as well. */
static bool map_byte_is_hill_or_mountain(uint8_t terrain_byte) {
  return (terrain_byte & 0x20u) != 0;
}

static bool map_byte_is_mountain(uint8_t terrain_byte) {
  return (terrain_byte & 0xa0u) == 0xa0u;
}

static bool map_byte_is_hill(uint8_t terrain_byte) {
  return map_byte_is_hill_or_mountain(terrain_byte) && !map_byte_is_mountain(terrain_byte);
}

static bool map_byte_has_river(uint8_t terrain_byte) {
  return (terrain_byte & 0x40u) != 0;
}

static bool overlay_is_hill(uint8_t overlay, uint8_t terrain_byte) {
  (void)overlay;
  return map_byte_is_hill(terrain_byte);
}

static bool overlay_is_mountain(uint8_t overlay, uint8_t terrain_byte) {
  (void)overlay;
  return map_byte_is_mountain(terrain_byte);
}

/*
 * MAPEDIT cardinal mask (FUN_1a47_030e / 036e / 0418): N=8, S=4, W=2, E=1.
 */
static uint8_t map_cardinal_mask(
  const ColonizeWorldMap* map,
  int x,
  int y,
  bool (*matches)(uint8_t tile_byte, uint8_t self_byte),
  uint8_t self_byte
) {
  uint8_t mask = 0;
  static const int dx[4] = {0, 0, -1, 1};
  static const int dy[4] = {-1, 1, 0, 0};
  static const uint8_t bits[4] = {8u, 4u, 2u, 1u}; /* N,S,W,E */
  for (int dir = 0; dir < 4; ++dir) {
    const int nx = x + dx[dir];
    const int ny = y + dy[dir];
    if (nx < 0 || ny < 0 || nx >= map->width || ny >= map->height) {
      continue;
    }
    const uint8_t neighbor = map_get_terrain(map, nx, ny);
    if (matches(neighbor, self_byte)) {
      mask = (uint8_t)(mask | bits[dir]);
    }
  }
  return mask;
}

static bool forest_neighbor(uint8_t tile_byte, uint8_t self_byte) {
  (void)self_byte;
  return map_byte_is_connective_forest(tile_byte);
}

static bool hill_mountain_neighbor(uint8_t tile_byte, uint8_t self_byte) {
  /* FUN_1a47_036e: (neighbor & 0xa0) == (self & 0xa0). */
  return (tile_byte & 0xa0u) == (self_byte & 0xa0u);
}

static bool river_bit_neighbor(uint8_t tile_byte, uint8_t self_byte) {
  (void)self_byte;
  return map_byte_has_river(tile_byte);
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
     * Corner id 0..3 = land toward NW/NE/SW/SE.
     * MAPEDIT ID 0x97+id (151–154) → 0-based index 150–153.
     */
    out[0] = (CoastOverlay){mapedit_phys0_index(0x97 + corner), 0, 0};
    return 1;
  }

  int count = 0;
  for (int q = 0; q < COAST_QUADS && count < max_out; ++q) {
    int ox = 0;
    int oy = 0;
    mapedit_coast_quad_offset(q, &ox, &oy);
    out[count++] = (CoastOverlay){
      mapedit_phys0_index(0x6d + (int)quads[q] * 4 + q),
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

  const int mapedit_base = ((terrain_byte & 0x80u) != 0) ? 0x8d : 0x8d + 4;
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
    out[count++] = (CoastOverlay){mapedit_phys0_index(mapedit_base + q), 0, 0};
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

static int phys0_connectivity_sprite(int base, uint8_t mask) {
  /* Shared MAPEDIT layout: base + mask; rivers leave base+0 blank so 0 → 15. */
  uint8_t m = mask & 0x0fu;
  if (m == 0 && (base == PHYS0_MAJOR_RIVER_BASE || base == PHYS0_MINOR_RIVER_BASE)) {
    m = 0x0fu;
  }
  return base + (int)m;
}

static bool map_has_special_mountain_marker(const ColonizeWorldMap* map, int x, int y) {
  /* AMER2 has one arctic tile tagged in layer 3 that DOS draws with mountain art. */
  return map_get_layer3(map, x, y) == 0x0eu;
}

static int phys0_river_sprite(uint8_t terrain_byte, uint8_t mask) {
  const int base = ((terrain_byte & 0x80u) != 0) ? PHYS0_MAJOR_RIVER_BASE : PHYS0_MINOR_RIVER_BASE;
  return phys0_connectivity_sprite(base, mask);
}

static int phys0_hill_mountain_sprite(uint8_t terrain_byte, uint8_t mask) {
  const int base = ((terrain_byte & 0x80u) != 0) ? PHYS0_MOUNTAIN_BASE : PHYS0_HILL_BASE;
  return phys0_connectivity_sprite(base, mask);
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
  const int terrain_index = map_decode_terrain_index(terrain_byte);

  if (map_is_ocean_index(terrain_index)) {
    return -1;
  }

  /* MAPEDIT FUN_1a47_0418 / 03de: any non-scrub forest, sprite 64 + mask. */
  if (map_byte_is_connective_forest(terrain_byte)) {
    const uint8_t mask = map_cardinal_mask(map, x, y, forest_neighbor, terrain_byte);
    return phys0_connectivity_sprite(PHYS0_FOREST_BASE, mask);
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
  int count = map_phys0_coast_layer_count(map, x, y);

  if (map_has_special_mountain_marker(map, x, y)) {
    ++count;
  }

  if (!map_is_ocean_index(map_decode_terrain_index(terrain_byte))) {
    if (map_byte_is_hill_or_mountain(terrain_byte)) {
      ++count;
    }
    if (map_byte_has_river(terrain_byte)) {
      ++count;
    }
  }

  count += map_phys0_estuary_layer_count(map, x, y);
  return count;
}

int map_phys0_overlay_sprite_at(const ColonizeWorldMap* map, int x, int y, int layer) {
  if (!map || layer < 0) {
    return -1;
  }

  const uint8_t terrain_byte = map_get_terrain(map, x, y);
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

  if (!map_is_ocean_index(map_decode_terrain_index(terrain_byte))) {
    if (map_byte_is_hill_or_mountain(terrain_byte)) {
      if (layer == feature_layer) {
        const uint8_t mask =
          map_cardinal_mask(map, x, y, hill_mountain_neighbor, terrain_byte);
        return phys0_hill_mountain_sprite(terrain_byte, mask);
      }
      ++feature_layer;
    }

    if (map_byte_has_river(terrain_byte)) {
      if (layer == feature_layer) {
        const uint8_t mask = map_cardinal_mask(map, x, y, river_bit_neighbor, terrain_byte);
        return phys0_river_sprite(terrain_byte, mask);
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
  if (!map_is_ocean_index(map_decode_terrain_index(terrain_byte))) {
    if (map_byte_is_hill_or_mountain(terrain_byte)) {
      ++feature_layer;
    }
    if (map_byte_has_river(terrain_byte)) {
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
