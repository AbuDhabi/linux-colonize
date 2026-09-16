#include <stdio.h>
#include <string.h>

#include "core/map_gen.h"
#include "platform/diagnostics.h"

#include "../common/test_runner.h"

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

/* Fixed customize-style mid settings shared by every case that needs a
 * generated map. Each case calls setup() itself, which frees any
 * previously generated s_map and regenerates from scratch (deterministic,
 * fixed seed) -- so cases never depend on state a previous case left
 * behind. */
static MapGenParams s_params;
static ColonizeWorldMap s_map;
static int s_map_ready = 0;

static int setup(void) {
  char err[256];
  if (s_map_ready) {
    map_free(&s_map);
    s_map_ready = 0;
  }
  memset(&s_params, 0, sizeof(s_params));
  s_params.land_mass = 1;
  s_params.land_form = 1;
  s_params.temperature = 1;
  s_params.climate = 1;
  s_params.forest_extra = 1;
  s_params.seed = 0xC01A71Eu;

  memset(&s_map, 0, sizeof(s_map));
  if (!map_generate(&s_map, &s_params, err, sizeof(err))) {
    fprintf(stderr, "map_generate failed: %s\n", err);
    return 0;
  }
  s_map_ready = 1;
  return 1;
}

static int test_params_random_in_range(void) {
  MapGenParams params;
  memset(&params, 0, sizeof(params));
  map_gen_params_random(&params, 0xC01A71Eu);
  /* NEW WORLD axes are FUN_281f_04d4(0,3) -> 0..3 (CUSTOMIZE UI stays 0..2). */
  if (params.land_mass < 0 || params.land_mass > 3 || params.land_form < 0 || params.land_form > 3 ||
      params.temperature < 0 || params.temperature > 3 || params.climate < 0 || params.climate > 3 ||
      params.forest_extra < 0 || params.forest_extra > 3) {
    fprintf(stderr, "map_gen_params_random out of range\n");
    return 1;
  }
  return 0;
}

static int test_dims_and_land_budget(void) {
  if (!setup()) {
    return 1;
  }
  const int budget = (s_params.land_form + s_params.land_mass + 1) * 0x140;

  if (s_map.width != MAP_GEN_WIDTH || s_map.height != MAP_GEN_HEIGHT) {
    fprintf(stderr, "dims %dx%d expected %dx%d\n", s_map.width, s_map.height, MAP_GEN_WIDTH, MAP_GEN_HEIGHT);
    map_free(&s_map);
    s_map_ready = 0;
    return 1;
  }

  const int land = count_land(&s_map);
  const int ocean = count_ocean(&s_map);
  int rc = 0;
  if (ocean < 200) {
    fprintf(stderr, "expected ocean tiles, got %d\n", ocean);
    rc = 1;
  }
  /* Cleanup + extras: land near budget, not empty / not full map. */
  if (land < budget / 4 || land > budget + 600) {
    fprintf(stderr, "land %d outside expected band around budget %d\n", land, budget);
    rc = 1;
  }
  if (rc == 0) {
    diag_info("unit_map_gen dims/budget ok land=%d budget=%d", land, budget);
  }
  return rc;
}

static int test_euro_landfall_nation0_is_water(void) {
  if (!setup()) {
    return 1;
  }
  int sx = -1, sy = -1;
  if (!map_gen_euro_landfall(&s_map, 0, &sx, &sy)) {
    fprintf(stderr, "map_gen_euro_landfall failed\n");
    return 1;
  }
  if (!map_tile_is_high_seas(&s_map, sx, sy) && !map_tile_is_water(&s_map, sx, sy)) {
    fprintf(stderr, "euro landfall (%d,%d) is not water/HS\n", sx, sy);
    return 1;
  }
  return 0;
}

static int test_euro_landfall_bands_distinct(void) {
  if (!setup()) {
    return 1;
  }
  const int h = (int)s_map.height;
  const int h5 = h / 5;
  int seen_band[4] = {0, 0, 0, 0};
  const int bands[4] = {h5, h5 * 2, h5 * 3, h5 * 4};
  for (int n = 0; n < 4; ++n) {
    int lx = -1, ly = -1;
    if (!map_gen_euro_landfall(&s_map, n, &lx, &ly)) {
      fprintf(stderr, "euro landfall missing for nation %d\n", n);
      return 1;
    }
    int band = -1;
    for (int b = 0; b < 4; ++b) {
      if (ly == bands[b]) {
        band = b;
        break;
      }
    }
    if (band < 0) {
      fprintf(stderr, "nation %d landfall Y=%d not in {14,28,42,56}-style bands\n", n, ly);
      return 1;
    }
    if (seen_band[band]) {
      fprintf(stderr, "duplicate landfall band %d\n", band);
      return 1;
    }
    seen_band[band] = 1;
  }
  return 0;
}

static int test_pick_start_fallback_is_land(void) {
  if (!setup()) {
    return 1;
  }
  int sx = -1, sy = -1;
  if (!map_gen_pick_start(&s_map, 0, -1, -1, 0, &sx, &sy)) {
    fprintf(stderr, "map_gen_pick_start failed\n");
    return 1;
  }
  if (!map_tile_is_land(&s_map, sx, sy)) {
    fprintf(stderr, "pick_start (%d,%d) is not land\n", sx, sy);
    return 1;
  }
  diag_info("unit_map_gen ok start=(%d,%d)", sx, sy);
  return 0;
}

static int test_seed_reproducibility(void) {
  if (!setup()) {
    return 1;
  }
  char err[256];
  ColonizeWorldMap map2;
  memset(&map2, 0, sizeof(map2));
  if (!map_generate(&map2, &s_params, err, sizeof(err))) {
    fprintf(stderr, "second map_generate failed: %s\n", err);
    return 1;
  }
  const int equal = maps_equal(&s_map, &map2);
  map_free(&map2);
  if (!equal) {
    fprintf(stderr, "same seed did not reproduce terrain\n");
    return 1;
  }
  return 0;
}

static const TestCase k_cases[] = {
    {"test_params_random_in_range", test_params_random_in_range},
    {"test_dims_and_land_budget", test_dims_and_land_budget},
    {"test_euro_landfall_nation0_is_water", test_euro_landfall_nation0_is_water},
    {"test_euro_landfall_bands_distinct", test_euro_landfall_bands_distinct},
    {"test_pick_start_fallback_is_land", test_pick_start_fallback_is_land},
    {"test_seed_reproducibility", test_seed_reproducibility},
};

TEST_MAIN(k_cases)
