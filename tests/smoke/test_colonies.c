#include <stdio.h>
#include <string.h>

#include "core/assets.h"
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

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char map_err[256];
  const bool map_ok = map_load_mp("COLONIZE/AMER2.MP", &map, map_err, sizeof(map_err));
  CHECK(map_ok, "load AMER2.MP");
  if (!map_ok) {
    fprintf(stderr, "map error: %s\n", map_err);
    return 1;
  }

  ColonizeColonyPool pool;
  colonies_init(&pool);
  CHECK(pool.colony_count == 0, "pool starts empty");

  CHECK(colonies_load_names(&pool, "COLONIZE/COLONY.TXT"), "load COLONY.TXT names");
  CHECK(pool.name_count > 0, "at least one colony name loaded");

  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  CHECK(assets_msg_load_file(&names, "COLONIZE/NAMES.TXT"), "load NAMES.TXT");
  CHECK(colonies_load_buildings(&pool, &names), "load @BUILDING");
  CHECK(pool.building_type_count > 8, "enough building types");
  const int town_hall = colonies_find_building(&pool, "Town Hall");
  const int carpenter = colonies_find_building(&pool, "Carpenter's Shop");
  const int warehouse = colonies_find_building(&pool, "Warehouse");
  const int stockade = colonies_find_building(&pool, "Stockade");
  const int docks = colonies_find_building(&pool, "Docks");
  CHECK(town_hall >= 0 && carpenter >= 0, "starter building names resolve");
  CHECK(warehouse >= 0 && stockade >= 0 && docks >= 0, "fence/docks/warehouse names resolve");

  int land_x = -1, land_y = -1;
  for (int y = 0; y < (int)map.height && land_x < 0; ++y) {
    for (int x = 0; x < (int)map.width && land_x < 0; ++x) {
      if (map_tile_is_land(&map, x, y) && !map_tile_is_coastal(&map, x, y)) {
        land_x = x;
        land_y = y;
      }
    }
  }
  if (land_x < 0) {
    for (int y = 0; y < (int)map.height && land_x < 0; ++y) {
      for (int x = 0; x < (int)map.width && land_x < 0; ++x) {
        if (map_tile_is_land(&map, x, y)) {
          land_x = x;
          land_y = y;
        }
      }
    }
  }
  CHECK(land_x >= 0, "found a land tile");

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

  CHECK(colonies_can_found(&pool, &map, land_x, land_y), "can found on land tile");

  /* Found without a colonist (type -1) — still gets starter buildings. */
  const int empty_id = colonies_found(&pool, &map, land_x, land_y, -1, 0, 0, 0);
  CHECK(empty_id >= 0, "colonies_found without founder");
  const ColonizeColony* empty = colonies_get(&pool, empty_id);
  CHECK(empty && empty->population == 0 && empty->colonist_count == 0, "no founder => pop 0");
  CHECK(empty && empty->has_building[town_hall], "starter includes Town Hall");
  CHECK(empty && empty->has_building[carpenter], "starter includes Carpenter's Shop");
  CHECK(empty && !empty->has_building[stockade], "starter excludes Stockade");
  CHECK(empty && !empty->has_building[warehouse], "starter excludes Warehouse");
  CHECK(empty && !empty->has_building[docks], "starter excludes Docks");
  CHECK(empty && empty->stock[COLONIZE_CARGO_FOOD] == 200, "starter food stockpile");

  /* Coastal colony still does not get free Docks (upgrade only). */
  int coast_x = -1, coast_y = -1;
  for (int y = 0; y < (int)map.height && coast_x < 0; ++y) {
    for (int x = 0; x < (int)map.width && coast_x < 0; ++x) {
      if (map_tile_is_coastal(&map, x, y) && colonies_can_found(&pool, &map, x, y)) {
        coast_x = x;
        coast_y = y;
      }
    }
  }
  if (coast_x >= 0) {
    const int coast_id = colonies_found(&pool, &map, coast_x, coast_y, -1, 0, 0, 0);
    CHECK(coast_id >= 0, "found coastal colony");
    const ColonizeColony* coastal = colonies_get(&pool, coast_id);
    CHECK(coastal && !coastal->has_building[docks], "coastal starter excludes Docks");
    CHECK(coastal && !coastal->has_building[warehouse], "coastal starter excludes Warehouse");
  } else {
    printf("OK: skip coastal founding check (no free coastal tile)\n");
  }

  /* Another colony with a founder on a different land tile. */
  int land2_x = -1, land2_y = -1;
  for (int y = 0; y < (int)map.height && land2_x < 0; ++y) {
    for (int x = 0; x < (int)map.width && land2_x < 0; ++x) {
      if (map_tile_is_land(&map, x, y) && colonies_can_found(&pool, &map, x, y)) {
        land2_x = x;
        land2_y = y;
      }
    }
  }
  CHECK(land2_x >= 0, "second land tile");
  const int pioneer_type = 2; /* Pioneers are early in @UNIT; index used only for storage. */
  const int cid = colonies_found(&pool, &map, land2_x, land2_y, pioneer_type, 100, 0, 0);
  CHECK(cid >= 0, "colonies_found with founder");
  const ColonizeColony* col = colonies_get(&pool, cid);
  CHECK(col != NULL, "colonies_get returns colony");
  CHECK(col->active, "colony is active");
  CHECK(col->x == land2_x && col->y == land2_y, "colony at expected coordinates");
  CHECK(col->name[0] != '\0', "colony has a name");
  CHECK(col->population == 1 && col->colonist_count == 1, "founder becomes colonist");
  CHECK(col->colonists[0].unit_type_index == pioneer_type, "colonist type preserved");
  CHECK(col->colonists[0].building_type == town_hall, "founder works in Town Hall");
  CHECK(col->stock[COLONIZE_CARGO_TOOLS] == 100, "founder tools enter stockpile");
  CHECK(col->building_in_production == stockade, "found defaults Stockade project");
  printf("  colony name: %s at (%d,%d) pop=%d tools=%d\n",
         col->name, col->x, col->y, col->population, col->stock[COLONIZE_CARGO_TOOLS]);

  CHECK(
    colonies_assign_workplace(&pool, cid, 0, carpenter),
    "assign founder to Carpenter's Shop"
  );
  CHECK(
    colonies_get(&pool, cid)->colonists[0].building_type == carpenter,
    "workplace is Carpenter's Shop"
  );
  CHECK(!colonies_assign_workplace(&pool, cid, 0, stockade), "cannot assign to unbuilt Stockade");

  CHECK(
    colonies_assign_field(&pool, cid, 0, 0, COLONIZE_JOB_FARMER),
    "assign founder to North field as Farmer"
  );
  CHECK(colonies_get(&pool, cid)->tiles[0] == 0, "tiles[0] is founder");
  CHECK(colonies_get(&pool, cid)->colonists[0].field_job == COLONIZE_JOB_FARMER, "field_job Farmer");
  CHECK(colonies_get(&pool, cid)->colonists[0].building_type < 0, "field clears workplace");
  CHECK(colonies_colonist_tile(colonies_get(&pool, cid), 0) == 0, "colonist_tile is 0");
  CHECK(
    colonies_assign_workplace(&pool, cid, 0, carpenter),
    "reassign to Carpenter clears field"
  );
  CHECK(colonies_get(&pool, cid)->tiles[0] < 0, "field tile cleared by workplace");
  CHECK(colonies_get(&pool, cid)->colonists[0].field_job < 0, "field_job cleared");

  int buildable[32];
  const int n_buildable = colonies_list_buildable(&pool, cid, buildable, 32);
  CHECK(n_buildable > 0, "list_buildable returns projects");
  bool listed_stockade = false;
  for (int i = 0; i < n_buildable; ++i) {
    if (buildable[i] == stockade) {
      listed_stockade = true;
    }
  }
  CHECK(listed_stockade, "Stockade is buildable");

  CHECK(colonies_clear_construction(&pool, cid), "clear construction");
  CHECK(colonies_get(&pool, cid)->building_in_production < 0, "construction cleared");
  CHECK(colonies_set_construction(&pool, cid, stockade), "set construction to Stockade");
  CHECK(
    colonies_get(&pool, cid)->building_in_production == stockade,
    "building_in_production is Stockade"
  );
  CHECK(!colonies_set_construction(&pool, cid, town_hall), "cannot set already-built Town Hall");

  CHECK(!colonies_can_found(&pool, &map, land2_x, land2_y), "cannot found on occupied tile");
  CHECK(colonies_id_at(&pool, land2_x, land2_y) == cid, "colonies_id_at returns correct id");
  CHECK(colonies_id_at(&pool, land2_x + 1, land2_y) < 0, "colonies_id_at returns -1 for empty tile");

  assets_msg_free(&names);
  map_free(&map);

  if (failures == 0) {
    printf("smoke_colonies: all checks passed\n");
    return 0;
  }
  fprintf(stderr, "smoke_colonies: %d failure(s)\n", failures);
  return 1;
}
