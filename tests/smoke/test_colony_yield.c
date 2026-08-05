#include "core/colony.h"
#include "core/colony_yield.h"
#include "core/map.h"

#include <stdio.h>
#include <string.h>

/*
 * Col1 town-commons fixtures (live observations):
 *   Scrub Forest:              3 food + 3 furs
 *   Hills:                     4 food + 5 ore
 *   Broadleaf Forest:          4 food + 3 furs
 *   Prairie + Minor River:     5 food + 5 cotton
 *   Broadleaf Forest + Game:   6 food + 5 furs
 */

static int find_resource_tile(
  ColonizeWorldMap* map,
  uint8_t terrain,
  int want_res,
  int* out_x,
  int* out_y
) {
  for (int y = 0; y < map->height; ++y) {
    for (int x = 0; x < map->width; ++x) {
      map->terrain[y * map->width + x] = terrain;
      if (map_resource_type_for_yield(map, x, y) == want_res) {
        *out_x = x;
        *out_y = y;
        return 1;
      }
    }
  }
  return 0;
}

static int check_commons(
  ColonizeWorldMap* map,
  int x,
  int y,
  int expect_food,
  int expect_cargo,
  int expect_amt,
  const char* label
) {
  ColonizeTownCommonsYield tc;
  colony_yield_town_commons(map, x, y, &tc);
  if (tc.food != expect_food || tc.secondary_cargo != expect_cargo ||
      tc.secondary_amount != expect_amt) {
    fprintf(
      stderr,
      "town commons %s: expected food=%d cargo=%d amt=%d got food=%d cargo=%d amt=%d\n",
      label,
      expect_food,
      expect_cargo,
      expect_amt,
      tc.food,
      tc.secondary_cargo,
      tc.secondary_amount
    );
    return 1;
  }
  return 0;
}

int main(void) {
  char err[256];
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  if (!map_alloc(&map, 32, 32, err, sizeof(err))) {
    fprintf(stderr, "map_alloc failed: %s\n", err);
    return 1;
  }

  /* Scrub forest (pedia 9) — no special / river. */
  map.terrain[0] = 9;
  if (check_commons(
        &map,
        0,
        0,
        3,
        COLONIZE_CARGO_FURS,
        3,
        "scrub"
      )) {
    map_free(&map);
    return 1;
  }

  /* Hills (bit 0x20). */
  map.terrain[1] = (uint8_t)(0x20u);
  if (map_pedia_terrain_index_at(&map, 1, 0) != 28) {
    fprintf(stderr, "hills pedia expected 28 got %d\n", map_pedia_terrain_index_at(&map, 1, 0));
    map_free(&map);
    return 1;
  }
  if (check_commons(&map, 1, 0, 4, COLONIZE_CARGO_ORE, 5, "hills")) {
    map_free(&map);
    return 1;
  }

  /* Broadleaf forest (pedia 11). */
  map.terrain[2] = 11;
  if (check_commons(&map, 2, 0, 4, COLONIZE_CARGO_FURS, 3, "broadleaf")) {
    map_free(&map);
    return 1;
  }

  /* Prairie (3) + minor river (0x40). */
  map.terrain[3] = (uint8_t)(3u | 0x40u);
  if (!map_tile_has_river(&map, 3, 0) || map_tile_has_major_river(&map, 3, 0)) {
    fprintf(stderr, "prairie tile should be minor river only\n");
    map_free(&map);
    return 1;
  }
  if (check_commons(&map, 3, 0, 5, COLONIZE_CARGO_COTTON, 5, "prairie+minor river")) {
    map_free(&map);
    return 1;
  }

  /* Broadleaf + Game (resource type 9). Find a procedural hit. */
  {
    int gx = -1;
    int gy = -1;
    if (!find_resource_tile(&map, 11, 9, &gx, &gy)) {
      fprintf(stderr, "no broadleaf+Game procedural tile found on 32x32\n");
      map_free(&map);
      return 1;
    }
    if (check_commons(&map, gx, gy, 6, COLONIZE_CARGO_FURS, 5, "broadleaf+Game")) {
      map_free(&map);
      return 1;
    }
    /* Settlement bit hides sprites but yields must still see Game. */
    map.layer2[gy * map.width + gx] = (uint8_t)(map.layer2[gy * map.width + gx] | 2u);
    if (map_resource_type_at(&map, gx, gy) >= 0) {
      fprintf(stderr, "settlement bit should hide resource sprite lookup\n");
      map_free(&map);
      return 1;
    }
    if (map_resource_type_for_yield(&map, gx, gy) != 9) {
      fprintf(stderr, "yield lookup should still see Game under settlement bit\n");
      map_free(&map);
      return 1;
    }
    if (check_commons(
          &map,
          gx,
          gy,
          6,
          COLONIZE_CARGO_FURS,
          5,
          "broadleaf+Game (settlement bit)"
        )) {
      map_free(&map);
      return 1;
    }
  }

  /* Field river: prairie cotton +1 with minor river. */
  {
    map.terrain[5] = 3;
    const int cotton_dry = colony_yield_for_tile(&map, 5, 0, COLONIZE_JOB_COTTON_PLANTER);
    map.terrain[5] = (uint8_t)(3u | 0x40u);
    const int cotton_river = colony_yield_for_tile(&map, 5, 0, COLONIZE_JOB_COTTON_PLANTER);
    if (cotton_dry != 3 || cotton_river != 4) {
      fprintf(
        stderr,
        "field prairie cotton dry=%d river=%d expected 3/4\n",
        cotton_dry,
        cotton_river
      );
      map_free(&map);
      return 1;
    }
  }

  map_free(&map);
  printf("colony_yield town commons tests ok\n");
  return 0;
}
