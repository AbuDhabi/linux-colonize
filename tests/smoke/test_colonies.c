#include <stdio.h>
#include <string.h>

#include "core/colony.h"
#include "core/map.h"
#include "platform/diagnostics.h"

static int failures = 0;

#define CHECK(cond, msg) \
  do { \
    if (!(cond)) { \
      fprintf(stderr, "FAIL: %s\n", (msg)); \
      failures++; \
    } else { \
      printf("OK: %s\n", (msg)); \
    } \
  } while (0)

int main(void) {
  diag_init(0, NULL);

  /* Load map. */
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char map_err[256];
  const bool map_ok = map_load_mp("COLONIZE/AMER2.MP", &map, map_err, sizeof(map_err));
  CHECK(map_ok, "load AMER2.MP");
  if (!map_ok) {
    fprintf(stderr, "map error: %s\n", map_err);
    return 1;
  }

  /* Init colony pool. */
  ColonizeColonyPool pool;
  colonies_init(&pool);
  CHECK(pool.colony_count == 0, "pool starts empty");

  /* Load names from COLONY.TXT. */
  const bool names_ok = colonies_load_names(&pool, "COLONIZE/COLONY.TXT");
  CHECK(names_ok, "load COLONY.TXT names");
  CHECK(pool.name_count > 0, "at least one colony name loaded");

  /* Find a land tile. */
  int land_x = -1, land_y = -1;
  for (int y = 0; y < (int)map.height && land_x < 0; ++y) {
    for (int x = 0; x < (int)map.width && land_x < 0; ++x) {
      if (map_tile_is_land(&map, x, y)) {
        land_x = x;
        land_y = y;
      }
    }
  }
  CHECK(land_x >= 0, "found a land tile");

  /* Cannot found on water. */
  int water_x = -1, water_y = -1;
  for (int y = 0; y < (int)map.height && water_x < 0; ++y) {
    for (int x = 0; x < (int)map.width && water_x < 0; ++x) {
      if (map_tile_is_water(&map, x, y)) {
        water_x = x;
        water_y = y;
      }
    }
  }
  if (water_x >= 0) {
    CHECK(!colonies_can_found(&pool, &map, water_x, water_y), "cannot found on water");
  }

  /* Can found on land. */
  CHECK(colonies_can_found(&pool, &map, land_x, land_y), "can found on land tile");

  /* Found a colony. */
  const int cid = colonies_found(&pool, &map, land_x, land_y);
  CHECK(cid >= 0, "colonies_found returns valid id");
  CHECK(pool.colony_count == 1, "pool now has one colony");

  /* Lookup by id. */
  const ColonizeColony* col = colonies_get(&pool, cid);
  CHECK(col != NULL, "colonies_get returns colony");
  CHECK(col->active, "colony is active");
  CHECK(col->x == land_x && col->y == land_y, "colony at expected coordinates");
  CHECK(col->name[0] != '\0', "colony has a name");
  printf("  colony name: %s at (%d,%d)\n", col->name, col->x, col->y);

  /* Cannot found a second colony on the same tile. */
  CHECK(!colonies_can_found(&pool, &map, land_x, land_y), "cannot found on occupied tile");

  /* id_at lookup. */
  CHECK(colonies_id_at(&pool, land_x, land_y) == cid, "colonies_id_at returns correct id");
  CHECK(colonies_id_at(&pool, land_x + 1, land_y) < 0, "colonies_id_at returns -1 for empty tile");

  map_free(&map);

  if (failures == 0) {
    printf("smoke_colonies: all checks passed\n");
    return 0;
  }
  fprintf(stderr, "smoke_colonies: %d failure(s)\n", failures);
  return 1;
}
