#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/ai_popup.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_craft.h"
#include "core/colony_production.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/ss.h"
#include "core/units.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

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


/* Col1 +0x98: BUY construction accumulates hammers_purchased remainder. */
static int smoke_hammers_purchased_buy(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Stockade");
  pool.building_types[0].hammers = 64;
  pool.building_types[0].tools_cost = 0;
  pool.building_type_count = 1;
  ColonizeColony* c = &pool.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = 0;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->building_in_production = 0;
  c->hammers = 14;
  c->hammers_purchased = 0;
  pool.colony_count = 1;
  int gold = 200;
  const int expect = 64 - 14;
  if (!colonies_buy_construction(&pool, 0, &gold)) {
    fprintf(stderr, "hammers_purchased: buy failed\n");
    return 1;
  }
  if (c->hammers_purchased != (uint16_t)expect || gold != 200 - expect) {
    fprintf(stderr, "hammers_purchased=%u gold=%d expect=%d\n",
            (unsigned)c->hammers_purchased, gold, expect);
    return 1;
  }
  if ((c->colony_flags & COLONIZE_COLONY_FLAG_BUILD_COMPLETE) == 0) {
    fprintf(stderr, "expected build_complete flag after BUY\n");
    return 1;
  }
  fprintf(stderr, "smoke_colonies: hammers_purchased buy ok\n");
  return 0;
}

/* Col1 +0x95/+0x96: warehouse_level drives 100*(1+level); capitol INC on complete. */
static int smoke_warehouse_capitol_levels(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Warehouse");
  pool.building_types[0].hammers = 10;
  pool.building_types[0].tools_cost = 0;
  snprintf(pool.building_types[1].name, sizeof(pool.building_types[1].name), "Capitol");
  pool.building_types[1].hammers = 10;
  pool.building_types[1].tools_cost = 0;
  pool.building_type_count = 2;
  ColonizeColony* c = &pool.colonies[0];
  memset(c, 0, sizeof(*c));
  c->id = 0;
  c->active = true;
  c->building_in_production = -1;
  pool.colony_count = 1;

  if (colonies_warehouse_capacity(&pool, c, COLONIZE_CARGO_TOOLS) != 100) {
    fprintf(stderr, "warehouse_level0 cap want 100\n");
    return 1;
  }
  c->warehouse_level = 1;
  if (colonies_warehouse_capacity(&pool, c, COLONIZE_CARGO_TOOLS) != 200) {
    fprintf(stderr, "warehouse_level1 cap want 200 got %d\n",
            colonies_warehouse_capacity(&pool, c, COLONIZE_CARGO_TOOLS));
    return 1;
  }
  c->warehouse_level = 0;
  c->building_in_production = 0;
  c->hammers = 10;
  if (!colonies_try_complete_building(&pool, 0) || c->warehouse_level != 1) {
    fprintf(stderr, "Warehouse complete warehouse_level=%u\n",
            (unsigned)c->warehouse_level);
    return 1;
  }
  if (colonies_warehouse_capacity(&pool, c, COLONIZE_CARGO_TOOLS) != 200) {
    fprintf(stderr, "after Warehouse cap want 200\n");
    return 1;
  }
  c->building_in_production = 1;
  c->hammers = 10;
  if (!colonies_try_complete_building(&pool, 0) || c->capitol_level != 1) {
    fprintf(stderr, "Capitol complete capitol_level=%u\n", (unsigned)c->capitol_level);
    return 1;
  }
  fprintf(stderr, "smoke_colonies: warehouse/capitol levels ok\n");
  return 0;
}

/* Helper: @SEACOLONY / @NOPORT GAME.TXT + inland vs coastal found tiles. */
static int smoke_found_chrome(void) {
  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "found: GAME.TXT load failed\n");
    return 1;
  }
  char body[512];
  popup_msg_fill(
    &game_txt,
    "SEACOLONY",
    NULL,
    "Colonies cannot be built at sea.",
    body,
    sizeof(body)
  );
  if (strstr(body, "sea") == NULL && strstr(body, "Sea") == NULL) {
    fprintf(stderr, "found: SEACOLONY body weak '%s'\n", body);
    assets_msg_free(&game_txt);
    return 1;
  }

  popup_msg_fill(
    &game_txt,
    "NOPORT",
    NULL,
    "This square does not have access to the ocean.",
    body,
    sizeof(body)
  );
  if ((strstr(body, "ocean") == NULL && strstr(body, "Ocean") == NULL) ||
      (strstr(body, "port") == NULL && strstr(body, "Port") == NULL &&
       strstr(body, "wagon") == NULL && strstr(body, "Wagon") == NULL)) {
    fprintf(stderr, "found: NOPORT body weak '%s'\n", body);
    assets_msg_free(&game_txt);
    return 1;
  }
  char choices[4][POPUP_MSG_CHOICE_LEN];
  const ColonizeMsgSection* sec = assets_msg_find(&game_txt, "NOPORT");
  const int nch = popup_msg_choices(sec, choices, 4);
  if (nch < 2) {
    fprintf(stderr, "found: NOPORT want 2 choices got %d\n", nch);
    assets_msg_free(&game_txt);
    return 1;
  }
  if (strstr(choices[0], "forgot") == NULL) {
    fprintf(stderr, "found: NOPORT choice0 unexpected '%s'\n", choices[0]);
    assets_msg_free(&game_txt);
    return 1;
  }
  if (strstr(choices[1], "mind") == NULL && strstr(choices[1], "exactly") == NULL) {
    fprintf(stderr, "found: NOPORT choice1 unexpected '%s'\n", choices[1]);
    assets_msg_free(&game_txt);
    return 1;
  }
  assets_msg_free(&game_txt);

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char map_err[256];
  if (!map_load_mp("COLONIZE/AMER2.MP", &map, map_err, sizeof(map_err))) {
    fprintf(stderr, "found: AMER2 load failed: %s\n", map_err);
    return 1;
  }
  ColonizeColonyPool pool;
  colonies_init(&pool);

  int water = 0, inland = 0, coastal = 0;
  for (int y = 0; y < (int)map.height; ++y) {
    for (int x = 0; x < (int)map.width; ++x) {
      if (!map_tile_is_land(&map, x, y)) {
        water++;
        continue;
      }
      if (!colonies_can_found(&pool, &map, x, y)) {
        continue;
      }
      if (map_tile_is_coastal(&map, x, y)) {
        coastal++;
      } else {
        inland++;
      }
    }
  }
  map_free(&map);
  if (water < 1 || inland < 1 || coastal < 1) {
    fprintf(
      stderr,
      "found: tile classes water=%d inland=%d coastal=%d\n",
      water,
      inland,
      coastal
    );
    return 1;
  }
  fprintf(
    stderr,
    "smoke_colonies: found chrome ok (water=%d inland=%d coastal=%d)\n",
    water,
    inland,
    coastal
  );
  return 0;
}

/* Helper: @FULL chrome when colony is at population cap. */
static int smoke_full_chrome(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Jamestown");
  col->colonist_count = COLONIZE_COLONY_POP_MAX;
  col->population = COLONIZE_COLONY_POP_MAX;
  pool.colony_count = 1;

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "full: GAME.TXT load failed\n");
    return 1;
  }
  AiPopupState pops;
  ai_popup_init(&pops);
  colonies_emit_full_chrome(col, &pops, &game_txt);
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "Jamestown") == NULL &&
       strstr(pops.queue[0].body, "crowded") == NULL &&
       strstr(pops.queue[0].body, "immigrants") == NULL)) {
    fprintf(
      stderr,
      "full: FULL popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  assets_msg_free(&game_txt);
  fprintf(stderr, "smoke_colonies: FULL chrome ok\n");
  return 0;
}

/* Helper: @ALREADYHAVE / @NOMOREWAREHOUSE when construction already owned. */
static int smoke_alreadyhave_chrome(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Williamsburg");
  pool.colony_count = 1;

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "alreadyhave: GAME.TXT load failed\n");
    return 1;
  }

  AiPopupState pops;
  ai_popup_init(&pops);
  colonies_emit_already_have_chrome(col, "Printing Press", &pops, &game_txt);
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "Williamsburg") == NULL &&
       strstr(pops.queue[0].body, "already") == NULL &&
       strstr(pops.queue[0].body, "Printing") == NULL)) {
    fprintf(
      stderr,
      "alreadyhave: ALREADYHAVE popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }

  ai_popup_init(&pops);
  colonies_emit_already_have_chrome(col, "Warehouse Expansion", &pops, &game_txt);
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "Williamsburg") == NULL &&
       strstr(pops.queue[0].body, "expansion") == NULL &&
       strstr(pops.queue[0].body, "Expansion") == NULL &&
       strstr(pops.queue[0].body, "one") == NULL &&
       strstr(pops.queue[0].body, "One") == NULL)) {
    fprintf(
      stderr,
      "alreadyhave: NOMOREWAREHOUSE popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }

  assets_msg_free(&game_txt);
  fprintf(stderr, "smoke_colonies: ALREADYHAVE/NOMOREWAREHOUSE chrome ok\n");
  return 0;
}

/* Helper: unskilled school assign → @NOTEACHER; Teacher may assign. */
static int smoke_noteacher_chrome(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Schoolhouse");
  pool.building_type_count = 1;

  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Harvard");
  col->has_building[0] = true;
  col->colonists[0].active = true;
  col->colonists[0].profession = COLONIZE_PROF_FREE_COLONIST;
  col->colonists[0].building_type = -1;
  col->colonists[0].field_job = -1;
  col->colonist_count = 1;
  col->population = 1;
  pool.colony_count = 1;

  if (colonies_assign_workplace(&pool, 1, 0, 0)) {
    fprintf(stderr, "noteacher: Free Colonist should not assign to Schoolhouse\n");
    return 1;
  }

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "noteacher: GAME.TXT load failed\n");
    return 1;
  }
  AiPopupState pops;
  ai_popup_init(&pops);
  colonies_emit_noteacher_chrome(&pops, &game_txt);
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "mastered") == NULL &&
       strstr(pops.queue[0].body, "teach") == NULL &&
       strstr(pops.queue[0].body, "profession") == NULL)) {
    fprintf(
      stderr,
      "noteacher: NOTEACHER popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  assets_msg_free(&game_txt);

  col->colonists[0].profession = COLONIZE_PROF_TEACHER;
  if (!colonies_assign_workplace(&pool, 1, 0, 0)) {
    fprintf(stderr, "noteacher: Teacher should assign to Schoolhouse\n");
    return 1;
  }
  if (col->colonists[0].building_type != 0) {
    fprintf(stderr, "noteacher: Teacher building_type want 0 got %d\n", col->colonists[0].building_type);
    return 1;
  }

  fprintf(stderr, "smoke_colonies: NOTEACHER chrome ok\n");
  return 0;
}

/* Helper: school tier gates @NEEDCOLLEGE / @NEEDUNIVERSITY. */
static int smoke_needschool_chrome(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Schoolhouse");
  snprintf(pool.building_types[1].name, sizeof(pool.building_types[1].name), "College");
  snprintf(pool.building_types[2].name, sizeof(pool.building_types[2].name), "University");
  pool.building_type_count = 3;

  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Yale");
  col->has_building[0] = true; /* Schoolhouse only */
  col->colonists[0].active = true;
  col->colonists[0].profession = COLONIZE_PROF_BLACKSMITH;
  col->colonists[0].building_type = -1;
  col->colonists[0].field_job = -1;
  col->colonist_count = 1;
  col->population = 1;
  pool.colony_count = 1;

  if (colonies_assign_workplace(&pool, 1, 0, 0)) {
    fprintf(stderr, "needschool: Blacksmith should not assign to Schoolhouse\n");
    return 1;
  }

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "needschool: GAME.TXT load failed\n");
    return 1;
  }
  AiPopupState pops;
  ai_popup_init(&pops);
  colonies_emit_need_school_chrome(COLONIZE_PROF_BLACKSMITH, 1, &pops, &game_txt);
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "college") == NULL &&
       strstr(pops.queue[0].body, "College") == NULL &&
       strstr(pops.queue[0].body, "Blacksmith") == NULL)) {
    fprintf(
      stderr,
      "needschool: NEEDCOLLEGE popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }

  /* College only — Preacher needs University. */
  col->has_building[0] = false;
  col->has_building[1] = true;
  col->colonists[0].profession = COLONIZE_PROF_PREACHER;
  col->colonists[0].building_type = -1;
  if (colonies_assign_workplace(&pool, 1, 0, 1)) {
    fprintf(stderr, "needschool: Preacher should not assign to College-only\n");
    assets_msg_free(&game_txt);
    return 1;
  }
  ai_popup_init(&pops);
  colonies_emit_need_school_chrome(COLONIZE_PROF_PREACHER, 2, &pops, &game_txt);
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "university") == NULL &&
       strstr(pops.queue[0].body, "University") == NULL &&
       strstr(pops.queue[0].body, "Preacher") == NULL)) {
    fprintf(
      stderr,
      "needschool: NEEDUNIVERSITY popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  assets_msg_free(&game_txt);

  /* University + Preacher OK. */
  col->has_building[1] = false;
  col->has_building[2] = true;
  col->colonists[0].building_type = -1;
  if (!colonies_assign_workplace(&pool, 1, 0, 2)) {
    fprintf(stderr, "needschool: Preacher should assign to University\n");
    return 1;
  }

  /* Schoolhouse + Farmer OK. */
  col->has_building[2] = false;
  col->has_building[0] = true;
  col->colonists[0].profession = COLONIZE_JOB_FARMER;
  col->colonists[0].building_type = -1;
  if (!colonies_assign_workplace(&pool, 1, 0, 0)) {
    fprintf(stderr, "needschool: Farmer should assign to Schoolhouse\n");
    return 1;
  }

  fprintf(stderr, "smoke_colonies: NEEDCOLLEGE/NEEDUNIVERSITY chrome ok\n");
  return 0;
}

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
  CHECK(pool.name_count[0] > 0, "at least one English colony name loaded");

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
  const int empty_id = colonies_found(&pool, &map, land_x, land_y, 0, -1, UNITS_JOB_NONE, 0, 0, 0);
  CHECK(empty_id >= 0, "colonies_found without founder");
  const ColonizeColony* empty = colonies_get(&pool, empty_id);
  CHECK(empty && empty->population == 0 && empty->colonist_count == 0, "no founder => pop 0");
  CHECK(empty && empty->has_building[town_hall], "starter includes Town Hall");
  CHECK(empty && empty->has_building[carpenter], "starter includes Carpenter's Shop");
  CHECK(empty && !empty->has_building[stockade], "starter excludes Stockade");
  CHECK(empty && !empty->has_building[warehouse], "starter excludes Warehouse");
  CHECK(empty && !empty->has_building[docks], "starter excludes Docks");
  CHECK(empty && empty->stock[COLONIZE_CARGO_FOOD] == 0, "starter food stock is 0");

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
    const int coast_id = colonies_found(&pool, &map, coast_x, coast_y, 0, -1, UNITS_JOB_NONE, 0, 0, 0);
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
  int cid =
    colonies_found(&pool, &map, land2_x, land2_y, 0, pioneer_type, UNITS_JOB_PIONEER, 100, 0, 0);
  CHECK(cid >= 0, "colonies_found with founder");
  const ColonizeColony* col = colonies_get(&pool, cid);
  CHECK(col != NULL, "colonies_get returns colony");
  CHECK(col->active, "colony is active");
  CHECK(col->x == land2_x && col->y == land2_y, "colony at expected coordinates");
  CHECK(col->name[0] != '\0', "colony has a name");
  CHECK(col->population == 1 && col->colonist_count == 1, "founder becomes colonist");
  CHECK(col->colonists[0].unit_type_index == pioneer_type, "colonist type preserved");
  CHECK(col->colonists[0].profession == UNITS_JOB_PIONEER, "founder profession preserved");
  CHECK(col->colonists[0].building_type == town_hall, "founder works in Town Hall");
  CHECK(col->stock[COLONIZE_CARGO_TOOLS] == 100, "founder tools enter stockpile");
  CHECK(col->building_in_production == stockade, "found defaults Stockade project");
  CHECK(col->hammers == 0, "new colony starts with zero accumulated hammers");
  CHECK(
    units_working_colonist_sprite(NULL, pioneer_type, UNITS_JOB_PIONEER) ==
      UNITS_ICON_HARDY_PIONEER_WORK,
    "hardy pioneer work sprite"
  );
  CHECK(
    units_working_colonist_sprite(NULL, pioneer_type, UNITS_JOB_SOLDIER) ==
      UNITS_ICON_VETERAN_SOLDIER_WORK,
    "veteran soldier work sprite"
  );
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

  /* Admit outside unit → colonist; eject colonist → outside unit. */
  {
    ColonizeUnitPool units;
    memset(&units, 0, sizeof(units));
    CHECK(units_load_types(&units, &names), "load unit types for admit/eject");
    const int free_col = units_find_type(&units, "Colonists");
    CHECK(free_col >= 0, "Colonists type");
    const ColonizeColony* before = colonies_get(&pool, cid);
    const int pop0 = before ? before->colonist_count : 0;
    const int uid = units_spawn_allow_stack(&units, free_col, land2_x, land2_y);
    CHECK(uid >= 0, "spawn outside Colonist");
    ColonizeUnit* ou = units_get(&units, uid);
    if (ou && before) {
      ou->nation_id = before->nation_id;
    }
    const int admitted = colonies_admit_unit(&pool, cid, &units, uid);
    CHECK(admitted == pop0, "admit returns new colonist index");
    CHECK(colonies_get(&pool, cid)->colonist_count == pop0 + 1, "pop +1 after admit");
    {
      const ColonizeUnit* gone = units_get_const(&units, uid);
      CHECK(!gone || !gone->active, "map unit despawned on admit");
    }
    const int ejected = colonies_eject_colonist(&pool, cid, admitted, &units, COLONIZE_EJECT_COLONIST);
    CHECK(ejected >= 0, "eject returns unit id");
    CHECK(colonies_get(&pool, cid)->colonist_count == pop0, "pop restored after eject");
    const ColonizeUnit* back = units_get_const(&units, ejected);
    CHECK(back && back->active && back->x == land2_x && back->y == land2_y, "ejected on colony tile");
    CHECK(
      colonies_has_fortification(&pool, colonies_get(&pool, cid)) == false,
      "no fortification yet"
    );

    /* Pioneer eject spends tools from warehouse. */
    {
      ColonizeColony* col = colonies_get_mut(&pool, cid);
      CHECK(col != NULL, "colony mut for pioneer eject");
      if (col) {
        col->stock[COLONIZE_CARGO_TOOLS] = 100;
      }
      const int uid2 = units_spawn_allow_stack(&units, free_col, land2_x, land2_y);
      CHECK(uid2 >= 0, "spawn for pioneer admit");
      ColonizeUnit* ou2 = units_get(&units, uid2);
      if (ou2 && col) {
        ou2->nation_id = col->nation_id;
      }
      const int ad2 = colonies_admit_unit(&pool, cid, &units, uid2);
      CHECK(ad2 >= 0, "admit before pioneer eject");
      CHECK(colonies_get(&pool, cid)->stock[COLONIZE_CARGO_TOOLS] >= 100, "tools in stock after admit");
      const int pej =
        colonies_eject_colonist(&pool, cid, ad2, &units, COLONIZE_EJECT_PIONEER);
      CHECK(pej >= 0, "eject as pioneer");
      const ColonizeUnit* pion = units_get_const(&units, pej);
      CHECK(pion && pion->tools >= 20, "pioneer carries tools");
      CHECK(units_map_sprite(&units, pej) == UNITS_ICON_PIONEER ||
              units_map_sprite(&units, pej) == UNITS_ICON_HARDY_PIONEER,
            "pioneer map icon");
    }
    /* Skill sticks: hardy pioneer armed as soldier looks non-veteran; re-admit keeps skill. */
    {
      ColonizeColony* col = colonies_get_mut(&pool, cid);
      CHECK(col != NULL, "colony mut for skill stick");
      if (col) {
        col->stock[COLONIZE_CARGO_TOOLS] = 100;
        col->stock[COLONIZE_CARGO_MUSKETS] = 50;
      }
      const int uid3 = units_spawn_allow_stack(&units, free_col, land2_x, land2_y);
      CHECK(uid3 >= 0, "spawn for hardy skill test");
      ColonizeUnit* ou3 = units_get(&units, uid3);
      if (ou3 && col) {
        ou3->nation_id = col->nation_id;
        ou3->profession = UNITS_JOB_PIONEER;
      }
      const int ad3 = colonies_admit_unit(&pool, cid, &units, uid3);
      CHECK(ad3 >= 0, "admit hardy pioneer");
      CHECK(
        colonies_get(&pool, cid)->colonists[ad3].profession == UNITS_JOB_PIONEER,
        "admit keeps hardy profession"
      );
      CHECK(
        units_working_colonist_sprite(
          &units,
          colonies_get(&pool, cid)->colonists[ad3].unit_type_index,
          UNITS_JOB_PIONEER
        ) == UNITS_ICON_HARDY_PIONEER_WORK,
        "working hardy pioneer sprite #58"
      );
      const int sej =
        colonies_eject_colonist(&pool, cid, ad3, &units, COLONIZE_EJECT_SOLDIER);
      CHECK(sej >= 0, "eject hardy as soldier");
      const ColonizeUnit* sold = units_get_const(&units, sej);
      CHECK(sold && sold->profession == UNITS_JOB_PIONEER, "soldier keeps hardy skill");
      CHECK(sold && sold->muskets > 0, "soldier carries muskets");
      CHECK(
        units_map_sprite(&units, sej) == UNITS_ICON_SOLDIER,
        "hardy+muskets uses non-veteran soldier icon"
      );
      const int ad4 = colonies_admit_unit(&pool, cid, &units, sej);
      CHECK(ad4 >= 0, "re-admit armed hardy");
      CHECK(
        colonies_get(&pool, cid)->colonists[ad4].profession == UNITS_JOB_PIONEER,
        "re-admit still hardy pioneer"
      );
      const int pej2 =
        colonies_eject_colonist(&pool, cid, ad4, &units, COLONIZE_EJECT_PIONEER);
      CHECK(pej2 >= 0, "re-eject as pioneer");
      const ColonizeUnit* pion2 = units_get_const(&units, pej2);
      CHECK(pion2 && pion2->profession == UNITS_JOB_PIONEER, "tools eject keeps skill");
      CHECK(
        units_map_sprite(&units, pej2) == UNITS_ICON_HARDY_PIONEER,
        "hardy+tools map icon #101"
      );
    }
    /* Church bless: Leave as Missionary when Church present; absent without. */
    {
      ColonizeColony* col = colonies_get_mut(&pool, cid);
      CHECK(col != NULL, "colony mut for missionary eject");
      const int church = colonies_find_building(&pool, "Church");
      CHECK(church >= 0, "Church building type");
      int roles[COLONIZE_EJECT_ROLE_COUNT];
      const int n0 = colonies_list_eject_roles(&pool, cid, 0, roles, COLONIZE_EJECT_ROLE_COUNT);
      int has_miss = 0;
      for (int i = 0; i < n0; ++i) {
        if (roles[i] == COLONIZE_EJECT_MISSIONARY) {
          has_miss = 1;
        }
      }
      CHECK(!has_miss, "no Missionary leave-as without Church");
      if (col && church >= 0) {
        col->has_building[church] = true;
      }
      /* Need a colonist on site. */
      const int uidm = units_spawn_allow_stack(&units, free_col, land2_x, land2_y);
      CHECK(uidm >= 0, "spawn for missionary admit");
      ColonizeUnit* oum = units_get(&units, uidm);
      if (oum && col) {
        oum->nation_id = col->nation_id;
      }
      const int adm = colonies_admit_unit(&pool, cid, &units, uidm);
      CHECK(adm >= 0, "admit before missionary eject");
      const int n1 = colonies_list_eject_roles(&pool, cid, adm, roles, COLONIZE_EJECT_ROLE_COUNT);
      has_miss = 0;
      for (int i = 0; i < n1; ++i) {
        if (roles[i] == COLONIZE_EJECT_MISSIONARY) {
          has_miss = 1;
        }
      }
      CHECK(has_miss, "Missionary leave-as with Church");
      const int mej =
        colonies_eject_colonist(&pool, cid, adm, &units, COLONIZE_EJECT_MISSIONARY);
      CHECK(mej >= 0, "eject as Missionary");
      const ColonizeUnit* miss = units_get_const(&units, mej);
      CHECK(miss && miss->active, "missionary unit active");
      const ColonizeUnitType* mt = units_type(&units, miss->type_index);
      CHECK(mt && strstr(mt->name, "Missionar") != NULL, "Missionaries unit type");
      CHECK(miss->profession == UNITS_JOB_NONE, "plain Church bless is non-Jesuit");
    }
  }

  int buildable[32];
  ColoniesBuildableOpts bopts;
  memset(&bopts, 0, sizeof(bopts));
  bopts.map = &map;
  const int n_buildable = colonies_list_buildable(&pool, cid, buildable, 32, &bopts);
  CHECK(n_buildable > 0, "list_buildable returns projects");
  bool listed_stockade = false;
  bool listed_warehouse = false;
  bool listed_weavers_shop = false;
  bool listed_iron_works = false;
  for (int i = 0; i < n_buildable; ++i) {
    if (buildable[i] == stockade) {
      listed_stockade = true;
    }
    if (strcmp(pool.building_types[buildable[i]].name, "Warehouse") == 0) {
      listed_warehouse = true;
    }
    if (strcmp(pool.building_types[buildable[i]].name, "Weaver's Shop") == 0) {
      listed_weavers_shop = true;
    }
    if (strcmp(pool.building_types[buildable[i]].name, "Iron Works") == 0) {
      listed_iron_works = true;
    }
  }
  /* New colony pop=1: Stockade needs min_colony 3. */
  CHECK(!listed_stockade, "Stockade hidden until population 3");
  CHECK(listed_warehouse, "Warehouse is buildable at pop 1");
  CHECK(listed_weavers_shop, "Weaver's Shop upgrade is buildable");
  CHECK(!listed_iron_works, "Iron Works requires Adam Smith");

  /* Grow to Stockade min pop and confirm it appears. */
  {
    ColonizeColony* grow = colonies_get_mut(&pool, cid);
    CHECK(grow != NULL, "mut colony for pop bump");
    grow->population = 3;
    grow->colonist_count = 3;
    for (int i = 1; i < 3; ++i) {
      grow->colonists[i] = grow->colonists[0];
      grow->colonists[i].active = true;
    }
  }
  const int n2 = colonies_list_buildable(&pool, cid, buildable, 32, &bopts);
  listed_stockade = false;
  for (int i = 0; i < n2; ++i) {
    if (buildable[i] == stockade) {
      listed_stockade = true;
    }
  }
  CHECK(listed_stockade, "Stockade is buildable at pop 3");

  bopts.has_adam_smith = true;
  {
    ColonizeColony* grow = colonies_get_mut(&pool, cid);
    grow->population = 8;
    grow->colonist_count = 8;
    for (int i = 3; i < 8; ++i) {
      grow->colonists[i] = grow->colonists[0];
      grow->colonists[i].active = true;
    }
    const int shop = colonies_find_building(&pool, "Blacksmith's Shop");
    CHECK(shop >= 0, "Blacksmith's Shop type exists");
    grow->has_building[shop] = true;
  }
  const int n3 = colonies_list_buildable(&pool, cid, buildable, 32, &bopts);
  listed_iron_works = false;
  for (int i = 0; i < n3; ++i) {
    if (strcmp(pool.building_types[buildable[i]].name, "Iron Works") == 0) {
      listed_iron_works = true;
    }
  }
  CHECK(listed_iron_works, "Iron Works buildable with Adam Smith + shop");

  /* Custom House requires Peter Stuyvesant (not available by default). */
  {
    bool listed_ch = false;
    for (int i = 0; i < n3; ++i) {
      if (strcmp(pool.building_types[buildable[i]].name, "Custom House") == 0) {
        listed_ch = true;
      }
    }
    CHECK(!listed_ch, "Custom House hidden without Stuyvesant");
    bopts.has_peter_stuyvesant = true;
    const int n4 = colonies_list_buildable(&pool, cid, buildable, 32, &bopts);
    listed_ch = false;
    for (int i = 0; i < n4; ++i) {
      if (strcmp(pool.building_types[buildable[i]].name, "Custom House") == 0) {
        listed_ch = true;
      }
    }
    CHECK(listed_ch, "Custom House buildable with Stuyvesant");
  }

  /* tools(*10): Warehouse Expansion NAMES value 2 → 20 tools. */
  {
    const int whe = colonies_find_building(&pool, "Warehouse Expansion");
    CHECK(whe >= 0, "Warehouse Expansion type exists");
    CHECK(pool.building_types[whe].tools_cost == 20, "Warehouse Expansion tools are 20");
  }

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

  /* SoL craft deepen: +sol_bonus per manufacturing worker on output. */
  {
    ColonizeColony* c = colonies_get_mut(&pool, cid);
    CHECK(c != NULL, "colony for SoL craft");
    const int shop = colonies_find_building(&pool, "Blacksmith's Shop");
    CHECK(shop >= 0, "Blacksmith's Shop for SoL craft");
    if (c && shop >= 0) {
      c->has_building[shop] = true;
      if (c->colonist_count < 1) {
        c->colonist_count = 1;
        c->colonists[0].active = true;
      }
      c->colonists[0].active = true;
      c->colonists[0].building_type = shop;
      c->colonists[0].profession = 19; /* free colonist */
      c->stock[COLONIZE_CARGO_ORE] = 20;
      c->stock[COLONIZE_CARGO_TOOLS] = 0;
      colony_craft_one_colony(&pool, c, NULL, 0);
      const int tools0 = c->stock[COLONIZE_CARGO_TOOLS];
      CHECK(tools0 > 0, "craft without SoL produces tools");
      c->stock[COLONIZE_CARGO_ORE] = 20;
      c->stock[COLONIZE_CARGO_TOOLS] = 0;
      colony_craft_one_colony(&pool, c, NULL, 2);
      const int tools2 = c->stock[COLONIZE_CARGO_TOOLS];
      CHECK(tools2 > tools0, "SoL +2 craft yields more tools than baseline");
      c->has_building[shop] = false;
      c->colonists[0].building_type = -1;
    }
  }

  /* Abandon removes colony (cargo discarded). */
  {
    ColonizeColony* c = colonies_get_mut(&pool, cid);
    CHECK(c != NULL, "colony before abandon");
    c->stock[COLONIZE_CARGO_FOOD] = 50;
    /* Warehouse spoilage clamp (FUN_15eb_0a50): tools above base 100 → 100. */
    c->stock[COLONIZE_CARGO_TOOLS] = 150;
    CHECK(colonies_warehouse_capacity(&pool, c, COLONIZE_CARGO_TOOLS) == 100, "base tools cap 100");
    CHECK(colonies_apply_warehouse_spoilage(&pool, c, NULL, NULL) == 50, "spoil 50 tools over cap");
    CHECK(c->stock[COLONIZE_CARGO_TOOLS] == 100, "tools clamped to 100");
    /* Multi-type spoil → type count. */
    c->stock[COLONIZE_CARGO_TOOLS] = 150;
    c->stock[COLONIZE_CARGO_LUMBER] = 150;
    int first = -1, types = 0;
    CHECK(
      colonies_apply_warehouse_spoilage(&pool, c, &first, &types) == 100,
      "spoil 100 across two cargos"
    );
    CHECK(types == 2, "spoil type count 2");
    CHECK(first == COLONIZE_CARGO_LUMBER || first == COLONIZE_CARGO_TOOLS, "first spoil set");
    CHECK(c->stock[COLONIZE_CARGO_TOOLS] == 100, "tools reclamped");
    CHECK(c->stock[COLONIZE_CARGO_LUMBER] == 100, "lumber clamped");
    const int stockade_b = colonies_find_building(&pool, "Stockade");
    CHECK(stockade_b >= 0, "stockade type for fortification check");
    c->has_building[stockade_b] = true;
    CHECK(colonies_has_fortification(&pool, c), "fortification detected");
    CHECK(colonies_abandon(&pool, cid), "abandon colony");
    CHECK(colonies_get(&pool, cid) == NULL, "colony gone after abandon");
    CHECK(colonies_id_at(&pool, land2_x, land2_y) < 0, "tile free after abandon");
  }

  /* Re-found so map-icon test has a colony. */
  cid = colonies_found(&pool, &map, land2_x, land2_y, 0, pioneer_type, UNITS_JOB_PIONEER, 0, 0, 0);
  CHECK(cid >= 0, "re-found for map icon");

  /* Map marker: ICONS.SS #0–3 colony settlement (not cargo greys #38+). */
  {
    ColonizeSpriteSheet icons;
    memset(&icons, 0, sizeof(icons));
    char icons_err[256];
    const bool icons_ok = ss_load("COLONIZE/ICONS.SS", &icons, icons_err, sizeof(icons_err));
    CHECK(icons_ok && icons.sprite_count > 3, "load ICONS.SS for colony map icon");
    if (icons_ok) {
      CHECK(icons.sprites[3].width == 21 && icons.sprites[3].height == 16,
            "unfortified colony icon is 21x16");
      uint8_t pixels[16 * 16];
      memset(pixels, 0, sizeof(pixels));
      ColonizeFramebuffer8 fb = {.width = 16, .height = 16, .pixels = pixels};
      colonies_render_on_map(&pool, &icons, &fb, NULL, land2_x, land2_y, 1, 1, 16, 16, 0, 0, NULL, 0);

      int cyan = 0;
      int opaque = 0;
      for (int i = 0; i < 16 * 16; ++i) {
        if (pixels[i] == 11) {
          ++cyan;
        }
        if (pixels[i] != 0 && pixels[i] != COLONIZE_SS_TRANSPARENT) {
          ++opaque;
        }
      }
      CHECK(opaque > 0, "colony map icon painted opaque pixels");
      CHECK(cyan < 16 * 16, "colony map icon is not a solid cyan tile");
      ss_free(&icons);
    }
  }

  assets_msg_free(&names);
  map_free(&map);

  /* colonies_destroy_building: clear building + workplace; refuse Town Hall. */
  {
    const int stockade = colonies_find_building(&pool, "Stockade");
    const int town = colonies_find_building(&pool, "Town Hall");
    CHECK(stockade >= 0 && town >= 0, "Stockade and Town Hall types for destroy");
    ColonizeColony* c = colonies_get_mut(&pool, cid);
    CHECK(c != NULL, "destroy colony lookup");
    if (c && stockade >= 0 && town >= 0) {
      c->has_building[stockade] = true;
      if (c->colonist_count > 0) {
        c->colonists[0].building_type = stockade;
      }
      CHECK(colonies_destroy_building(&pool, cid, stockade), "destroy Stockade");
      CHECK(!c->has_building[stockade], "Stockade cleared");
      if (c->colonist_count > 0) {
        CHECK(c->colonists[0].building_type == -1, "workplace cleared on destroy");
      }
      CHECK(!colonies_destroy_building(&pool, cid, town), "refuse destroy Town Hall");
      CHECK(c->has_building[town], "Town Hall remains");
    }
  }

  /*
   * SoL %: Col1 rebel fields when present; else nation liberty_bells/4.
   * Cite: colony_prod_sol_percent; FUN_43f7_0004-shaped; manual_gap SoL display.
   */
  {
    ColonizeColony* c = colonies_get_mut(&pool, cid);
    CHECK(c != NULL, "sol colony");
    if (c) {
      ColonizeCol1Save col1;
      ColonizeCol1Colony col1c;
      memset(&col1, 0, sizeof(col1));
      memset(&col1c, 0, sizeof(col1c));
      col1.colony = &col1c;
      col1.head.colony_count = 1;
      for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
        col1.head.founding_father[i] = -1;
      }
      col1c.x = (uint8_t)c->x;
      col1c.y = (uint8_t)c->y;
      col1c.nation_id = (uint8_t)c->nation_id;
      col1c.rebel_dividend = 50;
      col1c.rebel_divisor = 100;
      CHECK(colony_prod_sol_percent(&col1, c) == 50, "SoL from rebel 50/100");
      CHECK(colony_prod_sol_bonus(&col1, c) == 1, "SoL bonus +1 at 50%");
      c->colony_flags = 0;
      colony_prod_refresh_sol_flags(c, &col1);
      CHECK((c->colony_flags & COLONIZE_COLONY_FLAG_SOL_50) != 0, "sol_50 latch at 50%");
      CHECK((c->colony_flags & COLONIZE_COLONY_FLAG_SOL_100) == 0, "sol_100 clear at 50%");
      col1c.rebel_dividend = 0;
      col1c.rebel_divisor = 0;
      col1.nation[c->nation_id].liberty_bells_total = 200; /* /4 → 50 */
      CHECK(colony_prod_sol_percent(&col1, c) == 50, "SoL bells fallback 200/4");
      CHECK(colony_prod_sol_bonus(&col1, c) == 1, "SoL bonus from bells fallback");
      col1.nation[c->nation_id].liberty_bells_total = 400; /* /4 → 100 */
      CHECK(colony_prod_sol_percent(&col1, c) == 100, "SoL bells cap 100");
      CHECK(colony_prod_sol_bonus(&col1, c) == 2, "SoL bonus +2 at 100%");
      colony_prod_refresh_sol_flags(c, &col1);
      CHECK((c->colony_flags & COLONIZE_COLONY_FLAG_SOL_100) != 0, "sol_100 latch at 100%");
      CHECK((c->colony_flags & COLONIZE_COLONY_FLAG_SOL_50) != 0, "sol_50 stays at 100%");
      /* DOS hysteresis: sol_100 stays while SoL in 95..99. */
      col1.nation[c->nation_id].liberty_bells_total = 388; /* /4 → 97 */
      CHECK(colony_prod_sol_percent(&col1, c) == 97, "SoL 97 for hysteresis");
      colony_prod_refresh_sol_flags(c, &col1);
      CHECK((c->colony_flags & COLONIZE_COLONY_FLAG_SOL_100) != 0, "sol_100 holds at 97%");
      col1.nation[c->nation_id].liberty_bells_total = 360; /* /4 → 90 */
      colony_prod_refresh_sol_flags(c, &col1);
      CHECK((c->colony_flags & COLONIZE_COLONY_FLAG_SOL_100) == 0, "sol_100 clear below 95%");
      CHECK((c->colony_flags & COLONIZE_COLONY_FLAG_SOL_50) != 0, "sol_50 holds at 90%");
      col1.nation[c->nation_id].liberty_bells_total = 0;
      colony_prod_refresh_sol_flags(c, &col1);
      CHECK((c->colony_flags & (COLONIZE_COLONY_FLAG_SOL_50 | COLONIZE_COLONY_FLAG_SOL_100)) == 0,
            "SoL flags clear when SoL drops");

      /*
       * FUN_15eb_0274 Bolivar: +20 display for human; AI control gets none; cap 100.
       * Cite: sons_of_liberty.md; founding_fathers_bolivar_sol_bonus.
       */
      {
        for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
          col1.head.founding_father[i] = -1;
        }
        col1.player[c->nation_id].control = 0;
        col1c.rebel_dividend = 40;
        col1c.rebel_divisor = 100;
        CHECK(colony_prod_sol_percent(&col1, c) == 40, "SoL 40 without Bolivar");
        col1.head.founding_father[FF_SIMON_BOLIVAR] = (int8_t)c->nation_id;
        CHECK(colony_prod_sol_percent(&col1, c) == 60, "SoL Bolivar human 40+20");
        col1c.rebel_dividend = 90;
        CHECK(colony_prod_sol_percent(&col1, c) == 100, "SoL Bolivar caps at 100");
        col1c.rebel_dividend = 40;
        col1.player[c->nation_id].control = 1; /* AI */
        CHECK(colony_prod_sol_percent(&col1, c) == 40, "SoL Bolivar skipped for AI");
        col1.head.founding_father[FF_SIMON_BOLIVAR] = -1;
        col1.player[c->nation_id].control = 0;
        col1c.rebel_dividend = 50;
        col1c.rebel_divisor = 100;
      }

      /*
       * Tory floor: −⌊tories/thresh⌋ + sol latches.
       * Cite: sons_of_liberty.md; difficulty.md; decomp ~11880.
       */
      {
        const int pop_saved = c->population;
        const int count_saved = c->colonist_count;
        const uint8_t flags_saved = c->colony_flags;
        c->population = 12;
        c->colonist_count = 12;
        c->colony_flags = 0;
        col1.head.difficulty = 0; /* Discoverer → human thresh 10 */
        col1.player[c->nation_id].control = 0; /* human */
        col1c.rebel_dividend = 0;
        col1c.rebel_divisor = 100; /* sol% = 0 */
        col1.nation[c->nation_id].liberty_bells_total = 0;
        /* tories=(12*100+50)/100=12; floor(12/10)=1 → mod=-1 */
        CHECK(colony_prod_sol_bonus(&col1, c) == -1, "Tory floor −1 pop12 sol0 Discoverer");

        col1.head.difficulty = 4; /* Viceroy → thresh 6 */
        /* floor(12/6)=2 → mod=-2 */
        CHECK(colony_prod_sol_bonus(&col1, c) == -2, "Tory floor −2 pop12 sol0 Viceroy");

        col1.player[c->nation_id].control = 1; /* AI → thresh 10 always */
        CHECK(colony_prod_sol_bonus(&col1, c) == -1, "Tory floor AI thresh 10 at Viceroy");

        /* sol 50 + latch: tories=(12*50+50)/100=6; floor(6/10)=0; +1 → 1 */
        col1.player[c->nation_id].control = 0;
        col1.head.difficulty = 0;
        col1c.rebel_dividend = 50;
        col1c.rebel_divisor = 100;
        c->colony_flags = COLONIZE_COLONY_FLAG_SOL_50;
        CHECK(colony_prod_sol_bonus(&col1, c) == 1, "Tory floor 0 + sol_50 latch");

        c->population = pop_saved;
        c->colonist_count = count_saved;
        c->colony_flags = flags_saved;
        col1.head.difficulty = 0;
        col1.player[c->nation_id].control = 0;
        col1c.rebel_dividend = 0;
        col1c.rebel_divisor = 0;
      }
    }
  }

  /*
   * TRADE Edit autofill: unload from unit holds; load from colony surplus.
   * Cite: colonies_trade_stop_autofill.
   */
  {
    ColonizeUnitPool units;
    units_reset(&units);
    units.type_count = 1;
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
    units.types[0].movement = 2;
    units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    units.types[0].cargo = 2;

    ColonizeColony* c = colonies_get_mut(&pool, cid);
    CHECK(c != NULL, "autofill colony");
    if (c) {
      c->stock[COLONIZE_CARGO_TOOLS] = 80;
      c->stock[COLONIZE_CARGO_LUMBER] = 80;
      c->stock[COLONIZE_CARGO_ORE] = 5;
      const int wid = units_spawn(&units, 0, c->x, c->y);
      ColonizeUnit* w = units_get(&units, wid);
      CHECK(w != NULL, "autofill wagon");
      if (w) {
        CHECK(units_load_goods(&units, wid, COLONIZE_CARGO_FOOD, 20) == 20, "autofill FOOD hold");
        ColonizeCol1TradeStop st;
        memset(&st, 0, sizeof(st));
        st.colony_index = (uint16_t)cid;
        colonies_trade_stop_autofill(&st, c, &units, wid);
        CHECK(st.unload_count == 1, "autofill unload FOOD");
        CHECK(
          col1_trade_nibble_cargo(st.unload_cargo_nibbles, 0) == COLONIZE_CARGO_FOOD,
          "autofill unload nibble FOOD"
        );
        CHECK(st.load_count >= 2, "autofill load tools+lumber surplus");
        CHECK(
          col1_trade_nibble_cargo(st.load_cargo_nibbles, 0) == COLONIZE_CARGO_TOOLS,
          "autofill load TOOLS first"
        );
        CHECK(
          col1_trade_nibble_cargo(st.load_cargo_nibbles, 1) == COLONIZE_CARGO_LUMBER,
          "autofill load LUMBER second"
        );
        /* Europe: unload only */
        ColonizeCol1TradeStop eu;
        memset(&eu, 0, sizeof(eu));
        eu.colony_index = 999;
        colonies_trade_stop_autofill(&eu, NULL, &units, wid);
        CHECK(eu.unload_count == 1 && eu.load_count == 0, "Europe autofill unload-only");
      }
    }
  }

  /*
   * TRADE cargo picker setter: explicit unload TOOLS + load SILVER (not autofill).
   * Cite: colonies_trade_stop_set_cargos; ColonizeCol1TradeStop.
   */
  {
    ColonizeUnitPool units;
    units_reset(&units);
    units.type_count = 1;
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
    units.types[0].movement = 2;
    units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    units.types[0].cargo = 2;

    ColonizeColony* c = colonies_get_mut(&pool, cid);
    CHECK(c != NULL, "picker colony");
    if (c) {
      c->stock[COLONIZE_CARGO_TOOLS] = 40;
      c->stock[COLONIZE_CARGO_SILVER] = 80;
      c->stock[COLONIZE_CARGO_FOOD] = 5;
      const int tools0 = c->stock[COLONIZE_CARGO_TOOLS];
      const int silver0 = c->stock[COLONIZE_CARGO_SILVER];

      const int wid = units_spawn(&units, 0, c->x, c->y);
      ColonizeUnit* w = units_get(&units, wid);
      CHECK(w != NULL, "picker wagon");
      if (w) {
        w->nation_id = c->nation_id;
        CHECK(units_load_goods(&units, wid, COLONIZE_CARGO_TOOLS, 20) == 20, "picker TOOLS");
        CHECK(units_load_goods(&units, wid, COLONIZE_CARGO_FOOD, 20) == 20, "picker FOOD");

        ColonizeCol1TradeStop st;
        memset(&st, 0, sizeof(st));
        st.colony_index = (uint16_t)cid;
        const int unload_t[] = {COLONIZE_CARGO_TOOLS};
        const int load_t[] = {COLONIZE_CARGO_SILVER};
        colonies_trade_stop_set_cargos(&st, unload_t, 1, load_t, 1);
        CHECK(st.unload_count == 1 && st.load_count == 1, "picker counts");
        CHECK(
          col1_trade_nibble_cargo(st.unload_cargo_nibbles, 0) == COLONIZE_CARGO_TOOLS,
          "picker unload TOOLS"
        );
        CHECK(
          col1_trade_nibble_cargo(st.load_cargo_nibbles, 0) == COLONIZE_CARGO_SILVER,
          "picker load SILVER"
        );
        CHECK(
          colonies_trade_route_service_stop(&pool, cid, &units, wid, &st) == 1,
          "picker service"
        );
        CHECK(c->stock[COLONIZE_CARGO_TOOLS] == tools0 + 20, "picker unload TOOLS");
        CHECK(c->stock[COLONIZE_CARGO_SILVER] == silver0 - 20, "picker load SILVER");
        int food_left = 0;
        int silver_on = 0;
        const int nh = units_goods_hold_count(&units, wid);
        for (int h = 0; h < nh; ++h) {
          if (w->hold_goods_type[h] == COLONIZE_CARGO_FOOD) {
            food_left += w->hold_goods_amount[h];
          }
          if (w->hold_goods_type[h] == COLONIZE_CARGO_SILVER) {
            silver_on += w->hold_goods_amount[h];
          }
        }
        CHECK(food_left == 20, "picker FOOD stays aboard");
        CHECK(silver_on == 20, "picker SILVER aboard");

        /* Europe: unload list only */
        ColonizeCol1TradeStop eu;
        memset(&eu, 0, sizeof(eu));
        eu.colony_index = 999;
        colonies_trade_stop_set_cargos(&eu, unload_t, 1, load_t, 1);
        /* Force load_count 0 for Europe sell path semantics in setter use. */
        colonies_trade_stop_set_cargos(&eu, unload_t, 1, NULL, 0);
        CHECK(eu.unload_count == 1 && eu.load_count == 0, "Europe picker unload-only");
      }
    }
  }

  /*
   * TRADE stop Col1 nibbles: unload only listed cargo; load listed cargo.
   * Cite: ColonizeCol1TradeStop; colonies_trade_route_service_stop.
   */
  {
    ColonizeUnitPool units;
    units_reset(&units);
    units.type_count = 1;
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
    units.types[0].movement = 2;
    units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    units.types[0].cargo = 2;

    ColonizeColony* c = colonies_get_mut(&pool, cid);
    CHECK(c != NULL, "trade-nibble colony");
    if (c) {
      c->stock[COLONIZE_CARGO_TOOLS] = 80;
      c->stock[COLONIZE_CARGO_LUMBER] = 80;
      c->stock[COLONIZE_CARGO_FOOD] = 5;
      const int tools0 = c->stock[COLONIZE_CARGO_TOOLS];
      const int lumber0 = c->stock[COLONIZE_CARGO_LUMBER];

      const int wid = units_spawn(&units, 0, c->x, c->y);
      ColonizeUnit* w = units_get(&units, wid);
      CHECK(w != NULL, "trade-nibble wagon spawn");
      if (w) {
        w->nation_id = c->nation_id;
        CHECK(units_load_goods(&units, wid, COLONIZE_CARGO_FOOD, 20) == 20, "load FOOD onto wagon");
        CHECK(units_load_goods(&units, wid, COLONIZE_CARGO_TOOLS, 20) == 20, "load TOOLS onto wagon");

        ColonizeCol1TradeStop st;
        memset(&st, 0, sizeof(st));
        st.colony_index = (uint16_t)cid;
        st.unload_count = 1;
        col1_trade_nibble_set(st.unload_cargo_nibbles, 0, COLONIZE_CARGO_TOOLS);
        st.load_count = 1;
        col1_trade_nibble_set(st.load_cargo_nibbles, 0, COLONIZE_CARGO_LUMBER);

        CHECK(
          colonies_trade_route_service_stop(&pool, cid, &units, wid, &st) == 1,
          "trade nibble service moves cargo"
        );
        CHECK(c->stock[COLONIZE_CARGO_TOOLS] == tools0 + 20, "unload TOOLS only into warehouse");
        CHECK(c->stock[COLONIZE_CARGO_FOOD] == 5, "FOOD stay on wagon (not in unload list)");
        int food_left = 0;
        int lumber_on = 0;
        int tools_on = 0;
        const int nh = units_goods_hold_count(&units, wid);
        for (int h = 0; h < nh; ++h) {
          if (w->hold_goods_type[h] == COLONIZE_CARGO_FOOD) {
            food_left += w->hold_goods_amount[h];
          }
          if (w->hold_goods_type[h] == COLONIZE_CARGO_LUMBER) {
            lumber_on += w->hold_goods_amount[h];
          }
          if (w->hold_goods_type[h] == COLONIZE_CARGO_TOOLS) {
            tools_on += w->hold_goods_amount[h];
          }
        }
        CHECK(food_left == 20, "FOOD remains on wagon after selective unload");
        CHECK(tools_on == 0, "TOOLS hold cleared");
        CHECK(lumber_on == 20, "load LUMBER per Col1 load nibble");
        CHECK(c->stock[COLONIZE_CARGO_LUMBER] == lumber0 - 20, "warehouse LUMBER decreased");
        CHECK(
          col1_trade_nibble_cargo(st.unload_cargo_nibbles, 0) == COLONIZE_CARGO_TOOLS,
          "nibble pack TOOLS low"
        );
        CHECK(
          col1_trade_nibble_cargo(st.load_cargo_nibbles, 0) == COLONIZE_CARGO_LUMBER,
          "nibble pack LUMBER low"
        );
      }
    }
  }

  if (failures == 0) {
    printf("smoke_colonies: all checks passed\n");
    if (smoke_found_chrome() != 0) {
      return 1;
    }
    if (smoke_full_chrome() != 0) {
      return 1;
    }
    if (smoke_alreadyhave_chrome() != 0) {
      return 1;
    }
    if (smoke_noteacher_chrome() != 0) {
      return 1;
    }
    if (smoke_needschool_chrome() != 0) {
      return 1;
    }
    if (smoke_hammers_purchased_buy() != 0) {
      return 1;
    }
    if (smoke_warehouse_capitol_levels() != 0) {
      return 1;
    }
    return 0;
  }
  fprintf(stderr, "smoke_colonies: %d failure(s)\n", failures);
  return 1;
}
