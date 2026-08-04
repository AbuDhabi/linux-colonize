#include <stdio.h>
#include <string.h>

#include "core/map_gen.h"
#include "platform/diagnostics.h"

static int count_land(const ColonizeWorldMap* map) {
  int n = 0;
  for (int y = 0; y < map->height; ++y) {
    for (int x = 0; x < map->width; ++x) {
      if (map_tile_is_land(map, x, y)) {
        n++;
      }
    }
  }
  return n;
}

static int count_ocean(const ColonizeWorldMap* map) {
  int n = 0;
  for (int y = 0; y < map->height; ++y) {
    for (int x = 0; x < map->width; ++x) {
      if (map_tile_is_water(map, x, y)) {
        n++;
      }
    }
  }
  return n;
}

static int maps_equal(const ColonizeWorldMap* a, const ColonizeWorldMap* b) {
  if (a->width != b->width || a->height != b->height) {
    return 0;
  }
  const size_t n = (size_t)a->width * (size_t)a->height;
  return memcmp(a->terrain, b->terrain, n) == 0;
}

int main(void) {
  char err[256];
  MapGenParams params;
  memset(&params, 0, sizeof(params));
  map_gen_params_random(&params, 0xC01A71Eu);
  /* NEW WORLD axes are FUN_281f_04d4(0,3) → 0..3 (CUSTOMIZE UI stays 0..2). */
  if (params.land_mass < 0 || params.land_mass > 3 || params.land_form < 0 || params.land_form > 3 ||
      params.temperature < 0 || params.temperature > 3 || params.climate < 0 || params.climate > 3 ||
      params.forest_extra < 0 || params.forest_extra > 3) {
    fprintf(stderr, "map_gen_params_random out of range\n");
    return 1;
  }

  /* Fixed customize-style mid settings for stable land-budget assertions. */
  params.land_mass = 1;
  params.land_form = 1;
  params.temperature = 1;
  params.climate = 1;
  params.forest_extra = 1;
  params.seed = 0xC01A71Eu;

  const int budget = (params.land_form + params.land_mass + 1) * 0x140;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  if (!map_generate(&map, &params, err, sizeof(err))) {
    fprintf(stderr, "map_generate failed: %s\n", err);
    return 1;
  }

  if (map.width != MAP_GEN_WIDTH || map.height != MAP_GEN_HEIGHT) {
    fprintf(stderr, "dims %dx%d expected %dx%d\n", map.width, map.height, MAP_GEN_WIDTH, MAP_GEN_HEIGHT);
    map_free(&map);
    return 1;
  }

  const int land = count_land(&map);
  const int ocean = count_ocean(&map);
  if (ocean < 200) {
    fprintf(stderr, "expected ocean tiles, got %d\n", ocean);
    map_free(&map);
    return 1;
  }
  /* Cleanup + extras: land near budget, not empty / not full map. */
  if (land < budget / 4 || land > budget + 600) {
    fprintf(stderr, "land %d outside expected band around budget %d\n", land, budget);
    map_free(&map);
    return 1;
  }

  int sx = -1, sy = -1;
  if (!map_gen_pick_start(&map, 0, -1, -1, 0, &sx, &sy)) {
    fprintf(stderr, "map_gen_pick_start failed\n");
    map_free(&map);
    return 1;
  }
  if (!map_tile_is_land(&map, sx, sy)) {
    fprintf(stderr, "start (%d,%d) is not land\n", sx, sy);
    map_free(&map);
    return 1;
  }

  ColonizeWorldMap map2;
  memset(&map2, 0, sizeof(map2));
  if (!map_generate(&map2, &params, err, sizeof(err))) {
    fprintf(stderr, "second map_generate failed: %s\n", err);
    map_free(&map);
    return 1;
  }
  if (!maps_equal(&map, &map2)) {
    fprintf(stderr, "same seed did not reproduce terrain\n");
    map_free(&map);
    map_free(&map2);
    return 1;
  }

  map_free(&map);
  map_free(&map2);
  diag_info("smoke_map_gen ok land=%d budget=%d start=(%d,%d)", land, budget, sx, sy);
  return 0;
}
