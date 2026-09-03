#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/ai_diplo.h"
#include "core/ai_popup.h"
#include "core/col1_save.h"
#include "core/col1_stuff_census.h"
#include "core/colony.h"
#include "core/colony_preview.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/ai_king.h"
#include "core/turn.h"
#include "core/units.h"
#include "platform/diagnostics.h"

static int expect_date(uint16_t year, uint16_t autumn, const char* want) {
  char got[32];
  turn_format_date(year, autumn, got, sizeof(got));
  if (strcmp(got, want) != 0) {
    fprintf(stderr, "date expected '%s' got '%s'\n", want, got);
    return 1;
  }
  return 0;
}

static int expect_cal(
  uint16_t year,
  uint16_t autumn,
  uint32_t turn,
  uint16_t ey,
  uint16_t ea,
  uint32_t et
) {
  turn_advance_calendar(&year, &autumn, &turn);
  if (year != ey || autumn != ea || turn != et) {
    fprintf(
      stderr,
      "calendar got year=%u autumn=%u turn=%u expected %u/%u/%u\n",
      year,
      autumn,
      turn,
      ey,
      ea,
      et
    );
    return 1;
  }
  return 0;
}

/*
 * Phase P century tip chrome (helper keeps large locals off main's stack).
 * Drives the crossing via a skilled Distiller (Sugar → Rum craft, output 6
 * rum/turn — no map/field-yield needed, unlike lumber): 2026-08-16 removed
 * the "invent 1 lumber for Carpenter demos" stub this used to lean on (a
 * fabricated resource, never DOS-cited — see turn.c's Carpenter hammers
 * block fix), so this now drives a real production path instead.
 */
static int unit_century_cargoready(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  snprintf(
    pool.building_types[0].name, sizeof(pool.building_types[0].name), "Rum Distiller's House"
  );
  pool.building_type_count = 1;
  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Salem");
  col->building_in_production = -1;
  col->warehouse_level = 5; /* cap 300; 100 cross → CARGOREADY0 */
  col->has_building[0] = true;
  col->stock[COLONIZE_CARGO_SUGAR] = 50;
  col->stock[COLONIZE_CARGO_RUM] = 99;
  col->stock[COLONIZE_CARGO_FOOD] = 50;
  col->colonists[0].active = true;
  col->colonists[0].building_type = 0;
  col->colonists[0].profession = COLONIZE_PROF_DISTILLER;
  col->colonists[0].field_job = -1;
  col->colonist_count = 1;
  col->population = 1;
  pool.colony_count = 1;

  EuropeScreen eu;
  memset(&eu, 0, sizeof(eu));
  eu.cargo_count = COLONIZE_CARGO_COUNT;
  for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
    eu.cargo[i].bid = 1;
  }
  snprintf(eu.cargo[COLONIZE_CARGO_RUM].name, sizeof(eu.cargo[0].name), "Rum");
  AiPopupState pops;
  ai_popup_init(&pops);
  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "century tip: GAME.TXT load failed\n");
    return 1;
  }

  ColonizeCol1Save tipcol;
  memset(&tipcol, 0, sizeof(tipcol));

  ColonizeTurnResult prod;
  memset(&prod, 0, sizeof(prod));
  turn_run_colony_production(&pool, NULL, &tipcol, &eu, 0, &prod, &pops, &game_txt, NULL);
  if (strstr(eu.status, "New cargo") == NULL || !tipcol.head.tut3.nr6) {
    fprintf(
      stderr,
      "century tip want New cargo+latch rum=%d latch=%u '%s'\n",
      col->stock[COLONIZE_CARGO_RUM],
      (unsigned)tipcol.head.tut3.nr6,
      eu.status
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "ready") == NULL &&
       strstr(pops.queue[0].body, "Salem") == NULL &&
       strstr(pops.queue[0].body, "Rum") == NULL)) {
    fprintf(
      stderr,
      "century CARGOREADY0 weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  if (strstr(pops.queue[0].body, "storage capacity") != NULL) {
    fprintf(stderr, "century tip not at cap must be CARGOREADY0 got '%s'\n", pops.queue[0].body);
    assets_msg_free(&game_txt);
    return 1;
  }
  /*
   * bugs.md: every 100-unit crossing announces itself again — the latch only
   * silences the one-off @TUTORIAL6 hint, not @CARGOREADY* itself.
   */
  col->stock[COLONIZE_CARGO_SUGAR] = 50;
  col->stock[COLONIZE_CARGO_RUM] = 99;
  eu.status[0] = '\0';
  ai_popup_clear(&pops);
  memset(&prod, 0, sizeof(prod));
  turn_run_colony_production(&pool, NULL, &tipcol, &eu, 0, &prod, &pops, &game_txt, NULL);
  if (strstr(eu.status, "New cargo") == NULL || pops.queue_count != 1) {
    fprintf(stderr, "century tip second crossing got '%s' q=%d\n", eu.status, pops.queue_count);
    assets_msg_free(&game_txt);
    return 1;
  }
  if (strstr(pops.queue[0].body, "pick up this cargo") != NULL) {
    fprintf(stderr, "century tip @TUTORIAL6 must fire only once\n");
    assets_msg_free(&game_txt);
    return 1;
  }

  /* Option off (report_new_cargos_available) → nothing at all. */
  tipcol.head.colony_report_options.report_new_cargos_available = 1;
  col->stock[COLONIZE_CARGO_SUGAR] = 50;
  col->stock[COLONIZE_CARGO_RUM] = 99;
  eu.status[0] = '\0';
  ai_popup_clear(&pops);
  memset(&prod, 0, sizeof(prod));
  turn_run_colony_production(&pool, NULL, &tipcol, &eu, 0, &prod, &pops, &game_txt, NULL);
  if (strstr(eu.status, "New cargo") != NULL || pops.queue_count > 0) {
    fprintf(stderr, "century tip option-off got '%s' q=%d\n", eu.status, pops.queue_count);
    assets_msg_free(&game_txt);
    return 1;
  }
  tipcol.head.colony_report_options.report_new_cargos_available = 0;
  fprintf(stderr, "warehouse century tip ok\n");

  /* At exact basic warehouse cap → @CARGOREADY1. */
  tipcol.head.tut3.nr6 = 0;
  col->warehouse_level = 0; /* cap 100 */
  col->stock[COLONIZE_CARGO_SUGAR] = 50;
  col->stock[COLONIZE_CARGO_RUM] = 94; /* 94 + 6 rum craft = exactly 100 */
  eu.status[0] = '\0';
  ai_popup_clear(&pops);
  memset(&prod, 0, sizeof(prod));
  turn_run_colony_production(&pool, NULL, &tipcol, &eu, 0, &prod, &pops, &game_txt, NULL);
  if (col->stock[COLONIZE_CARGO_RUM] != 100) {
    fprintf(stderr, "century CARGOREADY1 rum want 100 got %d\n", col->stock[COLONIZE_CARGO_RUM]);
    assets_msg_free(&game_txt);
    return 1;
  }
  if (pops.queue_count < 1 || strstr(pops.queue[0].body, "storage capacity") == NULL) {
    fprintf(
      stderr,
      "century CARGOREADY1 want storage capacity q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  assets_msg_free(&game_txt);
  fprintf(stderr, "warehouse century CARGOREADY1 ok\n");
  return 0;
}

static int unit_eot_fog_reveal(void) {
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "fog map_alloc: %s\n", err);
    return 1;
  }
  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Colonists");
  units.type_count = 1;
  const int id = units_spawn(&units, 0, 3, 3);
  ColonizeUnit* u = units_get(&units, id);
  if (!u) {
    fprintf(stderr, "fog unit spawn failed\n");
    map_free(&map);
    return 1;
  }
  units_set_nation(u, 0);
  map_reveal_radius(&map, u->x, u->y, 0, 1);
  if (!map_tile_seen_by(&map, 3, 3, 0) || !map_tile_seen_by(&map, 2, 3, 0) ||
      !map_tile_seen_by(&map, 4, 4, 0)) {
    fprintf(stderr, "fog reveal radius-1 missed neighbour\n");
    map_free(&map);
    return 1;
  }
  if (map_tile_seen_by(&map, 7, 7, 0) || map_tile_seen_by(&map, 3, 3, 1)) {
    fprintf(stderr, "fog reveal leaked to far tile or other nation\n");
    map_free(&map);
    return 1;
  }
  map_reveal_radius(&map, 1, 1, 0, 2);
  if (!map_tile_seen_by(&map, 1, 1, 0) || !map_tile_seen_by(&map, 0, 0, 0)) {
    fprintf(stderr, "fog colony radius-2 missed\n");
    map_free(&map);
    return 1;
  }
  map_free(&map);
  fprintf(stderr, "EOT fog reveal ok\n");
  return 0;
}

/* Phase K @NEEDTOOLS0 when construction blocked on tools=0. */
static int unit_needtools0(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") ||
      !colonies_load_buildings(&pool, &names)) {
    fprintf(stderr, "needtools0: buildings load failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int carpenter = colonies_find_building(&pool, "Carpenter's Shop");
  const int press = colonies_find_building(&pool, "Printing Press");
  if (carpenter < 0 || press < 0) {
    fprintf(stderr, "needtools0: missing Carpenter/Printing Press\n");
    assets_msg_free(&names);
    return 1;
  }
  const ColonizeBuildingType* bt = colonies_building_type(&pool, press);
  if (!bt || bt->tools_cost <= 0 || bt->hammers <= 0) {
    fprintf(stderr, "needtools0: Printing Press should need tools\n");
    assets_msg_free(&names);
    return 1;
  }

  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Boston");
  col->has_building[carpenter] = true;
  col->building_in_production = press;
  col->hammers = bt->hammers;
  col->stock[COLONIZE_CARGO_FOOD] = 50;
  col->stock[COLONIZE_CARGO_LUMBER] = 50;
  col->stock[COLONIZE_CARGO_TOOLS] = 0;
  col->colonists[0].active = true;
  col->colonists[0].building_type = carpenter;
  col->colonists[0].field_job = -1;
  for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
    col->tiles[t] = -1;
  }
  col->colonist_count = 1;
  col->population = 1;
  pool.colony_count = 1;

  EuropeScreen eu;
  memset(&eu, 0, sizeof(eu));
  AiPopupState pops;
  ai_popup_init(&pops);
  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "needtools0: GAME.TXT load failed\n");
    assets_msg_free(&names);
    return 1;
  }
  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));

  ColonizeTurnResult prod;
  memset(&prod, 0, sizeof(prod));
  turn_run_colony_production(&pool, NULL, &col1, &eu, 0, &prod, &pops, &game_txt, NULL);
  if (col->has_building[press]) {
    fprintf(stderr, "needtools0: should not complete without tools\n");
    assets_msg_free(&game_txt);
    assets_msg_free(&names);
    return 1;
  }
  if (strstr(eu.status, "tools") == NULL && strstr(eu.status, "Tools") == NULL) {
    fprintf(stderr, "needtools0: status want Need tools got '%s'\n", eu.status);
    assets_msg_free(&game_txt);
    assets_msg_free(&names);
    return 1;
  }
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "tools") == NULL &&
       strstr(pops.queue[0].body, "Boston") == NULL &&
       strstr(pops.queue[0].body, "Printing") == NULL)) {
    fprintf(
      stderr,
      "needtools0: NEEDTOOLS0 popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    assets_msg_free(&names);
    return 1;
  }

  assets_msg_free(&game_txt);
  assets_msg_free(&names);
  fprintf(stderr, "unit_turn: NEEDTOOLS0 chrome ok\n");
  return 0;
}

/* Phase K @NEEDTOOLS when construction blocked on tools>0 but short. */
static int unit_needtools(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") ||
      !colonies_load_buildings(&pool, &names)) {
    fprintf(stderr, "needtools: buildings load failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int carpenter = colonies_find_building(&pool, "Carpenter's Shop");
  const int press = colonies_find_building(&pool, "Printing Press");
  if (carpenter < 0 || press < 0) {
    fprintf(stderr, "needtools: missing Carpenter/Printing Press\n");
    assets_msg_free(&names);
    return 1;
  }
  const ColonizeBuildingType* bt = colonies_building_type(&pool, press);
  if (!bt || bt->tools_cost < 2 || bt->hammers <= 0) {
    fprintf(stderr, "needtools: Printing Press should need >=2 tools\n");
    assets_msg_free(&names);
    return 1;
  }
  const int have = bt->tools_cost / 2;
  if (have < 1 || have >= bt->tools_cost) {
    fprintf(stderr, "needtools: bad partial tools have=%d cost=%d\n", have, bt->tools_cost);
    assets_msg_free(&names);
    return 1;
  }

  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Boston");
  col->has_building[carpenter] = true;
  col->building_in_production = press;
  col->hammers = bt->hammers;
  col->stock[COLONIZE_CARGO_FOOD] = 50;
  col->stock[COLONIZE_CARGO_LUMBER] = 50;
  col->stock[COLONIZE_CARGO_TOOLS] = have;
  col->colonists[0].active = true;
  col->colonists[0].building_type = carpenter;
  col->colonists[0].field_job = -1;
  for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
    col->tiles[t] = -1;
  }
  col->colonist_count = 1;
  col->population = 1;
  pool.colony_count = 1;

  EuropeScreen eu;
  memset(&eu, 0, sizeof(eu));
  AiPopupState pops;
  ai_popup_init(&pops);
  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "needtools: GAME.TXT load failed\n");
    assets_msg_free(&names);
    return 1;
  }
  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));

  ColonizeTurnResult prod;
  memset(&prod, 0, sizeof(prod));
  turn_run_colony_production(&pool, NULL, &col1, &eu, 0, &prod, &pops, &game_txt, NULL);
  if (col->has_building[press]) {
    fprintf(stderr, "needtools: should not complete with short tools\n");
    assets_msg_free(&game_txt);
    assets_msg_free(&names);
    return 1;
  }
  if (strstr(eu.status, "tools") == NULL && strstr(eu.status, "Tools") == NULL) {
    fprintf(stderr, "needtools: status want Need tools got '%s'\n", eu.status);
    assets_msg_free(&game_txt);
    assets_msg_free(&names);
    return 1;
  }
  if (pops.queue_count < 1 ||
      strstr(pops.queue[0].body, "tools") == NULL ||
      (strstr(pops.queue[0].body, "Boston") == NULL &&
       strstr(pops.queue[0].body, "Printing") == NULL) ||
      (strstr(pops.queue[0].body, "Only") == NULL &&
       strstr(pops.queue[0].body, "only") == NULL)) {
    fprintf(
      stderr,
      "needtools: NEEDTOOLS popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    assets_msg_free(&names);
    return 1;
  }

  assets_msg_free(&game_txt);
  assets_msg_free(&names);
  fprintf(stderr, "unit_turn: NEEDTOOLS chrome ok\n");
  return 0;
}

/* Phase G @TRAINFAIL when ready teacher has no eligible students. */
static int unit_trainfail(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Schoolhouse");
  pool.building_type_count = 1;

  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Roanoke");
  col->building_in_production = -1;
  col->has_building[0] = true;
  col->stock[COLONIZE_CARGO_FOOD] = 50;
  col->colonists[0].active = true;
  col->colonists[0].profession = COLONIZE_PROF_TEACHER;
  col->colonists[0].building_type = 0;
  col->colonists[0].field_job = -1;
  col->colonists[0].turns_in_job = 3; /* one tick → 4 ≥ need */
  col->colonist_count = 1;
  col->population = 1;
  pool.colony_count = 1;

  EuropeScreen eu;
  memset(&eu, 0, sizeof(eu));
  AiPopupState pops;
  ai_popup_init(&pops);
  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "trainfail: GAME.TXT load failed\n");
    return 1;
  }
  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  ColonizeTurnResult prod;
  memset(&prod, 0, sizeof(prod));
  turn_run_colony_production(&pool, NULL, &col1, &eu, 0, &prod, &pops, &game_txt, NULL);
  if (strstr(eu.status, "No students") == NULL) {
    fprintf(stderr, "trainfail: status want No students got '%s'\n", eu.status);
    assets_msg_free(&game_txt);
    return 1;
  }
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "Roanoke") == NULL &&
       strstr(pops.queue[0].body, "teacher") == NULL &&
       strstr(pops.queue[0].body, "specialty") == NULL &&
       strstr(pops.queue[0].body, "students") == NULL)) {
    fprintf(
      stderr,
      "trainfail: TRAINFAIL popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  assets_msg_free(&game_txt);
  fprintf(stderr, "unit_turn: TRAINFAIL chrome ok\n");
  return 0;
}

/* Phase G @TRAINPROFESSION when school graduation assigns a specialty. */
static int unit_trainprofession(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Schoolhouse");
  pool.building_type_count = 1;

  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Plymouth");
  col->building_in_production = -1;
  col->has_building[0] = true;
  col->stock[COLONIZE_CARGO_FOOD] = 50;
  col->colonists[0].active = true;
  col->colonists[0].profession = COLONIZE_PROF_TEACHER;
  col->colonists[0].building_type = 0;
  col->colonists[0].field_job = -1;
  col->colonists[0].turns_in_job = 3; /* one tick → 4 ≥ need */
  col->colonists[1].active = true;
  col->colonists[1].profession = COLONIZE_PROF_FREE_COLONIST;
  col->colonists[1].building_type = 0;
  col->colonists[1].field_job = -1;
  col->colonists[1].turns_in_job = 0;
  col->colonist_count = 2;
  col->population = 2;
  pool.colony_count = 1;

  EuropeScreen eu;
  memset(&eu, 0, sizeof(eu));
  AiPopupState pops;
  ai_popup_init(&pops);
  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "trainprof: GAME.TXT load failed\n");
    return 1;
  }
  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  ColonizeTurnResult prod;
  memset(&prod, 0, sizeof(prod));
  turn_run_colony_production(&pool, NULL, &col1, &eu, 0, &prod, &pops, &game_txt, NULL);
  if (col->colonists[1].profession != COLONIZE_JOB_FARMER) {
    fprintf(
      stderr,
      "trainprof: student want Farmer(%d) got %d\n",
      COLONIZE_JOB_FARMER,
      col->colonists[1].profession
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  if (strstr(eu.status, "graduate") == NULL) {
    fprintf(stderr, "trainprof: status want graduate got '%s'\n", eu.status);
    assets_msg_free(&game_txt);
    return 1;
  }
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "Plymouth") == NULL &&
       strstr(pops.queue[0].body, "Farmer") == NULL &&
       strstr(pops.queue[0].body, "profession") == NULL &&
       strstr(pops.queue[0].body, "learned") == NULL)) {
    fprintf(
      stderr,
      "trainprof: TRAINPROFESSION popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  assets_msg_free(&game_txt);
  fprintf(stderr, "unit_turn: TRAINPROFESSION chrome ok\n");
  return 0;
}

/* Phase G ladder: Criminal → Indentured + @TRAINCRIMINAL. */
static int unit_traincriminal(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Schoolhouse");
  pool.building_type_count = 1;

  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Boston");
  col->building_in_production = -1;
  col->has_building[0] = true;
  col->stock[COLONIZE_CARGO_FOOD] = 50;
  col->colonists[0].active = true;
  col->colonists[0].profession = COLONIZE_PROF_TEACHER;
  col->colonists[0].building_type = 0;
  col->colonists[0].field_job = -1;
  col->colonists[0].turns_in_job = 3;
  col->colonists[1].active = true;
  col->colonists[1].profession = COLONIZE_PROF_CRIMINAL;
  col->colonists[1].building_type = 0;
  col->colonists[1].field_job = -1;
  col->colonists[1].turns_in_job = 0;
  col->colonist_count = 2;
  col->population = 2;
  pool.colony_count = 1;

  EuropeScreen eu;
  memset(&eu, 0, sizeof(eu));
  AiPopupState pops;
  ai_popup_init(&pops);
  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "traincriminal: GAME.TXT load failed\n");
    return 1;
  }
  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  ColonizeTurnResult prod;
  memset(&prod, 0, sizeof(prod));
  turn_run_colony_production(&pool, NULL, &col1, &eu, 0, &prod, &pops, &game_txt, NULL);
  if (col->colonists[1].profession != COLONIZE_PROF_INDENTURED) {
    fprintf(
      stderr,
      "traincriminal: want Indentured(%d) got %d\n",
      COLONIZE_PROF_INDENTURED,
      col->colonists[1].profession
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "Boston") == NULL &&
       strstr(pops.queue[0].body, "criminal") == NULL &&
       strstr(pops.queue[0].body, "Criminal") == NULL &&
       strstr(pops.queue[0].body, "indentured") == NULL)) {
    fprintf(
      stderr,
      "traincriminal: TRAINCRIMINAL popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  assets_msg_free(&game_txt);
  fprintf(stderr, "unit_turn: TRAINCRIMINAL chrome ok\n");
  return 0;
}

/* Phase G ladder: Indentured → Free + @TRAININDENTURED. */
static int unit_trainindentured(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Schoolhouse");
  pool.building_type_count = 1;

  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Salem");
  col->building_in_production = -1;
  col->has_building[0] = true;
  col->stock[COLONIZE_CARGO_FOOD] = 50;
  col->colonists[0].active = true;
  col->colonists[0].profession = COLONIZE_PROF_TEACHER;
  col->colonists[0].building_type = 0;
  col->colonists[0].field_job = -1;
  col->colonists[0].turns_in_job = 3;
  col->colonists[1].active = true;
  col->colonists[1].profession = COLONIZE_PROF_INDENTURED;
  col->colonists[1].building_type = 0;
  col->colonists[1].field_job = -1;
  col->colonists[1].turns_in_job = 0;
  col->colonist_count = 2;
  col->population = 2;
  pool.colony_count = 1;

  EuropeScreen eu;
  memset(&eu, 0, sizeof(eu));
  AiPopupState pops;
  ai_popup_init(&pops);
  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "trainindentured: GAME.TXT load failed\n");
    return 1;
  }
  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  ColonizeTurnResult prod;
  memset(&prod, 0, sizeof(prod));
  turn_run_colony_production(&pool, NULL, &col1, &eu, 0, &prod, &pops, &game_txt, NULL);
  if (col->colonists[1].profession != COLONIZE_PROF_FREE_COLONIST) {
    fprintf(
      stderr,
      "trainindentured: want Free(%d) got %d\n",
      COLONIZE_PROF_FREE_COLONIST,
      col->colonists[1].profession
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "Salem") == NULL &&
       strstr(pops.queue[0].body, "indentured") == NULL &&
       strstr(pops.queue[0].body, "free") == NULL &&
       strstr(pops.queue[0].body, "Free") == NULL)) {
    fprintf(
      stderr,
      "trainindentured: TRAININDENTURED popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  assets_msg_free(&game_txt);
  fprintf(stderr, "unit_turn: TRAININDENTURED chrome ok\n");
  return 0;
}

/* Phase H @TRAINPROFESSION when Free Colonist discovers field skill. */
static int unit_phase_h_trainprofession(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Concord");
  col->building_in_production = -1;
  col->stock[COLONIZE_CARGO_FOOD] = 500;
  col->colonists[0].active = true;
  col->colonists[0].profession = COLONIZE_PROF_FREE_COLONIST;
  col->colonists[0].building_type = -1;
  col->colonists[0].field_job = COLONIZE_JOB_FARMER;
  col->colonist_count = 1;
  col->population = 1;
  col->tiles[0] = 0;
  pool.colony_count = 1;

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "phaseh: GAME.TXT load failed\n");
    return 1;
  }

  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.head.year = 1492;
  EuropeScreen eu;
  memset(&eu, 0, sizeof(eu));
  AiPopupState pops;
  ai_popup_init(&pops);
  ColonizeDosRng rng;
  dos_rng_seed(&rng, 1);
  int discovered = 0;
  for (unsigned t = 0; t < 5000u; ++t) {
    col1.head.turn = (uint16_t)(t & 0xffffu);
    col->colonists[0].profession = COLONIZE_PROF_FREE_COLONIST;
    col->colonists[0].field_job = COLONIZE_JOB_FARMER;
    col->stock[COLONIZE_CARGO_FOOD] = 500;
    eu.status[0] = '\0';
    ai_popup_init(&pops);
    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, &col1, &eu, 0, &prod, &pops, &game_txt, &rng);
    if (col->colonists[0].profession == COLONIZE_JOB_FARMER) {
      discovered = 1;
      if (pops.queue_count < 1 ||
          (strstr(pops.queue[0].body, "Concord") == NULL &&
           strstr(pops.queue[0].body, "Farmer") == NULL &&
           strstr(pops.queue[0].body, "learned") == NULL &&
           strstr(pops.queue[0].body, "profession") == NULL)) {
        fprintf(
          stderr,
          "phaseh: TRAINPROFESSION popup weak q=%d body='%s' status='%s'\n",
          pops.queue_count,
          pops.queue_count > 0 ? pops.queue[0].body : "",
          eu.status
        );
        assets_msg_free(&game_txt);
        return 1;
      }
      break;
    }
  }
  assets_msg_free(&game_txt);
  if (!discovered) {
    fprintf(stderr, "phaseh: no field skill discover in 5000 ticks\n");
    return 1;
  }
  fprintf(stderr, "unit_turn: Phase H TRAINPROFESSION chrome ok\n");
  return 0;
}

int main(void) {
  diag_init(0, NULL);

  if (unit_needtools0() != 0) {
    return 1;
  }
  if (unit_needtools() != 0) {
    return 1;
  }
  if (unit_trainfail() != 0) {
    return 1;
  }
  if (unit_trainprofession() != 0) {
    return 1;
  }
  if (unit_traincriminal() != 0) {
    return 1;
  }
  if (unit_trainindentured() != 0) {
    return 1;
  }
  if (unit_phase_h_trainprofession() != 0) {
    return 1;
  }

  if (expect_date(1492, 0, "Spring 1492") != 0 || expect_date(1600, 1, "Autumn 1600") != 0) {
    return 1;
  }

  /* Pre-1600: one year per turn. */
  if (expect_cal(1492, 0, 0, 1493, 0, 1) != 0 || expect_cal(1493, 0, 1, 1494, 0, 2) != 0 ||
      expect_cal(1599, 0, 107, 1600, 0, 108) != 0) {
    return 1;
  }

  /* From 1600: Spring → Autumn → next Spring. */
  if (expect_cal(1600, 0, 108, 1600, 1, 109) != 0 ||
      expect_cal(1600, 1, 109, 1601, 0, 110) != 0 ||
      expect_cal(1601, 0, 110, 1601, 1, 111) != 0) {
    return 1;
  }

  /* Production without fields: consume 2 food / colonist. */
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  memset(c, 0, sizeof(*c));
  c->active = true;
  c->id = 1;
  c->building_in_production = -1;
  c->stock[COLONIZE_CARGO_FOOD] = 10;
  c->colonists[0].active = true;
  c->colonists[0].unit_type_index = 0;
  c->colonists[0].building_type = -1;
  c->colonists[0].field_job = -1;
  for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
    c->tiles[t] = -1;
  }
  c->colonist_count = 1;
  c->population = 1;
  colonies.colony_count = 1;

  ColonizeTurnResult prod;
  memset(&prod, 0, sizeof(prod));
  turn_colony_free_production(&colonies, c, NULL, &prod, NULL);
  if (c->stock[COLONIZE_CARGO_FOOD] != 8) { /* 10 - 2 */
    fprintf(stderr, "food expected 8 got %d\n", c->stock[COLONIZE_CARGO_FOOD]);
    return 1;
  }
  if (prod.colonies_produced != 1) {
    fprintf(stderr, "expected 1 colony produced\n");
    return 1;
  }

  /* Yield chart: plains farmer / ocean fisherman. */
  if (colony_yield_job_cargo(COLONIZE_JOB_LUMBERJACK) != COLONIZE_CARGO_LUMBER) {
    fprintf(stderr, "lumberjack cargo mapping wrong\n");
    return 1;
  }

  /* Full turn_end advances calendar and refreshes human MP. */
  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Scout");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  const int uid = units_spawn(&units, 0, 5, 5);
  if (uid < 0) {
    fprintf(stderr, "spawn failed\n");
    return 1;
  }
  ColonizeUnit* u = units_get(&units, uid);
  u->nation_id = 0;
  u->moves_left = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.needed_crosses = TURN_DEFAULT_NEEDED_CROSSES;

  uint32_t turn_number = 2;
  uint16_t year = 1494;
  uint16_t autumn = 0;
  char status[128];
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn_number;
  ctx.game_year = &year;
  ctx.game_autumn = &autumn;
  ctx.human_nation = 0;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.europe = &europe;
  ctx.status = status;
  ctx.status_size = sizeof(status);

  ColonizeTurnResult end = turn_end(&ctx);
  if (!end.advanced || year != 1495 || autumn != 0 || turn_number != 3) {
    fprintf(
      stderr,
      "turn_end calendar fail year=%u autumn=%u turn=%u\n",
      year,
      autumn,
      turn_number
    );
    return 1;
  }
  if (u->moves_left != 4 * UNITS_MP_PER_TILE) {
    fprintf(stderr, "human MP not refreshed got %d\n", u->moves_left);
    return 1;
  }
  if (strstr(status, "1495") == NULL) {
    fprintf(stderr, "status missing date: %s\n", status);
    return 1;
  }

  /* Next-unit selection wraps to units with moves. */
  const int uid2 = units_spawn_allow_stack(&units, 0, 6, 6);
  ColonizeUnit* u2 = units_get(&units, uid2);
  u2->nation_id = 0;
  u2->moves_left = 2 * UNITS_MP_PER_TILE;
  u->moves_left = 0;
  units.selected_id = uid;
  if (!turn_select_next_unit(&units, 0) || units.selected_id != uid2) {
    fprintf(stderr, "wait-next failed selected=%d\n", units.selected_id);
    return 1;
  }

  /* Turn-owner colors: NAMES.TXT @COUNTRY; England fill uses saturated red 112. */
  if (turn_nation_color(0) != 112 || turn_nation_color(1) != 9 || turn_nation_color(2) != 14 ||
      turn_nation_color(3) != 13) {
    fprintf(stderr, "european turn colors mismatch\n");
    return 1;
  }
  if (turn_nation_color(4) != 97 || turn_nation_color(11) != 71) {
    fprintf(stderr, "tribe turn colors mismatch\n");
    return 1;
  }

  {
    uint8_t pixels[320 * 200];
    ColonizeFramebuffer8 fb;
    fb.width = 320;
    fb.height = 200;
    fb.pixels = pixels;
    memset(pixels, 0, sizeof(pixels));
    turn_draw_owner_indicator(&fb, 2); /* Spain = 14 */
    const int x0 = TURN_OWNER_INDICATOR_X;
    const int y0 = TURN_OWNER_INDICATOR_Y;
    if (pixels[y0 * 320 + x0] != 14 ||
        pixels[(y0 + TURN_OWNER_INDICATOR_H - 1) * 320 + (x0 + TURN_OWNER_INDICATOR_W - 1)] != 14) {
      fprintf(stderr, "owner indicator pixels not filled\n");
      return 1;
    }
    if (pixels[y0 * 320 + (x0 - 1)] != 0) {
      fprintf(stderr, "owner indicator spilled left\n");
      return 1;
    }
  }

  /* Indicator is only armed during EURO/INDIAN processor steps. */
  {
    ColonizeTurnProcessor proc;
    turn_processor_start(&proc);
    if (turn_processor_show_indicator(&proc)) {
      fprintf(stderr, "indicator should be off at start\n");
      return 1;
    }
    uint32_t turn = 1;
    uint16_t year = 1492;
    uint16_t autumn = 0;
    ColonizeTurnContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.turn_number = &turn;
    ctx.game_year = &year;
    ctx.game_autumn = &autumn;
    ctx.human_nation = 0;
    int active = 0;
    ctx.active_turn_nation = &active;
    if (!turn_processor_advance(&proc, &ctx)) {
      fprintf(stderr, "setup should leave processor active\n");
      return 1;
    }
    /* DOS turn_owner_chrome runs at the head of every nation's EOT, the
     * human's included, so setup paints the box in the human colour. */
    if (!turn_processor_show_indicator(&proc) || active != 0) {
      fprintf(stderr, "setup should show the human turn-owner box\n");
      return 1;
    }
    /*
     * DOS order relative to the human's slot (turn/year_loop.c,
     * mid_pass_indian_rank.md): Euro slots above the human first, then the
     * 4d56_1b3a mid-pass Indian turns (4..11), then Euro slots below the
     * human. Human is England (0) here, so France (1) leads.
     */
    if (!turn_processor_advance(&proc, &ctx) || !turn_processor_show_indicator(&proc)) {
      fprintf(stderr, "indicator should show during euro AI step\n");
      return 1;
    }
    if (active != 1) {
      fprintf(stderr, "expected france active got %d\n", active);
      return 1;
    }
    for (int i = 0; i < 2; ++i) {
      if (!turn_processor_advance(&proc, &ctx)) {
        fprintf(stderr, "euro steps should keep processor active\n");
        return 1;
      }
    }
    if (!turn_processor_advance(&proc, &ctx) || !turn_processor_show_indicator(&proc)) {
      fprintf(stderr, "indicator should show during indian AI step\n");
      return 1;
    }
    if (active != 4) {
      fprintf(stderr, "expected first indian nation active got %d\n", active);
      return 1;
    }
    for (int i = 0; i < 7; ++i) {
      if (!turn_processor_advance(&proc, &ctx)) {
        fprintf(stderr, "indian steps should keep processor active\n");
        return 1;
      }
    }
  }

  /* Carpenter workplace + Stockade project completes via free production ticks. */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeMsgCatalog names;
    assets_msg_init(&names);
    if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") ||
        !colonies_load_buildings(&pool, &names)) {
      fprintf(stderr, "failed to load buildings for hammer test\n");
      assets_msg_free(&names);
      return 1;
    }
    const int carpenter = colonies_find_building(&pool, "Carpenter's Shop");
    const int stockade = colonies_find_building(&pool, "Stockade");
    if (carpenter < 0 || stockade < 0) {
      fprintf(stderr, "missing Carpenter/Stockade building types\n");
      assets_msg_free(&names);
      return 1;
    }
    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->nation_id = 0;
    snprintf(col->name, sizeof(col->name), "Jamestown");
    col->has_building[carpenter] = true;
    col->building_in_production = stockade;
    col->stock[COLONIZE_CARGO_FOOD] = 100; /* under food cap 199; no birth mid-build */
    col->stock[COLONIZE_CARGO_LUMBER] = 200;
    col->colonists[0].active = true;
    col->colonists[0].unit_type_index = 0;
    col->colonists[0].building_type = carpenter;
    col->colonists[0].field_job = -1;
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      col->tiles[t] = -1;
    }
    col->colonist_count = 1;
    col->population = 1;
    pool.colony_count = 1;

    EuropeScreen eu;
    memset(&eu, 0, sizeof(eu));
    AiPopupState pops;
    ai_popup_init(&pops);
    ColonizeMsgCatalog game_txt;
    assets_msg_init(&game_txt);
    (void)assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT");

    const ColonizeBuildingType* bt = colonies_building_type(&pool, stockade);
    const int need = bt ? bt->hammers : 64;
    ColonizeColonyProdDelta delta;
    bool completed = false;
    for (int t = 0; t < need + 8; ++t) {
      ColonizeTurnResult prod;
      memset(&prod, 0, sizeof(prod));
      memset(&delta, 0, sizeof(delta));
      ai_popup_clear(&pops);
      eu.status[0] = '\0';
      turn_run_colony_production(&pool, NULL, NULL, &eu, 0, &prod, &pops, &game_txt, NULL);
      if (prod.buildings_completed > 0 || col->has_building[stockade]) {
        completed = true;
        if (strstr(eu.status, "Stockade") == NULL && strstr(eu.status, "completed") == NULL) {
          fprintf(stderr, "BUILT: status want Stockade completed got '%s'\n", eu.status);
          assets_msg_free(&game_txt);
          assets_msg_free(&names);
          return 1;
        }
        if (pops.queue_count < 1) {
          fprintf(stderr, "BUILT: expected popup on complete\n");
          assets_msg_free(&game_txt);
          assets_msg_free(&names);
          return 1;
        }
        if (strstr(pops.queue[0].body, "Jamestown") == NULL &&
            strstr(pops.queue[0].body, "Stockade") == NULL &&
            strstr(pops.queue[0].body, "produces") == NULL) {
          fprintf(stderr, "BUILT: popup body weak: '%s'\n", pops.queue[0].body);
          assets_msg_free(&game_txt);
          assets_msg_free(&names);
          return 1;
        }
        break;
      }
    }
    if (!completed || !col->has_building[stockade]) {
      fprintf(
        stderr,
        "Stockade not completed after ticks (hammers=%d need=%d)\n",
        col->hammers,
        need
      );
      assets_msg_free(&game_txt);
      assets_msg_free(&names);
      return 1;
    }
    /* DOS never clears building_in_production on completion (player-
     * confirmed 2026-08-17, colony_prod02 golden: a real single DOS turn
     * shows it still pointing at the just-finished project) — only
     * has_building[]/hammers change, checked above. */
    if (col->building_in_production != stockade) {
      fprintf(
        stderr,
        "expected building_in_production to stay %d after complete, got %d\n",
        stockade,
        col->building_in_production
      );
      assets_msg_free(&game_txt);
      assets_msg_free(&names);
      return 1;
    }
    /* No carpenter assigned → no hammers even if Carpenter's Shop exists. */
    col->colonists[0].building_type = -1;
    col->hammers = 0;
    col->building_in_production = stockade;
    col->has_building[stockade] = false;
    {
      ColonizeTurnResult prod;
      ColonizeColonyProdDelta delta2;
      memset(&prod, 0, sizeof(prod));
      memset(&delta2, 0, sizeof(delta2));
      turn_colony_free_production(&pool, col, NULL, &prod, &delta2);
      if (delta2.hammers_added != 0 || col->hammers != 0) {
        fprintf(
          stderr,
          "expected no hammers without carpenter got delta=%d stock=%d\n",
          delta2.hammers_added,
          col->hammers
        );
        assets_msg_free(&game_txt);
        assets_msg_free(&names);
        return 1;
      }
    }
    assets_msg_free(&game_txt);
    assets_msg_free(&names);
    fprintf(stderr, "BUILT building chrome ok\n");
  }
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeMsgCatalog names;
    assets_msg_init(&names);
    if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") ||
        !colonies_load_buildings(&pool, &names)) {
      fprintf(stderr, "craft test: load buildings failed\n");
      assets_msg_free(&names);
      return 1;
    }
    const int distiller = colonies_find_building(&pool, "Rum Distiller's House");
    const int weaver = colonies_find_building(&pool, "Weaver's House");
    const int smith = colonies_find_building(&pool, "Blacksmith's House");
    const int armory = colonies_find_building(&pool, "Armory");
    const int fur = colonies_find_building(&pool, "Fur Trader's House");
    if (distiller < 0 || weaver < 0 || smith < 0 || armory < 0 || fur < 0) {
      fprintf(stderr, "craft test: missing building types\n");
      assets_msg_free(&names);
      return 1;
    }

    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->building_in_production = -1;
    col->has_building[distiller] = true;
    col->stock[COLONIZE_CARGO_FOOD] = 20;
    col->stock[COLONIZE_CARGO_SUGAR] = 10;
    col->colonists[0].active = true;
    col->colonists[0].building_type = distiller;
    col->colonists[0].field_job = -1;
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      col->tiles[t] = -1;
    }
    col->colonist_count = 1;
    col->population = 1;
    pool.colony_count = 1;

    ColonizeTurnResult prod;
    ColonizeColonyProdDelta delta;
    memset(&prod, 0, sizeof(prod));
    turn_colony_free_production(&pool, col, NULL, &prod, &delta);
    if (col->stock[COLONIZE_CARGO_RUM] != 3 || col->stock[COLONIZE_CARGO_SUGAR] != 7 ||
        delta.goods[COLONIZE_CARGO_RUM] != 3) {
      fprintf(
        stderr,
        "rum craft failed sugar=%d rum=%d dRum=%d\n",
        col->stock[COLONIZE_CARGO_SUGAR],
        col->stock[COLONIZE_CARGO_RUM],
        delta.goods[COLONIZE_CARGO_RUM]
      );
      assets_msg_free(&names);
      return 1;
    }

    /* No furs → no coats. */
    col->has_building[fur] = true;
    col->colonists[0].building_type = fur;
    col->stock[COLONIZE_CARGO_FURS] = 0;
    const int coats_before = col->stock[COLONIZE_CARGO_COATS];
    turn_colony_free_production(&pool, col, NULL, &prod, &delta);
    if (col->stock[COLONIZE_CARGO_COATS] != coats_before || delta.goods[COLONIZE_CARGO_COATS] != 0) {
      fprintf(stderr, "expected no coats without furs\n");
      assets_msg_free(&names);
      return 1;
    }

    col->has_building[weaver] = true;
    col->colonists[0].building_type = weaver;
    col->stock[COLONIZE_CARGO_COTTON] = 5;
    turn_colony_free_production(&pool, col, NULL, &prod, &delta);
    if (col->stock[COLONIZE_CARGO_CLOTH] != 3 || col->stock[COLONIZE_CARGO_COTTON] != 2) {
      fprintf(
        stderr,
        "cloth craft failed cotton=%d cloth=%d\n",
        col->stock[COLONIZE_CARGO_COTTON],
        col->stock[COLONIZE_CARGO_CLOTH]
      );
      assets_msg_free(&names);
      return 1;
    }

    col->has_building[smith] = true;
    col->has_building[armory] = true;
    col->colonists[0].building_type = smith;
    col->colonists[0].active = true;
    col->stock[COLONIZE_CARGO_ORE] = 10;
    col->stock[COLONIZE_CARGO_TOOLS] = 0;
    col->stock[COLONIZE_CARGO_MUSKETS] = 0;
    /* Two workers: smith + gunsmith. */
    col->colonists[1].active = true;
    col->colonists[1].building_type = armory;
    col->colonists[1].field_job = -1;
    col->colonist_count = 2;
    col->population = 2;
    turn_colony_free_production(&pool, col, NULL, &prod, &delta);
    /* Smith makes 3 tools from ore; gunsmith converts 3 tools → 3 muskets same tick. */
    if (col->stock[COLONIZE_CARGO_ORE] != 7 || col->stock[COLONIZE_CARGO_TOOLS] != 0 ||
        col->stock[COLONIZE_CARGO_MUSKETS] != 3) {
      fprintf(
        stderr,
        "tools/muskets craft failed ore=%d tools=%d guns=%d\n",
        col->stock[COLONIZE_CARGO_ORE],
        col->stock[COLONIZE_CARGO_TOOLS],
        col->stock[COLONIZE_CARGO_MUSKETS]
      );
      assets_msg_free(&names);
      return 1;
    }
    assets_msg_free(&names);
  }

  /* Production rules: convert +1 on tiles; convert/criminal floor in buildings; wrong expert → free rate. */
  {
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    char err[256];
    if (!map_load_mp("COLONIZE/AMER2.MP", &map, err, sizeof(err))) {
      fprintf(stderr, "production rules: map load: %s\n", err);
      return 1;
    }
    int fx = -1, fy = -1;
    for (int y = 1; y < (int)map.height - 1 && fx < 0; ++y) {
      for (int x = 1; x < (int)map.width - 1 && fx < 0; ++x) {
        if (colony_yield_for_tile(&map, x, y, COLONIZE_JOB_LUMBERJACK) == 2) {
          fx = x;
          fy = y;
        }
      }
    }
    if (fx < 0) {
      fprintf(stderr, "production rules: no tile with lumberjack yield 2\n");
      map_free(&map);
      return 1;
    }
    const int base = colony_yield_for_tile(&map, fx, fy, COLONIZE_JOB_LUMBERJACK);
    /* Convert whitelist (FUN_15eb_18ec ~11974-11979): Lumberjack is
     * excluded — no +1 here, unlike Farmer/Sugar/Tobacco/Cotton/Fur
     * Trapper/Fisherman below. */
    const int convert_yld =
      colony_yield_for_worker(&map, fx, fy, COLONIZE_JOB_LUMBERJACK, COLONIZE_PROF_CONVERT, true, 0, 0);
    if (convert_yld != base) {
      fprintf(
        stderr,
        "convert lumberjack (not whitelisted) want %d got %d\n",
        base,
        convert_yld
      );
      map_free(&map);
      return 1;
    }
    {
      int ffx = -1, ffy = -1;
      for (int y = 1; y < (int)map.height - 1 && ffx < 0; ++y) {
        for (int x = 1; x < (int)map.width - 1 && ffx < 0; ++x) {
          if (colony_yield_for_tile(&map, x, y, COLONIZE_JOB_FARMER) > 0) {
            ffx = x;
            ffy = y;
          }
        }
      }
      if (ffx < 0) {
        fprintf(stderr, "production rules: no tile with farmer yield\n");
        map_free(&map);
        return 1;
      }
      const int farmer_base = colony_yield_for_tile(&map, ffx, ffy, COLONIZE_JOB_FARMER);
      const int farmer_convert =
        colony_yield_for_worker(&map, ffx, ffy, COLONIZE_JOB_FARMER, COLONIZE_PROF_CONVERT, true, 0, 0);
      if (farmer_convert != farmer_base + 1) {
        fprintf(
          stderr,
          "convert farmer (whitelisted) want %d got %d\n",
          farmer_base + 1,
          farmer_convert
        );
        map_free(&map);
        return 1;
      }
    }
    /*
     * Resource effect table (FUN_15eb_17fa): a resource can pair with more
     * than one job (Game(9) -> Farmer +2 AND Fur Trapper +2). The old port
     * modeled resource->job as a single mapping (Game -> Fur Trapper only),
     * so Farmer on a Game tile got no bonus at all — not just a wrong
     * number, a whole matching case the old shape couldn't express.
     */
    {
      int gx = -1, gy = -1;
      for (int y = 1; y < (int)map.height - 1 && gx < 0; ++y) {
        for (int x = 1; x < (int)map.width - 1 && gx < 0; ++x) {
          if (map_resource_type_for_yield(&map, x, y) == 9 /* Game */) {
            gx = x;
            gy = y;
          }
        }
      }
      if (gx >= 0) {
        const int farmer_no_res = colony_yield_for_tile(&map, gx, gy, COLONIZE_JOB_FARMER);
        /* Base without the resource: same pedia, off-tile so no resource hits. */
        const int base_pedia = map_pedia_terrain_index_at(&map, gx, gy);
        int base_no_res = -1;
        for (int y = 1; y < (int)map.height - 1 && base_no_res < 0; ++y) {
          for (int x = 1; x < (int)map.width - 1 && base_no_res < 0; ++x) {
            if (map_pedia_terrain_index_at(&map, x, y) == base_pedia &&
                map_resource_type_for_yield(&map, x, y) < 0 &&
                !map_tile_has_road(&map, x, y) && !map_tile_has_river(&map, x, y)) {
              base_no_res = colony_yield_for_tile(&map, x, y, COLONIZE_JOB_FARMER);
            }
          }
        }
        if (base_no_res >= 0 && !map_tile_has_road(&map, gx, gy) &&
            !map_tile_has_river(&map, gx, gy) && farmer_no_res != base_no_res + 2) {
          fprintf(
            stderr,
            "Game+Farmer resource effect want %d got %d (base %d)\n",
            base_no_res + 2,
            farmer_no_res,
            base_no_res
          );
          map_free(&map);
          return 1;
        }
      }
    }
    const int wrong_expert =
      colony_yield_for_worker(&map, fx, fy, COLONIZE_JOB_LUMBERJACK, COLONIZE_PROF_FREE_COLONIST, true, 0, 0);
    if (wrong_expert != base) {
      fprintf(
        stderr,
        "wrong field expert should match free yield base=%d got=%d\n",
        base,
        wrong_expert
      );
      map_free(&map);
      return 1;
    }
    map_free(&map);

    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeMsgCatalog names;
    assets_msg_init(&names);
    if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") ||
        !colonies_load_buildings(&pool, &names)) {
      fprintf(stderr, "production rules: load buildings failed\n");
      assets_msg_free(&names);
      return 1;
    }
    const int distiller = colonies_find_building(&pool, "Rum Distiller's House");
    if (distiller < 0) {
      fprintf(stderr, "production rules: missing distillery\n");
      assets_msg_free(&names);
      return 1;
    }
    const char* dname = pool.building_types[distiller].name;
    if (colony_prod_manufacturing_output(dname, COLONIZE_PROF_CONVERT, COLONIZE_PROF_DISTILLER, 0) != 1 ||
        colony_prod_manufacturing_output(dname, COLONIZE_PROF_BLACKSMITH, COLONIZE_PROF_DISTILLER, 0) != 3 ||
        colony_prod_manufacturing_output(dname, COLONIZE_PROF_DISTILLER, COLONIZE_PROF_DISTILLER, 0) != 6) {
      fprintf(stderr, "manufacturing class/skill rules failed\n");
      assets_msg_free(&names);
      return 1;
    }

    /*
     * FUN_15eb_1d4c: sol_bonus folds in *before* tier/skill math, not as a
     * flat post-hoc add — matters at factory tier (×1.5 of the whole running
     * total, not just the class portion) and whenever skill matches (the
     * whole thing doubles, sol_bonus included). See
     * manufacturing_worker_calc_1d4c.md.
     */
    {
      const int iron_works = colonies_find_building(&pool, "Iron Works");
      if (iron_works < 0) {
        fprintf(stderr, "production rules: missing Iron Works\n");
        assets_msg_free(&names);
        return 1;
      }
      const char* iname = pool.building_types[iron_works].name;
      /* Free colonist (tag=3), factory tier, sol_bonus=2 (the real maximum —
       * the old ">=2 clamps to 2, +1 truncates to 0" step is gone; DOS folds
       * local_e directly): v = 3+2=5; shop/factory re-add tag: 5+3=8;
       * factory ×1.5 floor: 8+4=12. */
      const int unskilled =
        colony_prod_manufacturing_output(iname, COLONIZE_PROF_FREE_COLONIST, COLONIZE_PROF_BLACKSMITH, 2);
      if (unskilled != 12) {
        fprintf(stderr, "factory sol-fold unskilled want 12 got %d\n", unskilled);
        assets_msg_free(&names);
        return 1;
      }
      /* Skilled (Blacksmith in Iron Works): whole running total doubles: 12*2=24. */
      const int skilled =
        colony_prod_manufacturing_output(iname, COLONIZE_PROF_BLACKSMITH, COLONIZE_PROF_BLACKSMITH, 2);
      if (skilled != 24) {
        fprintf(stderr, "factory sol-fold skilled want 24 got %d\n", skilled);
        assets_msg_free(&names);
        return 1;
      }
      /*
       * Factory input, player-confirmed 2026-08-15 (Viceroy): Textile Mill,
       * free colonist, +2 sentiment — 12 cloth/turn output, 8 cotton/turn
       * consumed (colony-wide cotton accounting: 23 produced, 15 surplus, 8
       * consumed). Reusing Iron Works here (same tier/tag math, recipe-
       * independent) — free colonist, sol_bonus=2: v=3+2=5, +tag=8,
       * factory x1.5 floor=12 (matches the 12 cloth exactly). Input:
       * (12*6+8)/9=8, matching the observed 8 cotton exactly — settles the
       * long-open "does factory input discount 6-for-9, and does it track
       * the SoL-adjusted output or the flat base rate" question both ways:
       * yes to the discount, and it tracks the *actual* output (the old
       * `sol_bonus=0`-forced reading would have given 6, not 8).
       */
      const int factory_out_sol2 =
        colony_prod_manufacturing_output(iname, COLONIZE_PROF_FREE_COLONIST, COLONIZE_PROF_BLACKSMITH, 2);
      if (factory_out_sol2 != 12) {
        fprintf(stderr, "factory output sol=2 want 12 got %d\n", factory_out_sol2);
        assets_msg_free(&names);
        return 1;
      }
      const int factory_in_sol2 =
        colony_prod_manufacturing_input(iname, COLONIZE_PROF_FREE_COLONIST, COLONIZE_PROF_BLACKSMITH, 2);
      if (factory_in_sol2 != 8) {
        fprintf(stderr, "factory input sol=2 want 8 got %d\n", factory_in_sol2);
        assets_msg_free(&names);
        return 1;
      }
      /* Base rate (sol_bonus=0) input stays 6 — the old, still-correct half
       * of the ratio; only the sol-fold was missing before. */
      const int factory_in_base =
        colony_prod_manufacturing_input(iname, COLONIZE_PROF_FREE_COLONIST, COLONIZE_PROF_BLACKSMITH, 0);
      if (factory_in_base != 6) {
        fprintf(stderr, "factory input base want 6 got %d\n", factory_in_base);
        assets_msg_free(&names);
        return 1;
      }
      /* Tory penalty (negative sol_bonus) must reduce output, not get
       * clamped away — house tier, criminal (tag=1), sol_bonus=-5 clamps to 0. */
      const int penalized =
        colony_prod_manufacturing_output(dname, COLONIZE_PROF_CRIMINAL, COLONIZE_PROF_DISTILLER, -5);
      if (penalized != 0) {
        fprintf(stderr, "Tory-penalty clamp want 0 got %d\n", penalized);
        assets_msg_free(&names);
        return 1;
      }
      /* Same penalty, free colonist (tag=3): 3-5=-2 clamps to 0 too. */
      const int penalized2 =
        colony_prod_manufacturing_output(dname, COLONIZE_PROF_FREE_COLONIST, COLONIZE_PROF_DISTILLER, -5);
      if (penalized2 != 0) {
        fprintf(stderr, "Tory-penalty clamp (free) want 0 got %d\n", penalized2);
        assets_msg_free(&names);
        return 1;
      }
      /* A smaller penalty that doesn't clamp: free colonist, house tier, sol=-1 -> 3-1=2. */
      const int penalized3 =
        colony_prod_manufacturing_output(dname, COLONIZE_PROF_FREE_COLONIST, COLONIZE_PROF_DISTILLER, -1);
      if (penalized3 != 2) {
        fprintf(stderr, "Tory-penalty (unclamped) want 2 got %d\n", penalized3);
        assets_msg_free(&names);
        return 1;
      }
      /* colony_prod_bells_worker: sol_bonus folds in before skill doubling
       * (FUN_15eb_1d4c Statesman body) — unit-level check independent of the
       * nation-tick machinery above. Skilled: (tag+sol)*2; unskilled: tag+sol
       * only, no doubling. */
      const int bells_skilled =
        colony_prod_bells_worker("Town Hall", COLONIZE_PROF_STATESMAN, 2);
      if (bells_skilled != 10) { /* (3+2)*2 */
        fprintf(stderr, "bells_worker skilled sol-fold want 10 got %d\n", bells_skilled);
        assets_msg_free(&names);
        return 1;
      }
      const int bells_unskilled =
        colony_prod_bells_worker("Town Hall", COLONIZE_PROF_FREE_COLONIST, 2);
      if (bells_unskilled != 5) { /* 3+2, not doubled */
        fprintf(stderr, "bells_worker unskilled sol-fold want 5 got %d\n", bells_unskilled);
        assets_msg_free(&names);
        return 1;
      }
      /*
       * colony_prod_crosses_worker / colony_prod_hammers_worker: Carpenter/
       * Preacher DOS shape is `(skilled?6:tag)+sol_bonus`, *then* doubled by
       * a colony-wide "owns the upgrade" flag — not the class-scaled rate
       * table the port used to use, which only matched at sol_bonus=0 (see
       * manufacturing_worker_calc_1d4c.md). These four values only diverge
       * from the pre-fix numbers precisely when the colony owns the
       * upgrade, which is exactly the case being tested here.
       */
      const int crosses_unskilled_cathedral =
        colony_prod_crosses_worker("Cathedral", COLONIZE_PROF_FREE_COLONIST, 2, true, false);
      if (crosses_unskilled_cathedral != 10) { /* (3+2)*2, not the old 6+2=8 */
        fprintf(
          stderr,
          "crosses_worker unskilled+cathedral want 10 got %d\n",
          crosses_unskilled_cathedral
        );
        assets_msg_free(&names);
        return 1;
      }
      const int crosses_skilled_cathedral =
        colony_prod_crosses_worker("Cathedral", COLONIZE_PROF_PREACHER, 2, true, false);
      if (crosses_skilled_cathedral != 16) { /* (6+2)*2, not the old 6*2+2=14 */
        fprintf(
          stderr,
          "crosses_worker skilled+cathedral want 16 got %d\n",
          crosses_skilled_cathedral
        );
        assets_msg_free(&names);
        return 1;
      }
      const int hammers_unskilled =
        colony_prod_hammers_worker("Carpenter's Shop", UNITS_JOB_NONE, 0, false);
      const int hammers_skilled =
        colony_prod_hammers_worker("Carpenter's Shop", COLONIZE_PROF_CARPENTER, 0, false);
      const int hammers_unskilled_sol =
        colony_prod_hammers_worker("Carpenter's Shop", UNITS_JOB_NONE, 2, false);
      const int hammers_unskilled_mill =
        colony_prod_hammers_worker("Lumber Mill", UNITS_JOB_NONE, 2, true);
      if (hammers_unskilled_mill != 10) { /* (3+2)*2, not the old 6+2=8 */
        fprintf(
          stderr,
          "hammers_worker unskilled+mill want 10 got %d\n",
          hammers_unskilled_mill
        );
        assets_msg_free(&names);
        return 1;
      }
      const int hammers_skilled_mill =
        colony_prod_hammers_worker("Lumber Mill", COLONIZE_PROF_CARPENTER, 2, true);
      if (hammers_skilled_mill != 16) { /* (6+2)*2, not the old 6*2+2=14 */
        fprintf(
          stderr,
          "hammers_worker skilled+mill want 16 got %d\n",
          hammers_skilled_mill
        );
        assets_msg_free(&names);
        return 1;
      }
      /*
       * William Penn stacks with Cathedral per-worker (v *= 2 for Cathedral,
       * *then* v += v>>1 for Penn — DOS falls through from the Cathedral
       * branch into the Penn check unconditionally, not an else). Confirmed
       * by direct asm read of the Preacher body; see
       * manufacturing_worker_calc_1d4c.md. Distinguishes this from the old
       * (wrong) flat colony-total ×1.5, which this function never sees at
       * all — it can only be right if the stacking happens right here.
       */
      const int crosses_unskilled_cathedral_penn =
        colony_prod_crosses_worker("Cathedral", COLONIZE_PROF_FREE_COLONIST, 0, true, true);
      if (crosses_unskilled_cathedral_penn != 9) { /* (3*2)+((3*2)>>1) = 6+3 */
        fprintf(
          stderr,
          "crosses_worker unskilled+cathedral+penn want 9 got %d\n",
          crosses_unskilled_cathedral_penn
        );
        assets_msg_free(&names);
        return 1;
      }
      const int crosses_skilled_cathedral_penn =
        colony_prod_crosses_worker("Cathedral", COLONIZE_PROF_PREACHER, 0, true, true);
      if (crosses_skilled_cathedral_penn != 18) { /* (6*2)+((6*2)>>1) = 12+6 */
        fprintf(
          stderr,
          "crosses_worker skilled+cathedral+penn want 18 got %d\n",
          crosses_skilled_cathedral_penn
        );
        assets_msg_free(&names);
        return 1;
      }
    }

    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->building_in_production = -1;
    col->has_building[distiller] = true;
    col->stock[COLONIZE_CARGO_FOOD] = 20;
    col->stock[COLONIZE_CARGO_SUGAR] = 10;
    col->colonists[0].active = true;
    col->colonists[0].building_type = distiller;
    col->colonists[0].profession = COLONIZE_PROF_CONVERT;
    col->colonists[0].field_job = -1;
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      col->tiles[t] = -1;
    }
    col->colonist_count = 1;
    col->population = 1;
    pool.colony_count = 1;

    ColonizeTurnResult prod;
    ColonizeColonyProdDelta delta;
    memset(&prod, 0, sizeof(prod));
    turn_colony_free_production(&pool, col, NULL, &prod, &delta);
    if (col->stock[COLONIZE_CARGO_RUM] != 1 || col->stock[COLONIZE_CARGO_SUGAR] != 9) {
      fprintf(
        stderr,
        "convert rum craft failed sugar=%d rum=%d\n",
        col->stock[COLONIZE_CARGO_SUGAR],
        col->stock[COLONIZE_CARGO_RUM]
      );
      assets_msg_free(&names);
      return 1;
    }
    assets_msg_free(&names);
  }

  /* Field lumberjack harvests from forest surround tile. */
  {
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    char err[256];
    if (!map_load_mp("COLONIZE/AMER2.MP", &map, err, sizeof(err))) {
      fprintf(stderr, "map load for field test: %s\n", err);
      return 1;
    }
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeMsgCatalog names;
    assets_msg_init(&names);
    if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") ||
        !colonies_load_buildings(&pool, &names) || !colonies_load_names(&pool, "COLONIZE/COLONY.TXT")) {
      fprintf(stderr, "names/buildings for field test failed\n");
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    int fx = -1, fy = -1, ftile = -1, cx = -1, cy = -1;
    for (int y = 1; y < (int)map.height - 1 && fx < 0; ++y) {
      for (int x = 1; x < (int)map.width - 1 && fx < 0; ++x) {
        if (!map_tile_is_land(&map, x, y) || !colonies_can_found(&pool, &map, x, y)) {
          continue;
        }
        for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
          int dx = 0, dy = 0;
          colonies_field_tile_delta(ti, &dx, &dy);
          const int yld =
            colony_yield_for_tile(&map, x + dx, y + dy, COLONIZE_JOB_LUMBERJACK);
          if (yld > 0) {
            cx = x;
            cy = y;
            fx = x + dx;
            fy = y + dy;
            ftile = ti;
            break;
          }
        }
      }
    }
    if (ftile < 0) {
      fprintf(stderr, "no colony site with lumberjack yield nearby\n");
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    const int cid = colonies_found(&pool, &map, cx, cy, 0, 0, UNITS_JOB_NONE, 0, 0, 0);
    ColonizeColony* col = colonies_get_mut(&pool, cid);
    if (!col || !colonies_assign_field(&pool, cid, 0, ftile, COLONIZE_JOB_LUMBERJACK)) {
      fprintf(stderr, "assign lumberjack failed at (%d,%d) tile %d\n", fx, fy, ftile);
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    /* Isolate field harvest from carpenter hammers on default Stockade project.
     * This colony has no Farmer, only the Lumberjack under test, so give it a
     * food buffer up front — otherwise Phase J's starve-kill (still short of
     * `pop*2` after the turn, and food was 0 at turn start) removes the
     * colony's only colonist on the very first turn_colony_free_production
     * call below, before either check in this block ever runs. */
    col->building_in_production = -1;
    col->stock[COLONIZE_CARGO_FOOD] = 100;
    const int before = col->stock[COLONIZE_CARGO_LUMBER];
    const int expect =
      colony_yield_for_worker(&map, fx, fy, COLONIZE_JOB_LUMBERJACK, col->colonists[0].profession, true, 0, 0);
    ColonizeTurnResult prod;
    ColonizeColonyProdDelta delta;
    memset(&prod, 0, sizeof(prod));
    turn_colony_free_production(&pool, col, &map, &prod, &delta);
    /* No carpenter assigned → hammers stay 0 (shop alone does not produce). */
    if (delta.lumber < expect) {
      fprintf(
        stderr,
        "field lumber delta too low got %d expect %d (stock %d->%d)\n",
        delta.lumber,
        expect,
        before,
        col->stock[COLONIZE_CARGO_LUMBER]
      );
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }

    /* Tory penalty must reduce the Production tab's field-yield preview too —
     * colony_preview.c's field loop had the same `sol_b > 0` guard bug as
     * bells/hammers, dropping every Tory penalty instead of applying it. */
    col->population = 15; /* tories=15, thresh=10 (col1 NULL) -> mod=-1 */
    const int base_yield =
      colony_yield_for_worker(&map, fx, fy, COLONIZE_JOB_LUMBERJACK, col->colonists[0].profession, true, 0, 0);
    ColonizeColonyPreview prev;
    colony_preview_compute(&pool, col, &map, NULL, &prev);
    if (prev.goods[COLONIZE_CARGO_LUMBER] != base_yield - 1) {
      fprintf(
        stderr,
        "Tory-penalty field preview want %d got %d (base_yield=%d)\n",
        base_yield - 1,
        prev.goods[COLONIZE_CARGO_LUMBER],
        base_yield
      );
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }

    assets_msg_free(&names);
    map_free(&map);
  }

  /*
   * Henry Hudson: fur trapper field output +100% (turn_produce_one_colony).
   * Preview must match — colony_preview.c had been missing this doubling.
   */
  {
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    char err[256];
    if (!map_load_mp("COLONIZE/AMER2.MP", &map, err, sizeof(err))) {
      fprintf(stderr, "map load for Hudson test: %s\n", err);
      return 1;
    }
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeMsgCatalog names;
    assets_msg_init(&names);
    if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") ||
        !colonies_load_buildings(&pool, &names) || !colonies_load_names(&pool, "COLONIZE/COLONY.TXT")) {
      fprintf(stderr, "names/buildings for Hudson test failed\n");
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    int fx = -1, fy = -1, ftile = -1, cx = -1, cy = -1;
    for (int y = 1; y < (int)map.height - 1 && fx < 0; ++y) {
      for (int x = 1; x < (int)map.width - 1 && fx < 0; ++x) {
        if (!map_tile_is_land(&map, x, y) || !colonies_can_found(&pool, &map, x, y)) {
          continue;
        }
        for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
          int dx = 0, dy = 0;
          colonies_field_tile_delta(ti, &dx, &dy);
          const int yld =
            colony_yield_for_tile(&map, x + dx, y + dy, COLONIZE_JOB_FUR_TRAPPER);
          if (yld > 0) {
            cx = x;
            cy = y;
            fx = x + dx;
            fy = y + dy;
            ftile = ti;
            break;
          }
        }
      }
    }
    if (ftile < 0) {
      fprintf(stderr, "no colony site with fur trapper yield nearby\n");
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    const int cid = colonies_found(&pool, &map, cx, cy, 0, 0, UNITS_JOB_NONE, 0, 0, 0);
    ColonizeColony* col = colonies_get_mut(&pool, cid);
    if (!col || !colonies_assign_field(&pool, cid, 0, ftile, COLONIZE_JOB_FUR_TRAPPER)) {
      fprintf(stderr, "assign fur trapper failed at (%d,%d) tile %d\n", fx, fy, ftile);
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    col->building_in_production = -1;
    col->nation_id = 0;
    /* No Farmer, only the Fur Trapper under test — seed a food buffer so
     * Phase J's starve-kill doesn't remove the colony's only colonist on
     * this first simulated turn (town-commons food alone nets exactly 0
     * against pop*2 consumption for a fresh 1-colonist colony). */
    col->stock[COLONIZE_CARGO_FOOD] = 100;

    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.player[0].control = 0;
    for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
      col1.head.founding_father[i] = -1;
    }

    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, &map, &col1, NULL, -1, &prod, NULL, NULL, NULL);
    const int base_furs = col->stock[COLONIZE_CARGO_FURS];
    if (base_furs <= 0) {
      fprintf(stderr, "Hudson test base fur harvest want >0 got %d\n", base_furs);
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }

    /* Grant Hudson, redo the same tick from a clean stock, expect exactly 2x. */
    col->stock[COLONIZE_CARGO_FURS] = 0;
    col1.head.founding_father[FF_HENRY_HUDSON] = 0; /* nation 0 owns it */
    ColonizeColonyPreview prev;
    colony_preview_compute(&pool, col, &map, &col1, &prev);
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, &map, &col1, NULL, -1, &prod, NULL, NULL, NULL);
    const int hudson_furs = col->stock[COLONIZE_CARGO_FURS];
    if (hudson_furs != base_furs * 2) {
      fprintf(
        stderr,
        "Hudson fur doubling want %d got %d\n",
        base_furs * 2,
        hudson_furs
      );
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    if (prev.goods[COLONIZE_CARGO_FURS] != hudson_furs) {
      fprintf(
        stderr,
        "Hudson fur preview mismatch want %d got %d\n",
        hudson_furs,
        prev.goods[COLONIZE_CARGO_FURS]
      );
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    assets_msg_free(&names);
    map_free(&map);
  }

  /*
   * Hammers bank even with no construction queued (turn.c "TURN5→6" comment,
   * colony_prod_colony_hammers). Preview had been hiding this row entirely
   * whenever building_in_production < 0.
   */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Carpenter's Shop");
    pool.building_type_count = 1;

    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->nation_id = 0;
    col->building_in_production = -1; /* no project selected */
    col->stock[COLONIZE_CARGO_FOOD] = 100; /* avoid starve-kill wiping the colony */
    col->stock[COLONIZE_CARGO_LUMBER] = 10;
    col->colonists[0].active = true;
    col->colonists[0].building_type = 0;
    col->colonists[0].profession = COLONIZE_PROF_FREE_COLONIST;
    col->colonists[0].field_job = -1;
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      col->tiles[t] = -1;
    }
    col->colonist_count = 1;
    col->population = 1;
    pool.colony_count = 1;

    ColonizeColonyPreview prev;
    colony_preview_compute(&pool, col, NULL, NULL, &prev);
    if (prev.hammers != 3) {
      fprintf(stderr, "no-project hammers preview want 3 got %d\n", prev.hammers);
      return 1;
    }

    ColonizeTurnResult prod;
    ColonizeColonyProdDelta delta;
    memset(&prod, 0, sizeof(prod));
    memset(&delta, 0, sizeof(delta));
    turn_colony_free_production(&pool, col, NULL, &prod, &delta);
    if (col->hammers != 3 || delta.hammers_added != 3 ||
        col->stock[COLONIZE_CARGO_LUMBER] != 7) {
      fprintf(
        stderr,
        "no-project hammers actual want hammers=3 delta=3 lumber=7 got %d/%d/%d\n",
        col->hammers,
        delta.hammers_added,
        col->stock[COLONIZE_CARGO_LUMBER]
      );
      return 1;
    }
  }

  /*
   * Tory penalty must reduce banked hammers too, not get silently dropped
   * (same `sol_b > 0` guard bug as bells above, now fixed). Lumber stock
   * must cover the sol-adjusted output — hammers cost lumber 1:1, capped by
   * what was on hand at the start of the turn (2026-08-16 real-DOS fix: a
   * carpenter with 0 lumber on hand now correctly bags 0 hammers, not the
   * sol-adjusted value for free — see turn.c's Carpenter hammers block).
   */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Carpenter's Shop");
    pool.building_type_count = 1;

    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->nation_id = 0;
    col->building_in_production = -1;
    col->stock[COLONIZE_CARGO_FOOD] = 100;
    col->stock[COLONIZE_CARGO_LUMBER] = 100;
    col->colonists[0].active = true;
    col->colonists[0].building_type = 0;
    col->colonists[0].profession = COLONIZE_PROF_FREE_COLONIST;
    col->colonists[0].field_job = -1;
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      col->tiles[t] = -1;
    }
    col->colonist_count = 1;
    col->population = 15; /* tories=15, thresh=10 (col1 NULL -> default) -> mod=-1 */
    pool.colony_count = 1;

    ColonizeColonyPreview prev;
    colony_preview_compute(&pool, col, NULL, NULL, &prev);
    if (prev.hammers != 2) {
      fprintf(stderr, "Tory-penalty hammers preview want 2 got %d\n", prev.hammers);
      return 1;
    }

    ColonizeTurnResult prod;
    ColonizeColonyProdDelta delta;
    memset(&prod, 0, sizeof(prod));
    memset(&delta, 0, sizeof(delta));
    turn_colony_free_production(&pool, col, NULL, &prod, &delta);
    if (col->hammers != 2 || delta.hammers_added != 2) {
      fprintf(
        stderr,
        "Tory-penalty hammers actual want 2 got hammers=%d delta=%d\n",
        col->hammers,
        delta.hammers_added
      );
      return 1;
    }
    fprintf(stderr, "Tory penalty reduces hammers ok\n");
  }

  /*
   * Fisherman needs Docks (FUN_15eb_18ec ~11925-11939): yields 0 without it,
   * regardless of what the tile table says. colony_yield_for_worker's
   * has_docks parameter must actually gate this, not just default to
   * "allowed" everywhere.
   */
  {
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    char err[256];
    if (!map_load_mp("COLONIZE/AMER2.MP", &map, err, sizeof(err))) {
      fprintf(stderr, "map load for docks-gate test: %s\n", err);
      return 1;
    }
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeMsgCatalog names;
    assets_msg_init(&names);
    if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") ||
        !colonies_load_buildings(&pool, &names) || !colonies_load_names(&pool, "COLONIZE/COLONY.TXT")) {
      fprintf(stderr, "names/buildings for docks-gate test failed\n");
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    int fx = -1, fy = -1, ftile = -1, cx = -1, cy = -1;
    for (int y = 1; y < (int)map.height - 1 && fx < 0; ++y) {
      for (int x = 1; x < (int)map.width - 1 && fx < 0; ++x) {
        if (!map_tile_is_land(&map, x, y) || !colonies_can_found(&pool, &map, x, y)) {
          continue;
        }
        for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
          int dx = 0, dy = 0;
          colonies_field_tile_delta(ti, &dx, &dy);
          const int yld =
            colony_yield_for_tile(&map, x + dx, y + dy, COLONIZE_JOB_FISHERMAN);
          if (yld > 0) {
            cx = x;
            cy = y;
            fx = x + dx;
            fy = y + dy;
            ftile = ti;
            break;
          }
        }
      }
    }
    if (ftile < 0) {
      fprintf(stderr, "no colony site with fisherman yield nearby\n");
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    const int cid = colonies_found(&pool, &map, cx, cy, 0, 0, UNITS_JOB_NONE, 0, 0, 0);
    ColonizeColony* col = colonies_get_mut(&pool, cid);
    if (!col || !colonies_assign_field(&pool, cid, 0, ftile, COLONIZE_JOB_FISHERMAN)) {
      fprintf(stderr, "assign fisherman failed at (%d,%d) tile %d\n", fx, fy, ftile);
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    const int docks = colonies_find_building(&pool, "Docks");
    if (docks < 0) {
      fprintf(stderr, "docks-gate test: missing Docks building type\n");
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    col->has_building[docks] = false;
    const int no_docks_yld = colony_yield_for_worker(
      &map, fx, fy, COLONIZE_JOB_FISHERMAN, col->colonists[0].profession, false, 0, 0
    );
    if (no_docks_yld != 0) {
      fprintf(stderr, "fisherman without Docks want 0 got %d\n", no_docks_yld);
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    const int with_docks_yld = colony_yield_for_worker(
      &map, fx, fy, COLONIZE_JOB_FISHERMAN, col->colonists[0].profession, true, 0, 0
    );
    if (with_docks_yld <= 0) {
      fprintf(stderr, "fisherman with Docks want >0 got %d\n", with_docks_yld);
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    assets_msg_free(&names);
    map_free(&map);
    fprintf(stderr, "fisherman Docks gate ok\n");
  }

  /*
   * Church and Cathedral passive crosses are the *same* (+1 each, on top of
   * the colony base +1) in DOS (FUN_15eb_1f72 ~11306-11314: unconditional
   * +1, then +1 independently if Church built, +1 independently if
   * Cathedral built) — not the manual/wiki-sourced +2/+3 this used to
   * return. No existing test exercised Cathedral specifically to catch a
   * regression back to the old split.
   */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Church");
    snprintf(pool.building_types[1].name, sizeof(pool.building_types[1].name), "Cathedral");
    pool.building_type_count = 2;

    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->nation_id = 0;
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      col->tiles[t] = -1;
    }
    pool.colony_count = 1;

    col->has_building[0] = true; /* Church */
    const int church_crosses = colony_prod_colony_crosses(&pool, col);
    col->has_building[0] = false;
    col->has_building[1] = true; /* Cathedral */
    const int cathedral_crosses = colony_prod_colony_crosses(&pool, col);
    if (church_crosses != 2 || cathedral_crosses != 2) {
      fprintf(
        stderr,
        "Church/Cathedral passive parity want 2/2 got %d/%d\n",
        church_crosses,
        cathedral_crosses
      );
      return 1;
    }
    fprintf(stderr, "Church/Cathedral passive parity ok\n");
  }

  /*
   * Real DOS gives expert Farmer/Fisherman a flat +2 on skill match, not
   * ×2 like every other field expert, plus the colony's SoL latch bits
   * re-added a second time (FUN_15eb_18ec ~11890-11899, asm-confirmed —
   * see docs/terrain_yields.md "Field Farmer/Fisherman expert formula").
   * Wired 2026-08-18, player-confirmed against four real,
   * un-synthesized golden_colony_prod02 town-commons-food values (which
   * pinned the sibling formula first) plus Fort Orange's real expert
   * Farmer (Savannah, no resource: base 3 + sol fold 2 + flat 2 + latch
   * re-add 2 = 9, not (3+2)×2 = 10) and New Amsterdam's real expert
   * Fisherman + Fishery resource (needs the same shape plus its own
   * doubled resource).
   *
   * `base` (free colonist, non-expert) includes the unconditional Farmer
   * +1 (colony_yield_pipeline) plus a possible river +1 — neither applies
   * to the expert path (skips this block entirely), so back both out to
   * get the raw table value the expert path's flat +2 applies to. No
   * colony context here (real map, no colony), so colony_flags=0 → no
   * latch re-add.
   */
  {
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    char err[256];
    if (!map_load_mp("COLONIZE/AMER2.MP", &map, err, sizeof(err))) {
      fprintf(stderr, "map load for expert food/fish test: %s\n", err);
      return 1;
    }
    int fx = -1, fy = -1;
    for (int y = 1; y < (int)map.height - 1 && fx < 0; ++y) {
      for (int x = 1; x < (int)map.width - 1 && fx < 0; ++x) {
        if (colony_yield_for_tile(&map, x, y, COLONIZE_JOB_FARMER) > 0) {
          fx = x;
          fy = y;
        }
      }
    }
    if (fx < 0) {
      fprintf(stderr, "no tile with farmer yield for expert test\n");
      map_free(&map);
      return 1;
    }
    const int base = colony_yield_for_tile(&map, fx, fy, COLONIZE_JOB_FARMER);
    const int expert_yld =
      colony_yield_for_worker(&map, fx, fy, COLONIZE_JOB_FARMER, COLONIZE_JOB_FARMER, true, 0, 0);
    /* 2026-09-03: the improvement stack (farmer +1, plow, river) applies to
     * expert and non-expert alike (asm 15eb:1c32-1c9c is skill-blind except
     * for u sizing, and u=1 for food jobs), so the expert delta over the
     * profession-less tile yield is exactly the flat +2 — golden_colony_
     * prod03's case3 (forest+Game 8, bare hill 4) pinned this. */
    const int want = base + 2;
    if (expert_yld != want) {
      fprintf(
        stderr,
        "expert farmer flat+2 want %d got %d (base %d)\n",
        want,
        expert_yld,
        base
      );
      map_free(&map);
      return 1;
    }
    map_free(&map);
    fprintf(stderr, "expert farmer flat+2 ok\n");
  }

  /*
   * Fisherman distance/enclosure modifier (FUN_15eb_18ec ~11814-11838):
   * open-ocean tiles (all 8 neighbors Ocean/Sea Lane) get -2; a sheltered
   * tile (few/no ocean neighbors) gets +1. Never ported before — new
   * mechanic found this pass, not a divergence-fix.
   */
  {
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    char err[64];
    if (!map_alloc(&map, 5, 5, err, sizeof(err))) {
      fprintf(stderr, "fisherman distance mod: map_alloc %s\n", err);
      return 1;
    }
    for (int i = 0; i < 25; ++i) {
      map.terrain[i] = 2; /* plains everywhere */
    }
    map.terrain[2 * 5 + 2] = 25; /* ocean center tile being fished */
    const int sheltered = colony_yield_for_tile(&map, 2, 2, COLONIZE_JOB_FISHERMAN);
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) {
          continue;
        }
        map.terrain[(2 + dy) * 5 + (2 + dx)] = 25; /* surround with open ocean too */
      }
    }
    const int open_ocean = colony_yield_for_tile(&map, 2, 2, COLONIZE_JOB_FISHERMAN);
    map_free(&map);
    if (sheltered != open_ocean + 3) {
      fprintf(
        stderr,
        "fisherman distance mod want sheltered=open_ocean+3 got sheltered=%d open_ocean=%d\n",
        sheltered,
        open_ocean
      );
      return 1;
    }
    fprintf(stderr, "fisherman distance mod ok\n");
  }

  /*
   * Field yields zero the SoL/Tory mod outright for AI-controlled colonies
   * (FUN_15eb_18ec); manufacturing/bells/crosses/hammers (FUN_15eb_1d4c)
   * only change the divisor, never zero it — colony_prod_sol_bonus_field
   * vs. the shared colony_prod_sol_bonus must actually differ for AI.
   */
  {
    ColonizeColony col;
    memset(&col, 0, sizeof(col));
    col.active = true;
    col.nation_id = 1;
    col.population = 15; /* tories=(15*100+50)/100=15; thresh=10 -> mod=-1 */

    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.player[1].control = 1; /* AI */

    const int building_mod = colony_prod_sol_bonus(&col1, &col);
    const int field_mod = colony_prod_sol_bonus_field(&col1, &col);
    if (building_mod != -1 || field_mod != 0) {
      fprintf(
        stderr,
        "AI field-vs-building SoL mod want building=-1 field=0 got building=%d field=%d\n",
        building_mod,
        field_mod
      );
      return 1;
    }
    fprintf(stderr, "AI field SoL zero-out ok\n");
  }

  /*
   * Custom House auto-sell (FUN_364b_0688 / FUN_364b_0636): stock>99 → leave 50;
   * Food denied; boycott bypass; tax then WoI untaxed.
   */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    snprintf(
      pool.building_types[0].name, sizeof(pool.building_types[0].name), "Custom House"
    );
    pool.building_type_count = 1;

    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->nation_id = 0;
    col->has_building[0] = true;
    col->building_in_production = -1;
    /* bits==0 means "nothing configured" (per-cargo UI PARKED) → sells
     * nothing; player-confirmed 2026-08-16 against a real DOS save
     * (colony-prod-tests). Enable Tobacco explicitly to exercise the sell
     * math below; the bits==0 no-op case is its own check further down. */
    col->custom_house_bits = (uint16_t)(1u << COLONIZE_CARGO_TOBACCO);
    col->stock[COLONIZE_CARGO_TOBACCO] = 120;
    col->stock[COLONIZE_CARGO_FOOD] = 200;

    EuropeScreen eu;
    memset(&eu, 0, sizeof(eu));
    eu.cargo_count = COLONIZE_CARGO_COUNT;
    for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
      eu.cargo[i].bid = 10;
    }
    eu.gold = 0;
    eu.tax_percent = 20;

    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.nation[0].boycott_bitmap = (uint16_t)(1u << COLONIZE_CARGO_TOBACCO);
    col1.nation[0].tax_rate = 20; /* 1dfa ledger reads the seller's nation tax */

    const int gained = europe_custom_house_autosell(&eu, &pool, col, &col1, 0);
    /* Sells at euro_price − 1 = 9: gross 630, tax 20% = 126 → 504
     * (FUN_364b_0688 rounding: gross − gross·tax/100); boycott bit ignored;
     * tax goes to royal_money. */
    if (col->stock[COLONIZE_CARGO_TOBACCO] != 50 || col->stock[COLONIZE_CARGO_FOOD] != 200) {
      fprintf(
        stderr,
        "custom house stock tobacco=%d food=%d (want 50/200)\n",
        col->stock[COLONIZE_CARGO_TOBACCO],
        col->stock[COLONIZE_CARGO_FOOD]
      );
      return 1;
    }
    if (gained != 504 || eu.gold != 504 || col1.nation[0].gold != 504u ||
        col1.nation[0].royal_money != 126 || col1.nation[0].trade.tons[COLONIZE_CARGO_TOBACCO] != 70 ||
        col1.nation[0].trade.tons2[COLONIZE_CARGO_TOBACCO] != 70 ||
        col1.nation[0].trade.gold[COLONIZE_CARGO_TOBACCO] != 504) {
      fprintf(
        stderr,
        "custom house gold gained=%d eu=%d nat=%u royal=%d (want 504/126)\n",
        gained,
        eu.gold,
        (unsigned)col1.nation[0].gold,
        (int)col1.nation[0].royal_money
      );
      return 1;
    }

    /* Blockade: enemy armed ship next to the colony shuts the Custom House
     * (FUN_364b_0688 colony +0x1b & 3). */
    col->stock[COLONIZE_CARGO_TOBACCO] = 120;
    col->ai_flags = 0x01;
    if (europe_custom_house_autosell(&eu, &pool, col, &col1, 0) != 0 ||
        col->stock[COLONIZE_CARGO_TOBACCO] != 120) {
      fprintf(stderr, "custom house should be blockaded\n");
      return 1;
    }
    col->ai_flags = 0;

    col->stock[COLONIZE_CARGO_TOBACCO] = 120;
    eu.gold = 0;
    col1.nation[0].gold = 0;
    col1.head.game_options.woi = 1; /* WoI — tax 0 */
    const int gained_woi = europe_custom_house_autosell(&eu, &pool, col, &col1, 0);
    if (gained_woi != 630 || eu.gold != 630) {
      fprintf(stderr, "custom house WoI gained=%d eu=%d (want 630)\n", gained_woi, eu.gold);
      return 1;
    }

    /* Mask bit off → no sell for that cargo. */
    col->stock[COLONIZE_CARGO_TOBACCO] = 120;
    col->custom_house_bits = (uint16_t)(1u << COLONIZE_CARGO_SUGAR); /* tobacco off */
    eu.gold = 0;
    if (europe_custom_house_autosell(&eu, &pool, col, &col1, 0) != 0 ||
        col->stock[COLONIZE_CARGO_TOBACCO] != 120) {
      fprintf(stderr, "custom house mask should skip tobacco\n");
      return 1;
    }

    /* bits==0 (nothing configured yet) → sells nothing, not "everything".
     * Player-confirmed 2026-08-16: real DOS save with Custom House built,
     * custom_house_bits==0, sold nothing that turn. */
    col->stock[COLONIZE_CARGO_TOBACCO] = 120;
    col->custom_house_bits = 0;
    eu.gold = 0;
    if (europe_custom_house_autosell(&eu, &pool, col, &col1, 0) != 0 ||
        col->stock[COLONIZE_CARGO_TOBACCO] != 120) {
      fprintf(stderr, "custom house bits==0 should sell nothing\n");
      return 1;
    }

    /* turn_run_colony_production wires autosell. */
    col->custom_house_bits = (uint16_t)(1u << COLONIZE_CARGO_TOBACCO);
    col->stock[COLONIZE_CARGO_TOBACCO] = 120;
    col->colonists[0].active = true;
    col->colonist_count = 1;
    col->population = 1;
    col->stock[COLONIZE_CARGO_FOOD] = 10; /* eat 2 */
    eu.gold = 0;
    col1.head.game_options.woi = 0;
    col1.nation[0].gold = 0;
    eu.tax_percent = 0;
    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, &col1, &eu, 0, &prod, NULL, NULL, NULL);
    if (col->stock[COLONIZE_CARGO_TOBACCO] != 50 || eu.gold != 630) {
      fprintf(
        stderr,
        "produce+CH tobacco=%d gold=%d (want 50/630)\n",
        col->stock[COLONIZE_CARGO_TOBACCO],
        eu.gold
      );
      return 1;
    }
    fprintf(stderr, "custom house autosell ok\n");
  }

  /*
   * Col1 +0x97 depletion_counter: ore/silver field work INC; wrap at 50 sets
   * MAP_LAYER2_SUPPRESS on the worked tile (FUN_364b_033a feature 4).
   */
  {
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    char err[256];
    if (!map_load_mp("COLONIZE/AMER2.MP", &map, err, sizeof(err))) {
      fprintf(stderr, "depletion: map load failed: %s\n", err);
      return 1;
    }
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeMsgCatalog names;
    assets_msg_init(&names);
    if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") ||
        !colonies_load_buildings(&pool, &names) ||
        !colonies_load_names(&pool, "COLONIZE/COLONY.TXT")) {
      fprintf(stderr, "depletion: names/buildings failed\n");
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    int cx = -1, cy = -1, fx = -1, fy = -1, ftile = -1;
    for (int y = 1; y < (int)map.height - 1 && ftile < 0; ++y) {
      for (int x = 1; x < (int)map.width - 1 && ftile < 0; ++x) {
        if (!map_tile_is_land(&map, x, y) || !colonies_can_found(&pool, &map, x, y)) {
          continue;
        }
        for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
          int dx = 0, dy = 0;
          colonies_field_tile_delta(ti, &dx, &dy);
          const int yld =
            colony_yield_for_tile(&map, x + dx, y + dy, COLONIZE_JOB_ORE_MINER);
          /*
           * depletion_counter only tracks a special-resource deposit being
           * mined down (2026-08-16 real-DOS fix), not any ore-yielding
           * tile — the site must actually carry the bonus resource.
           */
          if (yld > 0 && map_resource_type_for_yield(&map, x + dx, y + dy) == 6) {
            cx = x;
            cy = y;
            fx = x + dx;
            fy = y + dy;
            ftile = ti;
            break;
          }
        }
      }
    }
    if (ftile < 0) {
      fprintf(stderr, "depletion: no ore-miner field site\n");
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    const int cid = colonies_found(&pool, &map, cx, cy, 0, 0, UNITS_JOB_NONE, 0, 0, 0);
    ColonizeColony* col = colonies_get_mut(&pool, cid);
    if (!col || !colonies_assign_field(&pool, cid, 0, ftile, COLONIZE_JOB_ORE_MINER)) {
      fprintf(stderr, "depletion: assign ore miner failed\n");
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    col->building_in_production = -1;
    col->stock[COLONIZE_CARGO_FOOD] = 100;
    col->depletion_counter = 0x31; /* one INC wraps */
    snprintf(col->name, sizeof(col->name), "Potosi");

    EuropeScreen eu;
    memset(&eu, 0, sizeof(eu));
    AiPopupState pops;
    ai_popup_init(&pops);
    ColonizeMsgCatalog game_txt;
    assets_msg_init(&game_txt);
    (void)assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT");

    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, &map, NULL, &eu, 0, &prod, &pops, &game_txt, NULL);
    if (col->depletion_counter != 0) {
      fprintf(
        stderr,
        "depletion_counter wrap got %u want 0\n",
        (unsigned)col->depletion_counter
      );
      assets_msg_free(&game_txt);
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    const uint8_t after_l2 =
      map.layer2 ? map.layer2[fy * map.width + fx] : 0;
    if ((after_l2 & MAP_LAYER2_SUPPRESS) == 0) {
      fprintf(
        stderr,
        "depletion wrap did not set LAYER2_SUPPRESS at (%d,%d) after=%02x\n",
        fx,
        fy,
        after_l2
      );
      assets_msg_free(&game_txt);
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    if (col->stock[COLONIZE_CARGO_ORE] <= 0 && col->stock[COLONIZE_CARGO_SILVER] <= 0) {
      fprintf(stderr, "depletion: expected ore/silver yield in stock\n");
      assets_msg_free(&game_txt);
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    if (strstr(eu.status, "depleted") == NULL && pops.queue_count < 1) {
      fprintf(stderr, "depletion: want status/popup got '%s' q=%d\n", eu.status, pops.queue_count);
      assets_msg_free(&game_txt);
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    if (pops.queue_count >= 1 &&
        strstr(pops.queue[0].body, "depleted") == NULL &&
        strstr(pops.queue[0].body, "Potosi") == NULL) {
      fprintf(stderr, "depletion: popup body weak: '%s'\n", pops.queue[0].body);
      assets_msg_free(&game_txt);
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    fprintf(stderr, "depletion_counter wrap+suppress ok\n");
    assets_msg_free(&game_txt);
    assets_msg_free(&names);
    map_free(&map);
  }

  /* FUN_364b_0688 birth: food≥200 after eat → Free Colonist, −200 food. */
  {
    ColonizeColonyPool birth_pool;
    colonies_init(&birth_pool);
    ColonizeColony* b = &birth_pool.colonies[0];
    memset(b, 0, sizeof(*b));
    b->active = true;
    b->id = 1;
    b->nation_id = 0;
    b->building_in_production = -1;
    snprintf(b->name, sizeof(b->name), "Plymouth");
    b->stock[COLONIZE_CARGO_FOOD] = 250; /* eat 2 → 248 → birth −200 → 48 */
    b->colonists[0].active = true;
    b->colonists[0].unit_type_index = 0;
    b->colonists[0].profession = UNITS_JOB_NONE;
    b->colonists[0].building_type = -1;
    b->colonists[0].field_job = -1;
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      b->tiles[t] = -1;
    }
    b->colonist_count = 1;
    b->population = 1;
    birth_pool.colony_count = 1;

    EuropeScreen eu;
    memset(&eu, 0, sizeof(eu));
    AiPopupState pops;
    ai_popup_init(&pops);
    ColonizeMsgCatalog game_txt;
    assets_msg_init(&game_txt);
    (void)assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT");

    ColonizeTurnResult br;
    memset(&br, 0, sizeof(br));
    turn_run_colony_production(&birth_pool, NULL, NULL, &eu, 0, &br, &pops, &game_txt, NULL);
    if (!b->active || b->colonist_count != 2) {
      fprintf(stderr, "birth: colonist_count want 2 got %d\n", b->colonist_count);
      assets_msg_free(&game_txt);
      return 1;
    }
    if (b->stock[COLONIZE_CARGO_FOOD] != 48) {
      fprintf(stderr, "birth: food want 48 got %d\n", b->stock[COLONIZE_CARGO_FOOD]);
      assets_msg_free(&game_txt);
      return 1;
    }
    if (b->colonists[1].profession != UNITS_JOB_COLONIST) {
      fprintf(stderr, "birth: newborn should be Free Colonist job\n");
      assets_msg_free(&game_txt);
      return 1;
    }
    if (strstr(eu.status, "Birth") == NULL && strstr(eu.status, "Plymouth") == NULL) {
      fprintf(stderr, "birth: status want Birth/Plymouth got '%s'\n", eu.status);
      assets_msg_free(&game_txt);
      return 1;
    }
    if (pops.queue_count < 1) {
      fprintf(stderr, "birth: expected NEWCOLONIST popup\n");
      assets_msg_free(&game_txt);
      return 1;
    }
    if (strstr(pops.queue[0].body, "Population increase") == NULL &&
        strstr(pops.queue[0].body, "Plymouth") == NULL &&
        strstr(pops.queue[0].body, "Birth") == NULL) {
      fprintf(stderr, "birth: popup body weak: '%s'\n", pops.queue[0].body);
      assets_msg_free(&game_txt);
      return 1;
    }
    assets_msg_free(&game_txt);
    fprintf(stderr, "colony birth food≥200 ok\n");
  }

  /*
   * FUN_364b_0688 Phase B: AI Euro food += difficulty>>1.
   * Cite: colony_eot_production.md; difficulty.md.
   */
  {
    ColonizeColonyPool ai_pool;
    colonies_init(&ai_pool);
    ColonizeColony* a = &ai_pool.colonies[0];
    memset(a, 0, sizeof(*a));
    a->active = true;
    a->id = 1;
    a->nation_id = 1;
    a->building_in_production = -1;
    a->stock[COLONIZE_CARGO_FOOD] = 10;
    a->colonists[0].active = true;
    a->colonists[0].unit_type_index = 0;
    a->colonists[0].building_type = -1;
    a->colonists[0].field_job = -1;
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      a->tiles[t] = -1;
    }
    a->colonist_count = 1;
    a->population = 1;
    ai_pool.colony_count = 1;

    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.player[0].control = 0;
    col1.player[1].control = 1; /* AI */
    col1.head.difficulty = 4; /* Viceroy → +2 */

    ColonizeTurnResult ar;
    memset(&ar, 0, sizeof(ar));
    turn_run_colony_production(&ai_pool, NULL, &col1, NULL, 0, &ar, NULL, NULL, NULL);
    /* 10 + 2 AI food − 2 eat = 10 */
    if (a->stock[COLONIZE_CARGO_FOOD] != 10) {
      fprintf(
        stderr,
        "AI food bonus Viceroy: want 10 got %d\n",
        a->stock[COLONIZE_CARGO_FOOD]
      );
      return 1;
    }

    a->stock[COLONIZE_CARGO_FOOD] = 10;
    col1.head.difficulty = 0; /* Discoverer → +0 */
    turn_run_colony_production(&ai_pool, NULL, &col1, NULL, 0, &ar, NULL, NULL, NULL);
    if (a->stock[COLONIZE_CARGO_FOOD] != 8) {
      fprintf(
        stderr,
        "AI food bonus Discoverer: want 8 got %d\n",
        a->stock[COLONIZE_CARGO_FOOD]
      );
      return 1;
    }

    a->stock[COLONIZE_CARGO_FOOD] = 10;
    a->nation_id = 0; /* human */
    col1.head.difficulty = 4;
    turn_run_colony_production(&ai_pool, NULL, &col1, NULL, 0, &ar, NULL, NULL, NULL);
    if (a->stock[COLONIZE_CARGO_FOOD] != 8) {
      fprintf(
        stderr,
        "human no AI food bonus: want 8 got %d\n",
        a->stock[COLONIZE_CARGO_FOOD]
      );
      return 1;
    }
    fprintf(stderr, "AI colony food difficulty>>1 ok\n");
  }

  /*
   * FUN_364b_0688 Phase C: rebel dividend/divisor EOT tick.
   * Cite: sons_of_liberty.md; colony_prod_tick_rebel_accumulators.
   */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Town Hall");
    pool.building_type_count = 1;

    ColonizeColony* c = &pool.colonies[0];
    memset(c, 0, sizeof(*c));
    c->active = true;
    c->id = 1;
    c->x = 10;
    c->y = 12;
    c->nation_id = 0; /* human */
    c->building_in_production = -1;
    c->has_building[0] = true;
    c->stock[COLONIZE_CARGO_FOOD] = 50;
    c->colonists[0].active = true;
    c->colonists[0].building_type = 0;
    c->colonists[0].profession = COLONIZE_PROF_STATESMAN;
    c->colonists[0].field_job = -1;
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      c->tiles[t] = -1;
    }
    c->colonist_count = 1;
    c->population = 1;
    pool.colony_count = 1;

    ColonizeCol1Colony col1c;
    memset(&col1c, 0, sizeof(col1c));
    col1c.x = 10;
    col1c.y = 12;
    col1c.nation_id = 0;
    /* Pre-shrink 50%/100 so >>6 restores 50/100. */
    col1c.rebel_dividend = 50u << 6;
    col1c.rebel_divisor = 100u << 6;

    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.colony = &col1c;
    col1.head.colony_count = 1;
    col1.player[0].control = 0;
    col1.player[1].control = 1;
    for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
      col1.head.founding_father[i] = -1;
    }

    /* Town Hall +1 + Statesman 6 = 7 bells. */
    const int expect_bells = colony_prod_colony_bells(&pool, c);
    if (expect_bells != 7) {
      fprintf(stderr, "Phase C setup bells want 7 got %d\n", expect_bells);
      return 1;
    }

    /*
     * Production tab preview must match the EOT tick's FF-adjusted, per-worker
     * SoL bells (turn_count_bells_and_crosses_for_nation in turn.c), not the
     * plain colony_prod_colony_bells() used above only to sanity-check the
     * base rate. rebel_dividend/divisor above (50/100 <<6) give sol 50% ->
     * sol_bonus +1. sol_bonus now folds into colony_prod_bells_worker
     * *before* the skill-match doubling (matches FUN_15eb_1d4c's Statesman
     * body — manufacturing_worker_calc_1d4c.md): tag(3)+sol_bonus(1)=4,
     * doubled (skilled Statesman) = 8. Jefferson +50%: 8*1.5=12. Town Hall
     * passive +1 = 13.
     */
    col1.head.founding_father[FF_THOMAS_JEFFERSON] = 0; /* nation 0 owns it */
    ColonizeColonyPreview prev;
    colony_preview_compute(&pool, c, NULL, &col1, &prev);
    if (prev.bells != 13) {
      fprintf(stderr, "Phase C preview Jefferson bells want 13 got %d\n", prev.bells);
      return 1;
    }
    col1.head.founding_father[FF_THOMAS_JEFFERSON] = -1;

    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, &col1, NULL, 0, &prod, NULL, NULL, NULL);
    /* -= >>6 → 3150/6300; divisor+=2 → 6302; dividend+=7 → 3157. */
    if (col1c.rebel_dividend != 3157u || col1c.rebel_divisor != 6302u) {
      fprintf(
        stderr,
        "Phase C human tick want 3157/6302 got %u/%u\n",
        (unsigned)col1c.rebel_dividend,
        (unsigned)col1c.rebel_divisor
      );
      return 1;
    }

    /* WoI + crown-occupied: bells = -(7>>1) = -3 → dividend 3150-3=3147. */
    col1c.rebel_dividend = 50u << 6;
    col1c.rebel_divisor = 100u << 6;
    c->nation_id = 1; /* crown peer of human 0 */
    col1c.nation_id = 1;
    col1.head.game_options.woi = 1;
    c->stock[COLONIZE_CARGO_FOOD] = 50;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, &col1, NULL, 0, &prod, NULL, NULL, NULL);
    if (col1c.rebel_dividend != 3147u || col1c.rebel_divisor != 6302u) {
      fprintf(
        stderr,
        "Phase C WoI crown tick want 47/102 got %u/%u\n",
        (unsigned)col1c.rebel_dividend,
        (unsigned)col1c.rebel_divisor
      );
      return 1;
    }
    fprintf(stderr, "SoL Phase C rebel accumulator ok\n");
  }

  /*
   * FUN_364b_0688 Phase D: REBELMAJORITY / SONSUP chrome + report gates.
   * Cite: colony_eot_production.md; sons_of_liberty.md; GAME.TXT @REBELMAJORITY.
   */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Town Hall");
    pool.building_type_count = 1;

    ColonizeColony* c = &pool.colonies[0];
    memset(c, 0, sizeof(*c));
    c->active = true;
    c->id = 1;
    c->x = 10;
    c->y = 12;
    c->nation_id = 0;
    c->building_in_production = -1;
    c->has_building[0] = true;
    snprintf(c->name, sizeof(c->name), "Jamestown");
    c->stock[COLONIZE_CARGO_FOOD] = 80;
    c->colonists[0].active = true;
    c->colonists[0].building_type = 0;
    c->colonists[0].profession = COLONIZE_PROF_STATESMAN;
    c->colonists[0].field_job = -1;
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      c->tiles[t] = -1;
    }
    c->colonist_count = 1;
    c->population = 1;
    pool.colony_count = 1;

    ColonizeCol1Colony col1c;
    memset(&col1c, 0, sizeof(col1c));
    col1c.x = 10;
    col1c.y = 12;
    col1c.nation_id = 0;
    /* Pre-shrink 50%/100 → after tick +6 bells → ~50.07%. */
    col1c.rebel_dividend = 50u << 6;
    col1c.rebel_divisor = 100u << 6;

    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.colony = &col1c;
    col1.head.colony_count = 1;
    col1.player[0].control = 0;
    snprintf(col1.player[0].country_name, sizeof(col1.player[0].country_name), "England");
    for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
      col1.head.founding_father[i] = -1;
    }

    EuropeScreen eu;
    memset(&eu, 0, sizeof(eu));
    AiPopupState pops;
    ai_popup_init(&pops);
    ColonizeMsgCatalog game_txt;
    assets_msg_init(&game_txt);
    (void)assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT");

    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, &col1, &eu, 0, &prod, &pops, &game_txt, NULL);
    if ((c->colony_flags & COLONIZE_COLONY_FLAG_SOL_50) == 0) {
      fprintf(stderr, "Phase D majority: sol_50 latch missing\n");
      assets_msg_free(&game_txt);
      return 1;
    }
    if (strstr(eu.status, "SoL") == NULL && pops.queue_count < 1) {
      fprintf(stderr, "Phase D majority: want status/popup got '%s' q=%d\n", eu.status, pops.queue_count);
      assets_msg_free(&game_txt);
      return 1;
    }
    if (pops.queue_count >= 1 &&
        strstr(pops.queue[0].body, "majority") == NULL &&
        strstr(pops.queue[0].body, "SoL") == NULL &&
        strstr(eu.status, "up to") == NULL) {
      fprintf(stderr, "Phase D majority body/status weak: '%s' / '%s'\n", pops.queue[0].body, eu.status);
      assets_msg_free(&game_txt);
      return 1;
    }

    /* Suppress rebel-majority reports. */
    c->colony_flags = 0;
    col1c.rebel_dividend = 45u << 6;
    col1c.rebel_divisor = 100u << 6;
    c->stock[COLONIZE_CARGO_FOOD] = 80;
    col1.head.colony_report_options.report_rebel_majorities = 1;
    eu.status[0] = '\0';
    ai_popup_clear(&pops);
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, &col1, &eu, 0, &prod, &pops, &game_txt, NULL);
    if (pops.queue_count != 0 || strstr(eu.status, "SoL") != NULL) {
      fprintf(
        stderr,
        "Phase D suppress rebel maj: want quiet got q=%d '%s'\n",
        pops.queue_count,
        eu.status
      );
      assets_msg_free(&game_txt);
      return 1;
    }

    /* Decade up (@SONSUP): sol_50 already, 59%→60%. */
    col1.head.colony_report_options.report_rebel_majorities = 0;
    col1.head.colony_report_options.report_sons_of_liberty_membership = 0;
    c->colony_flags = COLONIZE_COLONY_FLAG_SOL_50;
    col1c.rebel_dividend = 3835u;
    col1c.rebel_divisor = 100u << 6;
    c->stock[COLONIZE_CARGO_FOOD] = 80;
    eu.status[0] = '\0';
    ai_popup_clear(&pops);
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, &col1, &eu, 0, &prod, &pops, &game_txt, NULL);
    if (strstr(eu.status, "SoL") == NULL && pops.queue_count < 1) {
      fprintf(stderr, "Phase D SONSUP: want status/popup got '%s' q=%d\n", eu.status, pops.queue_count);
      assets_msg_free(&game_txt);
      return 1;
    }
    /* Suppress sons membership reports. */
    c->colony_flags = COLONIZE_COLONY_FLAG_SOL_50;
    col1c.rebel_dividend = 3835u;
    col1c.rebel_divisor = 100u << 6;
    c->stock[COLONIZE_CARGO_FOOD] = 80;
    col1.head.colony_report_options.report_sons_of_liberty_membership = 1;
    eu.status[0] = '\0';
    ai_popup_clear(&pops);
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, &col1, &eu, 0, &prod, &pops, &game_txt, NULL);
    if (pops.queue_count != 0 || strstr(eu.status, "SoL") != NULL) {
      fprintf(
        stderr,
        "Phase D suppress sons: want quiet got q=%d '%s'\n",
        pops.queue_count,
        eu.status
      );
      assets_msg_free(&game_txt);
      return 1;
    }

    assets_msg_free(&game_txt);
    fprintf(stderr, "SoL Phase D membership chrome ok\n");
  }

  /*
   * FUN_364b_0688 Phase D Tory pressure: @INEFFICIENT / @EFFICIENT.
   * Cite: colony_eot_production.md; difficulty.md; GAME.TXT @INEFFICIENT.
   */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeColony* c = &pool.colonies[0];
    memset(c, 0, sizeof(*c));
    c->active = true;
    c->id = 1;
    c->x = 8;
    c->y = 8;
    c->nation_id = 0;
    c->building_in_production = -1;
    snprintf(c->name, sizeof(c->name), "Roanoke");
    c->stock[COLONIZE_CARGO_FOOD] = 200;
    for (int i = 0; i < 12; ++i) {
      c->colonists[i].active = true;
      c->colonists[i].building_type = -1;
      c->colonists[i].field_job = -1;
      c->colonists[i].profession = COLONIZE_PROF_FREE_COLONIST;
    }
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      c->tiles[t] = -1;
    }
    c->colonist_count = 12;
    c->population = 12;
    pool.colony_count = 1;

    ColonizeCol1Colony col1c;
    memset(&col1c, 0, sizeof(col1c));
    col1c.x = 8;
    col1c.y = 8;
    col1c.nation_id = 0;
    col1c.rebel_dividend = 0u << 6;
    col1c.rebel_divisor = 100u << 6;

    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.colony = &col1c;
    col1.head.colony_count = 1;
    col1.head.difficulty = 0; /* Discoverer thresh 10 */
    col1.player[0].control = 0;
    for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
      col1.head.founding_father[i] = -1;
    }
    /* Quiet SoL latch/decade chrome for this fixture. */
    col1.head.colony_report_options.report_rebel_majorities = 1;
    col1.head.colony_report_options.report_sons_of_liberty_membership = 1;

    EuropeScreen eu;
    memset(&eu, 0, sizeof(eu));
    AiPopupState pops;
    ai_popup_init(&pops);
    ColonizeMsgCatalog game_txt;
    assets_msg_init(&game_txt);
    (void)assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT");

    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, &col1, &eu, 0, &prod, &pops, &game_txt, NULL);
    if (c->inefficient_gov == 0) {
      fprintf(stderr, "INEFFICIENT: latch not set (sol low, pop 12)\n");
      assets_msg_free(&game_txt);
      return 1;
    }
    if (strstr(eu.status, "inefficient") == NULL && pops.queue_count < 1) {
      fprintf(stderr, "INEFFICIENT: want status/popup got '%s' q=%d\n", eu.status, pops.queue_count);
      assets_msg_free(&game_txt);
      return 1;
    }

    /* Raise SoL → tories 0 → @EFFICIENT. */
    col1c.rebel_dividend = 100u << 6;
    col1c.rebel_divisor = 100u << 6;
    c->stock[COLONIZE_CARGO_FOOD] = 200;
    c->colony_flags = (uint8_t)(COLONIZE_COLONY_FLAG_SOL_50 | COLONIZE_COLONY_FLAG_SOL_100);
    eu.status[0] = '\0';
    ai_popup_clear(&pops);
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, &col1, &eu, 0, &prod, &pops, &game_txt, NULL);
    if (c->inefficient_gov != 0) {
      fprintf(stderr, "EFFICIENT: latch still set\n");
      assets_msg_free(&game_txt);
      return 1;
    }
    if (strstr(eu.status, "efficien") == NULL && pops.queue_count < 1) {
      fprintf(stderr, "EFFICIENT: want status/popup got '%s' q=%d\n", eu.status, pops.queue_count);
      assets_msg_free(&game_txt);
      return 1;
    }

    /* Suppress reports: edge up silent but latch still sets. */
    c->inefficient_gov = 0;
    col1c.rebel_dividend = 0u << 6;
    col1c.rebel_divisor = 100u << 6;
    c->stock[COLONIZE_CARGO_FOOD] = 200;
    c->colony_flags = 0;
    col1.head.colony_report_options.report_inefficient_government = 1;
    eu.status[0] = '\0';
    ai_popup_clear(&pops);
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, &col1, &eu, 0, &prod, &pops, &game_txt, NULL);
    if (c->inefficient_gov == 0) {
      fprintf(stderr, "INEFFICIENT suppress: latch should still set\n");
      assets_msg_free(&game_txt);
      return 1;
    }
    if (pops.queue_count != 0 || strstr(eu.status, "inefficient") != NULL) {
      fprintf(
        stderr,
        "INEFFICIENT suppress: want quiet got q=%d '%s'\n",
        pops.queue_count,
        eu.status
      );
      assets_msg_free(&game_txt);
      return 1;
    }

    assets_msg_free(&game_txt);
    fprintf(stderr, "inefficient government chrome ok\n");
  }

  /* FUN_364b_0688 starve-kill: food_at_start==0 and still short → lose one. */
  {
    ColonizeColonyPool starve_pool;
    colonies_init(&starve_pool);
    ColonizeColony* s = &starve_pool.colonies[0];
    memset(s, 0, sizeof(*s));
    s->active = true;
    s->id = 1;
    s->building_in_production = -1;
    s->stock[COLONIZE_CARGO_FOOD] = 0; /* 2 pop need 4; stay starving */
    for (int i = 0; i < 2; ++i) {
      s->colonists[i].active = true;
      s->colonists[i].unit_type_index = 0;
      s->colonists[i].profession = UNITS_JOB_NONE;
      s->colonists[i].building_type = -1;
      s->colonists[i].field_job = -1;
    }
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      s->tiles[t] = -1;
    }
    s->colonist_count = 2;
    s->population = 2;
    starve_pool.colony_count = 1;
    ColonizeTurnResult sr;
    memset(&sr, 0, sizeof(sr));
    turn_colony_free_production(&starve_pool, s, NULL, &sr, NULL);
    if (!s->active || s->colonist_count != 1) {
      fprintf(
        stderr,
        "starve: colonist_count want 1 got %d active=%d\n",
        s->colonist_count,
        s->active
      );
      return 1;
    }
    if ((s->colony_flags & COLONIZE_COLONY_FLAG_STARVATION) == 0) {
      fprintf(stderr, "starve: STARVATION latch should remain\n");
      return 1;
    }
    fprintf(stderr, "colony starve-kill ok\n");
  }

  /* bugs.md (port_orange_starves.SAV): DOS deficit is DS:0x8e5a =
   * consumption − stock − production; a colony producing exactly what it
   * eats at 0 stores (commons feeds the lone statesman) must NOT latch
   * starvation or lose anyone, ever. Simulated without a map by pre-adding
   * the "production" to stock, which is how the tick's own commons food
   * lands before the latch runs — deficit 2−0−2 = 0. */
  {
    ColonizeColonyPool zpool;
    colonies_init(&zpool);
    ColonizeColony* s = &zpool.colonies[0];
    memset(s, 0, sizeof(*s));
    s->active = true;
    s->id = 1;
    s->building_in_production = -1;
    s->stock[COLONIZE_CARGO_FOOD] = 2; /* 1 pop eats 2 — net zero, not short */
    s->colonists[0].active = true;
    s->colonists[0].unit_type_index = 0;
    s->colonists[0].profession = UNITS_JOB_NONE;
    s->colonists[0].building_type = 9;
    s->colonists[0].field_job = -1;
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      s->tiles[t] = -1;
    }
    s->colonist_count = 1;
    s->population = 1;
    zpool.colony_count = 1;
    ColonizeTurnResult sr;
    memset(&sr, 0, sizeof(sr));
    turn_colony_free_production(&zpool, s, NULL, &sr, NULL);
    if (!s->active || s->colonist_count != 1) {
      fprintf(
        stderr, "starve netzero: colonist_count want 1 got %d active=%d\n",
        s->colonist_count, s->active
      );
      return 1;
    }
    if ((s->colony_flags & COLONIZE_COLONY_FLAG_STARVATION) != 0) {
      fprintf(stderr, "starve netzero: STARVATION latch must stay clear\n");
      return 1;
    }
    fprintf(stderr, "colony net-zero food no-starve ok\n");
  }

  /* Last colonist starve → @VANISH + abandon (DOS 0xe47). */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeColony* s = &pool.colonies[0];
    memset(s, 0, sizeof(*s));
    s->active = true;
    s->id = 1;
    s->nation_id = 0;
    s->building_in_production = -1;
    snprintf(s->name, sizeof(s->name), "Roanoke");
    s->stock[COLONIZE_CARGO_FOOD] = 0;
    s->colonists[0].active = true;
    s->colonists[0].unit_type_index = 0;
    s->colonists[0].profession = UNITS_JOB_NONE;
    s->colonists[0].building_type = -1;
    s->colonists[0].field_job = -1;
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      s->tiles[t] = -1;
    }
    s->colonist_count = 1;
    s->population = 1;
    pool.colony_count = 1;

    EuropeScreen eu;
    memset(&eu, 0, sizeof(eu));
    AiPopupState pops;
    ai_popup_init(&pops);
    ColonizeMsgCatalog game_txt;
    assets_msg_init(&game_txt);
    (void)assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT");

    ColonizeTurnResult sr;
    memset(&sr, 0, sizeof(sr));
    turn_run_colony_production(&pool, NULL, NULL, &eu, 0, &sr, &pops, &game_txt, NULL);
    if (s->active || pool.colony_count != 0) {
      fprintf(
        stderr,
        "vanish: want abandoned active=%d count=%d\n",
        s->active,
        pool.colony_count
      );
      assets_msg_free(&game_txt);
      return 1;
    }
    if (pops.queue_count < 1 ||
        (strstr(pops.queue[0].body, "vanished") == NULL &&
         strstr(pops.queue[0].body, "Roanoke") == NULL)) {
      fprintf(
        stderr,
        "vanish: popup weak q=%d body='%s'\n",
        pops.queue_count,
        pops.queue_count > 0 ? pops.queue[0].body : ""
      );
      assets_msg_free(&game_txt);
      return 1;
    }
    assets_msg_free(&game_txt);
    fprintf(stderr, "colony vanish starve ok\n");
  }

  /* Starve-kill chrome: @STARVE1 (spring) / @STARVE2 (autumn). */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeColony* s = &pool.colonies[0];
    memset(s, 0, sizeof(*s));
    s->active = true;
    s->id = 1;
    s->nation_id = 0;
    s->building_in_production = -1;
    snprintf(s->name, sizeof(s->name), "Roanoke");
    s->stock[COLONIZE_CARGO_FOOD] = 0;
    for (int i = 0; i < 2; ++i) {
      s->colonists[i].active = true;
      s->colonists[i].unit_type_index = 0;
      s->colonists[i].profession = UNITS_JOB_NONE;
      s->colonists[i].building_type = -1;
      s->colonists[i].field_job = -1;
    }
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      s->tiles[t] = -1;
    }
    s->colonist_count = 2;
    s->population = 2;
    pool.colony_count = 1;

    EuropeScreen eu;
    memset(&eu, 0, sizeof(eu));
    AiPopupState pops;
    ai_popup_init(&pops);
    ColonizeMsgCatalog game_txt;
    assets_msg_init(&game_txt);
    (void)assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT");

    ColonizeTurnResult sr;
    memset(&sr, 0, sizeof(sr));
    turn_run_colony_production(&pool, NULL, NULL, &eu, 0, &sr, &pops, &game_txt, NULL);
    if (s->colonist_count != 1) {
      fprintf(stderr, "starve1: colonist_count want 1 got %d\n", s->colonist_count);
      assets_msg_free(&game_txt);
      return 1;
    }
    if (pops.queue_count < 1 ||
        (strstr(pops.queue[0].body, "Roanoke") == NULL &&
         strstr(pops.queue[0].body, "starv") == NULL)) {
      fprintf(
        stderr,
        "starve1: popup weak q=%d body='%s'\n",
        pops.queue_count,
        pops.queue_count > 0 ? pops.queue[0].body : ""
      );
      assets_msg_free(&game_txt);
      return 1;
    }
    if (strstr(pops.queue[0].body, "coming soon") != NULL) {
      fprintf(stderr, "starve1: spring must not use STARVE2 got '%s'\n", pops.queue[0].body);
      assets_msg_free(&game_txt);
      return 1;
    }
    fprintf(stderr, "starve1 chrome ok\n");

    /* Reset for autumn → STARVE2. */
    memset(s, 0, sizeof(*s));
    s->active = true;
    s->id = 1;
    s->nation_id = 0;
    s->building_in_production = -1;
    snprintf(s->name, sizeof(s->name), "Roanoke");
    s->stock[COLONIZE_CARGO_FOOD] = 0;
    for (int i = 0; i < 2; ++i) {
      s->colonists[i].active = true;
      s->colonists[i].unit_type_index = 0;
      s->colonists[i].profession = UNITS_JOB_NONE;
      s->colonists[i].building_type = -1;
      s->colonists[i].field_job = -1;
    }
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      s->tiles[t] = -1;
    }
    s->colonist_count = 2;
    s->population = 2;
    pool.colony_count = 1;

    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.head.autumn = 1;
    /* Difficulty>=2: skip the Discoverer/Explorer easy-mode no-kill mercy
     * (FUN_364b_0688) so this deterministically still starve-kills. */
    col1.head.difficulty = 2;
    eu.status[0] = '\0';
    ai_popup_clear(&pops);
    memset(&sr, 0, sizeof(sr));
    turn_run_colony_production(&pool, NULL, &col1, &eu, 0, &sr, &pops, &game_txt, NULL);
    if (pops.queue_count < 1 ||
        (strstr(pops.queue[0].body, "coming soon") == NULL &&
         strstr(pops.queue[0].body, "worse") == NULL)) {
      fprintf(
        stderr,
        "starve2: want winter-coming q=%d body='%s'\n",
        pops.queue_count,
        pops.queue_count > 0 ? pops.queue[0].body : ""
      );
      assets_msg_free(&game_txt);
      return 1;
    }
    assets_msg_free(&game_txt);
    fprintf(stderr, "starve2 chrome ok\n");
  }

  /* Food shortage status for human (production deficit; stock stays ≥ need*4). */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->nation_id = 0;
    col->building_in_production = -1;
    /* Eat 4 → 16 left (= need*4); avoids @FOODLOW overwriting shortage status. */
    col->stock[COLONIZE_CARGO_FOOD] = 20;
    col->colonists[0].active = true;
    col->colonists[1].active = true;
    col->colonist_count = 2;
    col->population = 2;
    pool.colony_count = 1;

    EuropeScreen eu;
    memset(&eu, 0, sizeof(eu));
    eu.cargo_count = COLONIZE_CARGO_COUNT;
    for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
      eu.cargo[i].bid = 1;
    }

    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, NULL, &eu, 0, &prod, NULL, NULL, NULL);
    if (prod.food_shortages < 1 || strstr(eu.status, "Food shortage") == NULL) {
      fprintf(
        stderr,
        "food shortage want count+status got shortages=%d '%s'\n",
        prod.food_shortages,
        eu.status
      );
      return 1;
    }
    fprintf(stderr, "food shortage status ok\n");
  }

  /* DOS 0xe5e @FOODLOW: production shortfall 8e32; stock < 8e32×4; not starving. */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->nation_id = 0;
    col->building_in_production = -1;
    snprintf(col->name, sizeof(col->name), "Jamestown");
    /* 2 pop need 4; no field food → shortfall 4; start 10 → after eat 6 (< 16). */
    col->stock[COLONIZE_CARGO_FOOD] = 10;
    col->colonists[0].active = true;
    col->colonists[1].active = true;
    col->colonist_count = 2;
    col->population = 2;
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      col->tiles[t] = -1;
    }
    pool.colony_count = 1;

    EuropeScreen eu;
    memset(&eu, 0, sizeof(eu));
    AiPopupState pops;
    ai_popup_init(&pops);
    ColonizeMsgCatalog game_txt;
    assets_msg_init(&game_txt);
    (void)assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT");

    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, NULL, &eu, 0, &prod, &pops, &game_txt, NULL);
    if (col->stock[COLONIZE_CARGO_FOOD] != 6) {
      fprintf(stderr, "foodlow: stock want 6 got %d\n", col->stock[COLONIZE_CARGO_FOOD]);
      assets_msg_free(&game_txt);
      return 1;
    }
    if (strstr(eu.status, "Food low") == NULL && strstr(eu.status, "Jamestown") == NULL) {
      fprintf(stderr, "foodlow: status want Food low/Jamestown got '%s'\n", eu.status);
      assets_msg_free(&game_txt);
      return 1;
    }
    if (pops.queue_count < 1) {
      fprintf(stderr, "foodlow: expected FOODLOW popup\n");
      assets_msg_free(&game_txt);
      return 1;
    }
    if (strstr(pops.queue[0].body, "Jamestown") == NULL &&
        strstr(pops.queue[0].body, "food") == NULL &&
        strstr(pops.queue[0].body, "Food") == NULL) {
      fprintf(stderr, "foodlow: popup body weak: '%s'\n", pops.queue[0].body);
      assets_msg_free(&game_txt);
      return 1;
    }
    assets_msg_free(&game_txt);
    fprintf(stderr, "foodlow chrome ok\n");
  }

  /* Surplus harvest (8e32==0): no @FOODLOW even when stock < need×4. */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->nation_id = 0;
    col->building_in_production = -1;
    snprintf(col->name, sizeof(col->name), "Plymouth");
    col->stock[COLONIZE_CARGO_FOOD] = 12; /* after surplus net, still modest vs need×4 */
    col->colonists[0].active = true;
    col->colonists[0].field_job = COLONIZE_JOB_FARMER;
    col->colonists[0].profession = COLONIZE_JOB_FARMER;
    col->colonists[0].building_type = -1;
    col->colonists[1].active = true;
    col->colonists[1].field_job = COLONIZE_JOB_FARMER;
    col->colonists[1].profession = COLONIZE_JOB_FARMER;
    col->colonists[1].building_type = -1;
    col->colonist_count = 2;
    col->population = 2;
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      col->tiles[t] = -1;
    }
    col->tiles[0] = 0;
    col->tiles[1] = 1;
    pool.colony_count = 1;

    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    char err[64];
    if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
      fprintf(stderr, "foodlow-surplus: map_alloc %s\n", err);
      return 1;
    }
    for (int i = 0; i < 64; ++i) {
      map.terrain[i] = 1; /* plains */
    }
    col->x = 3;
    col->y = 3;

    EuropeScreen eu;
    memset(&eu, 0, sizeof(eu));
    AiPopupState pops;
    ai_popup_init(&pops);

    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, &map, NULL, &eu, 0, &prod, &pops, NULL, NULL);
    if (prod.food_shortages != 0) {
      fprintf(stderr, "foodlow-surplus: unexpected shortage %d\n", prod.food_shortages);
      map_free(&map);
      return 1;
    }
    if (strstr(eu.status, "Food low") != NULL) {
      fprintf(stderr, "foodlow-surplus: must not warn on surplus '%s'\n", eu.status);
      map_free(&map);
      return 1;
    }
    for (int i = 0; i < pops.queue_count; ++i) {
      if (strstr(pops.queue[i].body, "rapidly depleting") != NULL ||
          strstr(pops.queue[i].body, "Food low") != NULL) {
        fprintf(stderr, "foodlow-surplus: FOODLOW popup with surplus food\n");
        map_free(&map);
        return 1;
      }
    }
    /* Sanity: stock should not be below one turn's need after a surplus turn. */
    if (col->stock[COLONIZE_CARGO_FOOD] < 4) {
      fprintf(
        stderr,
        "foodlow-surplus: expected surplus leave stock>=4 got %d\n",
        col->stock[COLONIZE_CARGO_FOOD]
      );
      map_free(&map);
      return 1;
    }
    map_free(&map);
    fprintf(stderr, "foodlow surplus no-warn ok\n");
  }

  /* @FOOD1: first starvation latch (stock after eat < need, no prior latch). */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->nation_id = 0;
    col->building_in_production = -1;
    snprintf(col->name, sizeof(col->name), "Plymouth");
    /* 2 pop need 4; start 3 → after eat 0 (< need); no prior STARVATION. */
    col->stock[COLONIZE_CARGO_FOOD] = 3;
    col->colonists[0].active = true;
    col->colonists[1].active = true;
    col->colonist_count = 2;
    col->population = 2;
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      col->tiles[t] = -1;
    }
    pool.colony_count = 1;

    EuropeScreen eu;
    memset(&eu, 0, sizeof(eu));
    AiPopupState pops;
    ai_popup_init(&pops);
    ColonizeMsgCatalog game_txt;
    assets_msg_init(&game_txt);
    (void)assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT");

    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, NULL, &eu, 0, &prod, &pops, &game_txt, NULL);
    if ((col->colony_flags & COLONIZE_COLONY_FLAG_STARVATION) == 0) {
      fprintf(stderr, "food1: want STARVATION latch\n");
      assets_msg_free(&game_txt);
      return 1;
    }
    if (strstr(eu.status, "depleted") == NULL && strstr(eu.status, "Plymouth") == NULL) {
      fprintf(stderr, "food1: status want depleted/Plymouth got '%s'\n", eu.status);
      assets_msg_free(&game_txt);
      return 1;
    }
    if (pops.queue_count < 1 ||
        (strstr(pops.queue[0].body, "depleted") == NULL &&
         strstr(pops.queue[0].body, "Plymouth") == NULL)) {
      fprintf(
        stderr,
        "food1: popup weak q=%d body='%s'\n",
        pops.queue_count,
        pops.queue_count > 0 ? pops.queue[0].body : ""
      );
      assets_msg_free(&game_txt);
      return 1;
    }
    if (strstr(pops.queue[0].body, "Winter") != NULL) {
      fprintf(stderr, "food1: spring must not use FOOD2 got '%s'\n", pops.queue[0].body);
      assets_msg_free(&game_txt);
      return 1;
    }
    assets_msg_free(&game_txt);
    fprintf(stderr, "food1 chrome ok\n");
  }

  /* @FOOD2: same latch with Col1 autumn → winter-soon wording. */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->nation_id = 0;
    col->building_in_production = -1;
    snprintf(col->name, sizeof(col->name), "Plymouth");
    col->stock[COLONIZE_CARGO_FOOD] = 3;
    col->colonists[0].active = true;
    col->colonists[1].active = true;
    col->colonist_count = 2;
    col->population = 2;
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      col->tiles[t] = -1;
    }
    pool.colony_count = 1;

    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.head.autumn = 1;

    EuropeScreen eu;
    memset(&eu, 0, sizeof(eu));
    AiPopupState pops;
    ai_popup_init(&pops);
    ColonizeMsgCatalog game_txt;
    assets_msg_init(&game_txt);
    (void)assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT");

    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, &col1, &eu, 0, &prod, &pops, &game_txt, NULL);
    if (pops.queue_count < 1 ||
        (strstr(pops.queue[0].body, "Winter") == NULL &&
         strstr(pops.queue[0].body, "starve") == NULL)) {
      fprintf(
        stderr,
        "food2: want Winter/starve q=%d body='%s'\n",
        pops.queue_count,
        pops.queue_count > 0 ? pops.queue[0].body : ""
      );
      assets_msg_free(&game_txt);
      return 1;
    }
    assets_msg_free(&game_txt);
    fprintf(stderr, "food2 chrome ok\n");
  }

  /*
   * FUN_364b_0688 phase O — AI dump-sell: non-human Euro surplus → gold before
   * spoilage. Human colony must not sell. Horses → nation_horses (no gold).
   */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeColony* ai = &pool.colonies[0];
    memset(ai, 0, sizeof(*ai));
    ai->active = true;
    ai->id = 1;
    ai->nation_id = 1; /* AI French */
    ai->building_in_production = -1;
    ai->warehouse_level = 0; /* cap 100 */
    ai->stock[COLONIZE_CARGO_TOBACCO] = 150;
    ai->stock[COLONIZE_CARGO_HORSES] = 130;
    ai->stock[COLONIZE_CARGO_MUSKETS] = 160;
    ai->stock[COLONIZE_CARGO_FOOD] = 50;
    ai->colonists[0].active = true;
    ai->colonist_count = 1;
    ai->population = 1;

    ColonizeColony* human = &pool.colonies[1];
    memset(human, 0, sizeof(*human));
    human->active = true;
    human->id = 2;
    human->nation_id = 0;
    human->building_in_production = -1;
    human->warehouse_level = 0;
    human->stock[COLONIZE_CARGO_TOBACCO] = 150;
    human->stock[COLONIZE_CARGO_FOOD] = 50; /* avoid Phase J vanish on 0 food */
    human->colonists[0].active = true;
    human->colonist_count = 1;
    human->population = 1;
    pool.colony_count = 2;

    EuropeScreen eu;
    memset(&eu, 0, sizeof(eu));
    eu.cargo_count = COLONIZE_CARGO_COUNT;
    for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
      eu.cargo[i].bid = 10;
      eu.cargo[i].low = 1;
      eu.cargo[i].high = 20;
    }
    eu.tax_percent = 0;

    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.nation[1].tax_rate = 20;

    /* Direct API at euro_price−1 = 9, tax 20: tobacco 50→450−90=360 + muskets rem 10→90−18=72; horses→word; muskets 1 batch. */
    const int gained = europe_ai_colony_dump_sell(&eu, &pool, ai, &col1, 0);
    if (gained != 432 || col1.nation[1].gold != 432u) {
      fprintf(
        stderr,
        "dump-sell gained=%d gold=%u (want 432)\n",
        gained,
        (unsigned)col1.nation[1].gold
      );
      return 1;
    }
    if (eu.nation_horses[1] != 30u) {
      fprintf(stderr, "dump-sell horses word want 30 got %u\n", (unsigned)eu.nation_horses[1]);
      return 1;
    }
    if (eu.nation_musket_batches[1] != 1u) {
      fprintf(
        stderr,
        "dump-sell musket batches want 1 got %u\n",
        (unsigned)eu.nation_musket_batches[1]
      );
      return 1;
    }
    if (ai->stock[COLONIZE_CARGO_TOBACCO] != 150) {
      fprintf(stderr, "dump-sell must leave stock for spoilage, got %d\n", ai->stock[COLONIZE_CARGO_TOBACCO]);
      return 1;
    }
    if (europe_ai_colony_dump_sell(&eu, &pool, human, &col1, 0) != 0) {
      fprintf(stderr, "dump-sell must skip human colony\n");
      return 1;
    }

    /* Wired through production: spoilage clamps after credit. */
    col1.nation[1].gold = 0;
    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, &col1, &eu, 0, &prod, NULL, NULL, NULL);
    if (ai->stock[COLONIZE_CARGO_TOBACCO] != 100 || ai->stock[COLONIZE_CARGO_HORSES] != 100) {
      fprintf(
        stderr,
        "produce+dump tobacco=%d horses=%d (want 100/100)\n",
        ai->stock[COLONIZE_CARGO_TOBACCO],
        ai->stock[COLONIZE_CARGO_HORSES]
      );
      return 1;
    }
    if (col1.nation[1].gold != 432u) {
      fprintf(stderr, "produce+dump gold=%u (want 432)\n", (unsigned)col1.nation[1].gold);
      return 1;
    }
    if (human->stock[COLONIZE_CARGO_TOBACCO] != 100 || col1.nation[0].gold != 0u) {
      fprintf(
        stderr,
        "human must spoil without sell tobacco=%d gold=%u\n",
        human->stock[COLONIZE_CARGO_TOBACCO],
        (unsigned)col1.nation[0].gold
      );
      return 1;
    }
    fprintf(stderr, "AI colony dump-sell ok\n");
  }

  /*
   * Nation ticks: AI Euro colonies accrue liberty_bells into col1 (DOS 00f2 /
   * 4345_0a22 per nation). Human Europe chrome unchanged.
   */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Town Hall");
    pool.building_type_count = 1;

    ColonizeColony* ai = &pool.colonies[0];
    memset(ai, 0, sizeof(*ai));
    ai->active = true;
    ai->id = 1;
    ai->nation_id = 1;
    ai->building_in_production = -1;
    ai->has_building[0] = true;
    ai->colonists[0].active = true;
    ai->colonists[0].building_type = 0;
    ai->colonists[0].profession = COLONIZE_PROF_STATESMAN;
    ai->colonist_count = 1;
    ai->population = 1;
    pool.colony_count = 1;

    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.player[0].control = 0;
    col1.player[1].control = 1;
    col1.player[2].control = 2;
    col1.player[3].control = 1;
    for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
      col1.head.founding_father[i] = -1;
    }

    ColonizeTurnContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.human_nation = 0;
    ctx.colonies = &pool;
    ctx.col1 = &col1;
    ctx.col1_ok = true;

    turn_run_nation_ticks(&ctx, NULL);
    if (col1.nation[1].liberty_bells_last_turn == 0 || col1.nation[1].liberty_bells_total == 0) {
      fprintf(
        stderr,
        "AI bells last=%u total=%u (want >0)\n",
        (unsigned)col1.nation[1].liberty_bells_last_turn,
        (unsigned)col1.nation[1].liberty_bells_total
      );
      return 1;
    }
    if (col1.nation[0].liberty_bells_total != 0) {
      fprintf(stderr, "human with no colonies should stay 0 bells\n");
      return 1;
    }
    if (col1.nation[2].liberty_bells_total != 0) {
      fprintf(stderr, "withdrawn nation must not accrue bells\n");
      return 1;
    }
    fprintf(stderr, "AI nation bells accrue ok\n");
  }

  /*
   * Tory penalty (negative colony_prod_sol_bonus) must reduce bells, not get
   * silently dropped — turn_count_bells_and_crosses_for_nation used to guard
   * the SoL adjustment on `sol_b > 0`, which threw away every Tory penalty.
   * FUN_15eb_1d4c folds the (signed) term in unconditionally.
   */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Town Hall");
    pool.building_type_count = 1;

    ColonizeColony* ai = &pool.colonies[0];
    memset(ai, 0, sizeof(*ai));
    ai->active = true;
    ai->id = 1;
    ai->nation_id = 1;
    ai->building_in_production = -1;
    ai->has_building[0] = true;
    ai->colonists[0].active = true;
    ai->colonists[0].building_type = 0;
    ai->colonists[0].profession = COLONIZE_PROF_STATESMAN;
    ai->colonist_count = 1;
    ai->population = 15; /* tories=(15*100+50)/100=15; thresh=10 (AI, fixed); mod=-1 */
    pool.colony_count = 1;

    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.player[0].control = 0;
    col1.player[1].control = 1;
    for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
      col1.head.founding_father[i] = -1;
    }

    ColonizeTurnContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.human_nation = 0;
    ctx.colonies = &pool;
    ctx.col1 = &col1;
    ctx.col1_ok = true;

    turn_run_nation_ticks(&ctx, NULL);
    /* sol_bonus now folds into colony_prod_bells_worker *before* the
     * skill-match doubling (matches FUN_15eb_1d4c's Statesman body):
     * tag(3)+sol_b(-1)=2, doubled (skilled Statesman) = 4. Town Hall
     * passive +1 = 5 — not 7 (bug would leave it there un-penalized), and
     * not 6 either (that was this fix's own first pass, which only moved
     * the sign-drop bug and still added sol_b post-doubling). */
    if (col1.nation[1].liberty_bells_total != 5) {
      fprintf(
        stderr,
        "Tory-penalty bells want 5 got %u\n",
        (unsigned)col1.nation[1].liberty_bells_total
      );
      return 1;
    }
    fprintf(stderr, "Tory penalty reduces bells ok\n");
  }

  /*
   * FUN_5bfb_00f8 Euro rank: gold/100 + 2*colonies + pop → inverse place.
   */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    for (int n = 0; n < 3; ++n) {
      ColonizeColony* c = &pool.colonies[n];
      memset(c, 0, sizeof(*c));
      c->active = true;
      c->id = n + 1;
      c->nation_id = n;
      c->building_in_production = -1;
      c->population = (n == 0) ? 10 : (n == 1) ? 3 : 1;
      c->colonist_count = c->population;
    }
    pool.colony_count = 3;

    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.nation[0].gold = 100; /* +1 */
    col1.nation[1].gold = 5000; /* +50 — should win despite fewer pops */
    col1.nation[2].gold = 0;
    col1.nation[3].gold = 0;

    uint8_t rank[4];
    if (turn_rank_euro_nations(&col1, &pool, rank) != 0) {
      fprintf(stderr, "rank: call failed\n");
      return 1;
    }
    /* FR gold-heavy → place 0; EN pop → place 1; SP → 2; DU empty → 3. */
    if (rank[1] != 0 || rank[0] != 1 || rank[2] != 2 || rank[3] != 3) {
      fprintf(
        stderr,
        "rank got EN=%u FR=%u SP=%u DU=%u want 1/0/2/3\n",
        (unsigned)rank[0],
        (unsigned)rank[1],
        (unsigned)rank[2],
        (unsigned)rank[3]
      );
      return 1;
    }
    fprintf(stderr, "euro power rank ok\n");
  }

  /*
   * FUN_364b_0688 F–G thin: Teacher in Schoolhouse graduates Free Colonist
   * after 4 turns_in_job → Farmer.
   */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Schoolhouse");
    pool.building_type_count = 1;

    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->nation_id = 0;
    col->building_in_production = -1;
    col->has_building[0] = true;
    col->stock[COLONIZE_CARGO_FOOD] = 50;
    col->colonists[0].active = true;
    col->colonists[0].profession = COLONIZE_PROF_TEACHER;
    col->colonists[0].building_type = 0;
    col->colonists[0].field_job = -1;
    col->colonists[0].turns_in_job = 3; /* one tick → 4 ≥ need */
    col->colonists[1].active = true;
    col->colonists[1].profession = COLONIZE_PROF_FREE_COLONIST;
    col->colonists[1].building_type = 0;
    col->colonists[1].field_job = -1;
    col->colonists[1].turns_in_job = 0;
    col->colonist_count = 2;
    col->population = 2;
    pool.colony_count = 1;

    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_colony_free_production(&pool, col, NULL, &prod, NULL);
    if (col->colonists[1].profession != COLONIZE_JOB_FARMER) {
      fprintf(
        stderr,
        "education: student profession want Farmer(%d) got %d\n",
        COLONIZE_JOB_FARMER,
        col->colonists[1].profession
      );
      return 1;
    }
    if (col->colonists[0].turns_in_job != 0 || col->colonists[1].turns_in_job != 0) {
      fprintf(stderr, "education: turns should reset after graduate\n");
      return 1;
    }
    fprintf(stderr, "colony education graduate ok\n");

    /* Teacher field_job specialty → graduate that skill. */
    col->colonists[0].profession = COLONIZE_PROF_TEACHER;
    col->colonists[0].building_type = 0;
    col->colonists[0].field_job = COLONIZE_JOB_FISHERMAN;
    col->colonists[0].turns_in_job = 3;
    col->colonists[1].profession = COLONIZE_PROF_FREE_COLONIST;
    col->colonists[1].building_type = 0;
    col->colonists[1].field_job = -1;
    col->colonists[1].turns_in_job = 0;
    memset(&prod, 0, sizeof(prod));
    turn_colony_free_production(&pool, col, NULL, &prod, NULL);
    if (col->colonists[1].profession != COLONIZE_JOB_FISHERMAN) {
      fprintf(
        stderr,
        "education specialty: want Fisherman(%d) got %d\n",
        COLONIZE_JOB_FISHERMAN,
        col->colonists[1].profession
      );
      return 1;
    }
    fprintf(stderr, "colony education specialty ok\n");

    /* Teacher ready, no students → status crumb. */
    {
      EuropeScreen eu;
      memset(&eu, 0, sizeof(eu));
      col->colonists[0].profession = COLONIZE_PROF_TEACHER;
      col->colonists[0].building_type = 0;
      col->colonists[0].turns_in_job = 3;
      col->colonists[1].active = false;
      col->colonist_count = 1;
      col->population = 1;
      col->stock[COLONIZE_CARGO_FOOD] = 50; /* avoid food-shortage overwrite */
      memset(&prod, 0, sizeof(prod));
      turn_run_colony_production(&pool, NULL, NULL, &eu, 0, &prod, NULL, NULL, NULL);
      if (strstr(eu.status, "No students") == NULL) {
        fprintf(stderr, "education no-students want status got '%s'\n", eu.status);
        return 1;
      }
      fprintf(stderr, "colony education no-students ok\n");
      col->colonists[1].active = true;
      col->colonist_count = 2;
      col->population = 2;
    }  }

  /*
   * Phase H thin: Free Colonist on field job discovers that skill (1/100).
   */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->nation_id = 0;
    col->building_in_production = -1;
    col->stock[COLONIZE_CARGO_FOOD] = 500;
    col->colonists[0].active = true;
    col->colonists[0].profession = COLONIZE_PROF_FREE_COLONIST;
    col->colonists[0].building_type = -1;
    col->colonists[0].field_job = COLONIZE_JOB_FARMER;
    col->colonist_count = 1;
    col->population = 1;
    col->tiles[0] = 0;
    pool.colony_count = 1;

    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.head.year = 1492;
    ColonizeDosRng rng;
    dos_rng_seed(&rng, 1);
    int discovered = 0;
    for (unsigned t = 0; t < 5000u; ++t) {
      col1.head.turn = (uint16_t)(t & 0xffffu);
      col->colonists[0].profession = COLONIZE_PROF_FREE_COLONIST;
      col->colonists[0].field_job = COLONIZE_JOB_FARMER;
      col->stock[COLONIZE_CARGO_FOOD] = 500;
      ColonizeTurnResult prod;
      memset(&prod, 0, sizeof(prod));
      turn_run_colony_production(&pool, NULL, &col1, NULL, 0, &prod, NULL, NULL, &rng);
      if (col->colonists[0].profession == COLONIZE_JOB_FARMER) {
        discovered = 1;
        break;
      }
    }
    if (!discovered) {
      fprintf(stderr, "education H: no field skill discover in 5000 ticks\n");
      return 1;
    }
    fprintf(stderr, "colony random field skill ok\n");
  }

  /*
   * AI Euro crosses: +2 /turn; threshold → Free Colonist spawn PARKED
   * (seed-100 TURN goldens sit at needed without convert).
   * Cite: turn.c nation ticks; nation_ticks_bells_ff.md.
   */
  {
    ColonizeUnitPool units;
    memset(&units, 0, sizeof(units));
    units_reset(&units);
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Colonists");
    units.type_count = 1;

    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.player[0].control = 0;
    col1.player[1].control = 1;
    col1.nation[1].current_crosses = 8;
    col1.nation[1].needed_crosses = 8;

    ColonizeTurnContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.human_nation = 0;
    ctx.units = &units;
    ctx.col1 = &col1;
    ctx.col1_ok = true;

    ColonizeTurnResult out;
    memset(&out, 0, sizeof(out));
    turn_run_nation_ticks(&ctx, &out);
    /* PARKED spawn: no immigrant; crosses still get AI +2 → 10/8. */
    if (out.immigrants_arrived != 0) {
      fprintf(
        stderr,
        "AI immigrant PARKED: want 0 arrived got %d\n",
        out.immigrants_arrived
      );
      return 1;
    }
    if (col1.nation[1].needed_crosses != 8 || col1.nation[1].current_crosses != 10) {
      fprintf(
        stderr,
        "AI crosses want 10/8 got %u/%u\n",
        (unsigned)col1.nation[1].current_crosses,
        (unsigned)col1.nation[1].needed_crosses
      );
      return 1;
    }
    fprintf(stderr, "AI crosses +2 (immigrant spawn PARKED) ok\n");
  }

  /* 5e52 phase 4 immigration pressure thin. */
  {
    EuropeScreen eu;
    memset(&eu, 0, sizeof(eu));
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->nation_id = 0;
    col->colonist_count = 5;
    col->population = 5;
    pool.colony_count = 1;
    ColonizeUnitPool units;
    memset(&units, 0, sizeof(units));
    units_reset(&units);
    europe_tick_immigration_pressure(&eu, &pool, &units, NULL, 0, NULL);
    if (eu.needed_crosses <= 0) {
      fprintf(stderr, "immigration needed want >0 got %u\n", (unsigned)eu.needed_crosses);
      return 1;
    }
    if (eu.current_crosses != 2) {
      fprintf(stderr, "immigration current want +2 got %u\n", (unsigned)eu.current_crosses);
      return 1;
    }
    /* Force phase5: current > needed → dock; @UNREST popup owns chrome (no auto-Europe). */
    eu.current_crosses = (uint16_t)(eu.needed_crosses + 10);
    eu.immigration_pressure = (int16_t)eu.current_crosses;
    eu.dock_count = 0;
    eu.status[0] = '\0';
    eu.open_on_dock = false;
    if (!europe_tick_immigration_pressure(&eu, &pool, &units, NULL, 0, NULL) || eu.open_on_dock ||
        eu.dock_count < 1 || strstr(eu.status, "Immigrant") == NULL) {
      fprintf(
        stderr,
        "phase5 want dock+status no-open open=%d dock=%d '%s'\n",
        eu.open_on_dock ? 1 : 0,
        eu.dock_count,
        eu.status
      );
      return 1;
    }
    fprintf(stderr, "immigration pressure tick ok\n");
    /* 584a: AI nation ((8-diff)*score)>>3; English human also *2/3. */
    {
      ColonizeCol1Save icol;
      memset(&icol, 0, sizeof(icol));
      icol.head.difficulty = 4; /* Viceroy → AI score half */
      icol.player[0].control = 0;
      icol.player[1].control = 1;
      col->nation_id = 1;
      eu.current_crosses = 0;
      eu.immigration_pressure = 0;
      europe_tick_immigration_pressure(&eu, &pool, &units, &icol, 1, NULL);
      const int ai_score = (int)eu.needed_crosses;
      col->nation_id = 0;
      eu.current_crosses = 0;
      eu.immigration_pressure = 0;
      europe_tick_immigration_pressure(&eu, &pool, &units, &icol, 0, NULL);
      const int en_score = (int)eu.needed_crosses;
      /* Base pop5 → ((5)<<1)+8=18; EN *2/3=12; AI half of 18=9. */
      if (ai_score != 9 || en_score != 12) {
        fprintf(stderr, "584a scale want AI9 EN12 got AI%d EN%d\n", ai_score, en_score);
        return 1;
      }
      fprintf(stderr, "immigration 584a AI/EN scale ok\n");
    }
    /* 5e52 Brewster branch → FUN_38fd_4884(0,1): tick returns 2, nothing
     * docks, crosses kept; the CHOICE apply moves the pick + zeroes crosses;
     * cancel leaves everything so next turn re-asks. */
    {
      ColonizeCol1Save bcol;
      memset(&bcol, 0, sizeof(bcol));
      for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
        bcol.head.founding_father[i] = -1;
      }
      bcol.head.founding_father[FF_WILLIAM_BREWSTER] = 0;
      col->nation_id = 0;
      eu.dock_count = 0;
      eu.brewster_no_criminals = false;
      europe_tick_immigration_pressure(&eu, &pool, &units, &bcol, 0, NULL);
      eu.current_crosses = (uint16_t)(eu.needed_crosses + 10);
      const uint16_t kept = eu.current_crosses;
      if (europe_tick_immigration_pressure(&eu, &pool, &units, &bcol, 0, NULL) != 2 ||
          eu.dock_count != 0 || eu.current_crosses != kept || !eu.brewster_no_criminals) {
        fprintf(stderr, "brewster tick want 2/no dock/crosses kept\n");
        return 1;
      }
      AiPopupState pops;
      memset(&pops, 0, sizeof(pops));
      units_brewster_enqueue_pick(&eu, &pops, NULL, 0);
      if (pops.queue_count != 1 || pops.queue[0].tag != AI_POPUP_TAG_BREWSTER_PICK ||
          pops.queue[0].choice_count != EUROPE_POOL_SIZE) {
        fprintf(stderr, "brewster pick popup not queued (%d)\n", pops.queue_count);
        return 1;
      }
      pops.has_result = true;
      pops.result_tag = AI_POPUP_TAG_BREWSTER_PICK;
      pops.result_nation_a = 0;
      pops.result_cancelled = true;
      if (!units_brewster_apply_popup(&eu, &pops, &units) || eu.dock_count != 0 ||
          eu.current_crosses != kept) {
        fprintf(stderr, "brewster cancel must keep crosses/dock\n");
        return 1;
      }
      if (units.type_count < 1) {
        snprintf(units.types[0].name, sizeof(units.types[0].name), "Colonists");
        units.types[0].movement = 1;
        units.type_count = 1;
      }
      int before_units = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        before_units += units.units[i].active ? 1 : 0;
      }
      char want[64];
      snprintf(want, sizeof(want), "%s", eu.pool[2].name);
      pops.result_cancelled = false;
      pops.result_choice_id = 2;
      if (!units_brewster_apply_popup(&eu, &pops, &units) || eu.dock_count != 1 ||
          eu.current_crosses != 0 || strcmp(eu.dock[0].name, want) != 0) {
        fprintf(stderr, "brewster pick apply: dock=%d crosses=%u '%s' want '%s'\n",
                eu.dock_count, (unsigned)eu.current_crosses, eu.dock[0].name, want);
        return 1;
      }
      int after_units = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        after_units += units.units[i].active ? 1 : 0;
      }
      if (after_units != before_units + 1) {
        fprintf(stderr, "brewster pick must mirror one Europe-map unit\n");
        return 1;
      }
      fprintf(stderr, "brewster pick-among-pool ok\n");
    }
  }

  /* §C tail = @KINGFRIGATE (ai_king.c), NOT a turn.c spawn: the old
   * duplicate here handed out a free "Merchantman" every 8th turn
   * (bugs.md free_merchanman.SAV; type 0x11 is the Frigate). Assert the
   * turn tick spawns nothing on its own. */
  {
    ColonizeUnitPool units;
    memset(&units, 0, sizeof(units));
    units_reset(&units);
    snprintf(units.types[0x11].name, sizeof(units.types[0x11].name), "Frigate");
    units.types[0x11].domain = COLONIZE_UNIT_DOMAIN_SEA;
    units.types[0x11].movement = 6;
    units.types[0x11].attack = 16;
    units.type_count = 0x12;
    uint16_t year = 1600;
    uint16_t autumn = 0;
    uint32_t turn_number = 8;
    ColonizeTurnContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.human_nation = 0;
    ctx.units = &units;
    ctx.game_year = &year;
    ctx.game_autumn = &autumn;
    ctx.turn_number = &turn_number;
    turn_run_nation_ticks(&ctx, NULL);
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      if (units.units[i].active) {
        fprintf(stderr, "turn tick must not spawn free ships (KINGFRIGATE lives in ai_king)\n");
        return 1;
      }
    }
  }

  /*
   * colony_prod_horse_breed direct unit checks — DOS-confirmed 2026-08-26
   * against real golden_colony_prod01/02 fixtures (COLONY00->01, actual
   * one-turn DOS runs). Two of the 13 exactly-matching colonies from that
   * verification, locked in here as a formula-level regression independent
   * of the full colony/map simulation below.
   */
  {
    /* Fort Nassau (golden_colony_prod01): stock=176 pop=13 food_gross=37
     * wcap=200 no Stable -> potential=ceil(176/50)*2=8, food_avail=37-26=11,
     * food_cap=(11+1)/2=6, capped=min(8,6)=6, headroom=200-176=24,
     * bred=min(6,24)=6 -> stock 176+6=182 (golden expected). */
    ColonyProdHorseBreed b =
      colony_prod_horse_breed(176, 13, 37, 200, false);
    if (b.bred != 6 || b.shortfall != 2) {
      fprintf(
        stderr,
        "colony_prod_horse_breed Fort Nassau want bred=6 shortfall=2 got bred=%d shortfall=%d\n",
        b.bred, b.shortfall
      );
      return 1;
    }
    /* Quebec (golden_colony_prod01): stock=51 pop=5 food_gross=9 wcap=100
     * with Stable -> potential=ceil(51/25)*2=6, food_avail=max(0,9-10)=0,
     * food_cap=0, capped=0, bred=0 -> stock unchanged (golden expected). */
    b = colony_prod_horse_breed(51, 5, 9, 100, true);
    if (b.bred != 0 || b.shortfall != 6) {
      fprintf(
        stderr,
        "colony_prod_horse_breed Quebec want bred=0 shortfall=6 got bred=%d shortfall=%d\n",
        b.bred, b.shortfall
      );
      return 1;
    }
    /* horses < 2 -> no growth at all, regardless of food/warehouse room. */
    b = colony_prod_horse_breed(1, 1, 50, 100, true);
    if (b.bred != 0 || b.shortfall != 0) {
      fprintf(
        stderr,
        "colony_prod_horse_breed <2 horses want bred=0 shortfall=0 got bred=%d shortfall=%d\n",
        b.bred, b.shortfall
      );
      return 1;
    }
    /* Warehouse headroom binds even when food would allow more. */
    b = colony_prod_horse_breed(99, 0, 100, 100, true);
    if (b.bred != 1) {
      fprintf(
        stderr,
        "colony_prod_horse_breed warehouse-headroom want bred=1 got bred=%d\n",
        b.bred
      );
      return 1;
    }
    fprintf(stderr, "colony_prod_horse_breed direct ok\n");
  }

  /*
   * Horse breed: ≥2 horses + food surplus → +horses, −food; Stable raises cap.
   */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Stable");
    pool.building_type_count = 1;

    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->nation_id = 0;
    col->building_in_production = -1;
    col->has_building[0] = true;
    /* One farmer producing will need map — use stock food surplus stand-in via
     * free production with no field yield: inject food_surplus by pre-stocking
     * food high and zero pop consume… pop=1 eats 2; set field via craft skip.
     * Use turn_run with no map → field_food=0 → no breed. Force via direct
     * stock path: pop 0 invalid. Instead assign no colonists but population 0
     * skips eat — horses≥2 + we need food_surplus_turn>0 from field.
     */
    col->stock[COLONIZE_CARGO_HORSES] = 2;
    col->stock[COLONIZE_CARGO_FOOD] = 20;
    col->colonists[0].active = true;
    col->colonists[0].profession = COLONIZE_PROF_FREE_COLONIST;
    col->colonists[0].field_job = COLONIZE_JOB_FARMER;
    col->colonists[0].building_type = -1;
    col->colonist_count = 1;
    col->population = 1;
    col->tiles[0] = 0;
    pool.colony_count = 1;

    /* Without map, field_food=0 → surplus negative → no breed. Add map plains. */
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    char err[64];
    if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
      fprintf(stderr, "breed: map_alloc %s\n", err);
      return 1;
    }
    for (int i = 0; i < 64; ++i) {
      map.terrain[i] = 1; /* plains */
    }
    col->x = 3;
    col->y = 3;

    /* Preview must show the same breeding the EOT tick is about to do. */
    ColonizeColonyPreview prev;
    colony_preview_compute(&pool, col, &map, NULL, &prev);

    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    const int h0 = col->stock[COLONIZE_CARGO_HORSES];
    turn_run_colony_production(&pool, &map, NULL, NULL, 0, &prod, NULL, NULL, NULL);
    const int bred = col->stock[COLONIZE_CARGO_HORSES] - h0;
    if (bred <= 0) {
      fprintf(
        stderr,
        "breed: horses %d→%d want increase (food=%d)\n",
        h0,
        col->stock[COLONIZE_CARGO_HORSES],
        col->stock[COLONIZE_CARGO_FOOD]
      );
      map_free(&map);
      return 1;
    }
    if (prev.goods[COLONIZE_CARGO_HORSES] != bred) {
      fprintf(
        stderr,
        "breed preview mismatch want %d got %d\n",
        bred,
        prev.goods[COLONIZE_CARGO_HORSES]
      );
      map_free(&map);
      return 1;
    }
    map_free(&map);
    fprintf(stderr, "colony horse breed ok\n");
  }

  /*
   * FUN_3844_0442 B: year≥1600, no human colonies, peacetime → defeat.
   */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    uint16_t year = 1600;
    uint16_t autumn = 0;
    uint32_t turn_number = 200;
    char status[64];
    ColonizeTurnContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.game_year = &year;
    ctx.game_autumn = &autumn;
    ctx.turn_number = &turn_number;
    ctx.human_nation = 0;
    ctx.colonies = &pool;
    ctx.status = status;
    ctx.status_size = sizeof(status);

    ColonizeTurnResult out;
    memset(&out, 0, sizeof(out));
    turn_run_year_end_chrome(&ctx, &out);
    if (!out.year_end_defeat || strstr(status, "Defeat") == NULL) {
      fprintf(stderr, "year-end defeat want latch+status got defeat=%d '%s'\n", out.year_end_defeat, status);
      return 1;
    }
    /* WoI skips defeat. */
    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.head.game_options.woi = 1;
    ctx.col1 = &col1;
    ctx.col1_ok = true;
    memset(&out, 0, sizeof(out));
    status[0] = '\0';
    turn_run_year_end_chrome(&ctx, &out);
    if (out.year_end_defeat) {
      fprintf(stderr, "year-end defeat must skip during WoI\n");
      return 1;
    }
    /* Same fixture: WoI + no crown colonies → C1 victory. */
    if (!out.year_end_victory || ai_king_latch_get(&col1, 4) != 1 ||
        col1.head.show_entire_map != 1 || strstr(status, "Victory") == NULL) {
      fprintf(
        stderr,
        "year-end victory want latch+map+status got victory=%d u46[4]=%u map=%u '%s'\n",
        out.year_end_victory,
        (unsigned)ai_king_latch_get(&col1, 4),
        (unsigned)col1.head.show_entire_map,
        status
      );
      return 1;
    }
    fprintf(stderr, "year-end defeat chrome ok\n");
    fprintf(stderr, "year-end victory chrome ok\n");
    /* calendar_latch ("scoring complete") is set by the retire-score chain,
     * not the victory latch (bugs.md 265) — the WON latch alone must both
     * leave it clear and still suppress the anniversary chrome. */
    if (col1.head.game_options.calendar_latch) {
      fprintf(stderr, "year-end victory must NOT set calendar_latch (scoring pending)\n");
      return 1;
    }
    /* Endgame latch suppresses further E anniversary while campaign stopped. */
    year = 1790;
    status[0] = '\0';
    memset(&out, 0, sizeof(out));
    turn_run_year_end_chrome(&ctx, &out);
    if (strstr(status, "Anniversary") != NULL) {
      fprintf(stderr, "calendar_latch must suppress anniversary got '%s'\n", status);
      return 1;
    }
    fprintf(stderr, "year-end calendar_latch ok\n");

    /* C1 REF pool fat blocks victory; independence_force bypasses. */
    {
      ColonizeCol1Save fat;
      memset(&fat, 0, sizeof(fat));
      fat.head.game_options.woi = 1;
      fat.head.expeditionary_force[0] = 8; /* regulars → score ≥4 */
      fat.head.expeditionary_force[1] = 4;
      fat.head.expeditionary_force[3] = 2;
      ctx.col1 = &fat;
      ctx.col1_ok = true;
      status[0] = '\0';
      memset(&out, 0, sizeof(out));
      turn_run_year_end_chrome(&ctx, &out);
      if (out.year_end_victory) {
        fprintf(stderr, "year-end C1 REF-fat must block victory\n");
        return 1;
      }
      fat.head.game_options.independence_force = 1;
      status[0] = '\0';
      memset(&out, 0, sizeof(out));
      turn_run_year_end_chrome(&ctx, &out);
      if (!out.year_end_victory || strstr(status, "Victory") == NULL) {
        fprintf(
          stderr,
          "year-end C1 force bypass want victory got %d '%s'\n",
          out.year_end_victory,
          status
        );
        return 1;
      }
      fprintf(stderr, "year-end C1 REF pool gate ok\n");
    }
    year = 1790;
    status[0] = '\0';
    memset(&out, 0, sizeof(out));
    ctx.col1_ok = false;
    ctx.col1 = NULL;
    /* Give human a colony so defeat B does not fire. */
    ColonizeColony* c = &pool.colonies[0];
    memset(c, 0, sizeof(*c));
    c->active = true;
    c->nation_id = 0;
    c->building_in_production = -1;
    pool.colony_count = 1;
    turn_run_year_end_chrome(&ctx, &out);
    if (out.year_end_defeat || strstr(status, "Anniversary") == NULL) {
      fprintf(stderr, "anniversary want status got defeat=%d '%s'\n", out.year_end_defeat, status);
      return 1;
    }
    fprintf(stderr, "year-end anniversary ok\n");

    /* E game-era 1800 with richest colony name. */
    year = 1800;
    status[0] = '\0';
    memset(&out, 0, sizeof(out));
    snprintf(c->name, sizeof(c->name), "Jamestown");
    c->colonist_count = 5;
    turn_run_year_end_chrome(&ctx, &out);
    if (strstr(status, "Game era") == NULL || strstr(status, "Jamestown") == NULL) {
      fprintf(stderr, "game-era want Jamestown status got '%s'\n", status);
      return 1;
    }
    fprintf(stderr, "year-end game-era richest ok\n");

    /* C2: WoI + crown colonies + high crown SoL → peace offer status. */
    year = 1700;
    status[0] = '\0';
    memset(&out, 0, sizeof(out));
    ColonizeCol1Save c2;
    col1_save_init(&c2);
    c2.head.game_options.woi = 1; /* WoI */
    c2.head.colony_count = 2;
    c2.colony = calloc(2, sizeof(ColonizeCol1Colony));
    if (!c2.colony) {
      return 1;
    }
    c2.colony[0].nation_id = 0;
    c2.colony[0].population = 1;
    c2.colony[0].rebel_dividend = 5;
    c2.colony[0].rebel_divisor = 100;
    c2.colony[1].nation_id = 1;
    c2.colony[1].population = 1;
    c2.colony[1].rebel_dividend = 90;
    c2.colony[1].rebel_divisor = 100;
    ColonizeColony* crown_c2 = &pool.colonies[1];
    memset(crown_c2, 0, sizeof(*crown_c2));
    crown_c2->active = true;
    crown_c2->nation_id = 1;
    crown_c2->building_in_production = -1;
    if (pool.colony_count < 2) {
      pool.colony_count = 2;
    }
    ctx.col1 = &c2;
    ctx.col1_ok = true;
    ctx.colonies = &pool;
    turn_run_year_end_chrome(&ctx, &out);
    if (out.year_end_victory || strstr(status, "peace") == NULL) {
      fprintf(
        stderr,
        "year-end C2 want peace status got victory=%d '%s'\n",
        out.year_end_victory,
        status
      );
      free(c2.colony);
      return 1;
    }
    free(c2.colony);
    ctx.col1 = NULL;
    ctx.col1_ok = false;
    fprintf(stderr, "year-end C2 peace ok\n");

    /* Section D: rival_nation_slot + threshold + rebellion_pct dedup. */
    year = 1700;
    status[0] = '\0';
    memset(&out, 0, sizeof(out));
    ColonizeCol1Save dcol;
    col1_save_init(&dcol);
    dcol.head.difficulty = 3;
    dcol.player[0].control = 0;
    dcol.player[1].control = 1;
    dcol.player[2].control = 1;
    dcol.head.rival_nation_slot_1 = 2;
    dcol.head.colony_count = 1;
    dcol.colony = calloc(1, sizeof(ColonizeCol1Colony));
    if (!dcol.colony) {
      return 1;
    }
    dcol.colony[0].nation_id = 2;
    dcol.colony[0].population = 8;
    dcol.colony[0].rebel_dividend = 50;
    dcol.colony[0].rebel_divisor = 100;
    dcol.nation[2].rebel_sentiment = 55;
    ctx.col1 = &dcol;
    ctx.col1_ok = true;
    turn_run_year_end_chrome(&ctx, &out);
    if (strstr(status, "declares war") == NULL) {
      fprintf(stderr, "year-end D want auto-declare at SoL>=50 got '%s'\n", status);
      free(dcol.colony);
      return 1;
    }
    dcol.head.difficulty = 4;
    dcol.colony[0].rebel_dividend = 25;
    dcol.nation[2].rebel_sentiment = 25;
    dcol.nation[2].rebellion_pct_last_notified = 10;
    status[0] = '\0';
    memset(&out, 0, sizeof(out));
    turn_run_year_end_chrome(&ctx, &out);
    if (strstr(status, "Rival SoL rising") == NULL) {
      fprintf(stderr, "year-end D want rising got '%s'\n", status);
      free(dcol.colony);
      return 1;
    }
    if (dcol.nation[2].rebellion_pct_last_notified != 25) {
      fprintf(
        stderr,
        "year-end D latch want 25 got %u\n",
        (unsigned)dcol.nation[2].rebellion_pct_last_notified
      );
      free(dcol.colony);
      return 1;
    }
    free(dcol.colony);
    ctx.col1 = NULL;
    ctx.col1_ok = false;
    fprintf(stderr, "year-end rival SoL ok\n");

    /* D auto-declare via rival slot + colony SoL (not crown nation 1). */
    year = 1700;
    status[0] = '\0';
    memset(&out, 0, sizeof(out));
    ColonizeCol1Save dw;
    col1_save_init(&dw);
    dw.head.difficulty = 3;
    dw.player[0].control = 0;
    dw.player[1].control = 1;
    dw.player[2].control = 1;
    dw.head.rival_nation_slot_1 = 2;
    dw.head.colony_count = 1;
    dw.colony = calloc(1, sizeof(ColonizeCol1Colony));
    if (!dw.colony) {
      return 1;
    }
    dw.colony[0].nation_id = 2;
    dw.colony[0].population = 5;
    dw.colony[0].rebel_dividend = 55;
    dw.colony[0].rebel_divisor = 100;
    ctx.col1 = &dw;
    ctx.col1_ok = true;
    turn_run_year_end_chrome(&ctx, &out);
    if (strstr(status, "declares war") == NULL || !ai_diplo_at_war(&dw, 2, 0)) {
      fprintf(
        stderr,
        "year-end D auto-declare want war status got '%s' at_war=%d\n",
        status,
        ai_diplo_at_war(&dw, 2, 0)
      );
      free(dw.colony);
      return 1;
    }
    free(dw.colony);
    ctx.col1 = NULL;
    ctx.col1_ok = false;
    fprintf(stderr, "year-end D auto-declare ok\n");

    /*
     * D rival slots: slot_1 stays valid and quiet (no war/rising/falling),
     * so the loop must fall through to slot_2 — and if slot_2's nation was
     * defeated meanwhile, ensure_rival_slots must refresh it rather than
     * leaving the stale eliminated nation id in place forever.
     */
    year = 1700;
    status[0] = '\0';
    memset(&out, 0, sizeof(out));
    ColonizeCol1Save ds;
    col1_save_init(&ds);
    ds.head.difficulty = 3; /* thresh = (8-3)*10 = 50 */
    ds.player[0].control = 0;
    ds.player[1].control = 1;
    ds.player[2].control = 1;
    ds.player[3].control = 2; /* already defeated */
    ds.head.rival_nation_slot_1 = 2;
    ds.head.rival_nation_slot_2 = 3; /* stale: nation 3 is gone */
    ds.nation[2].rebel_sentiment = 30;
    ds.nation[2].rebellion_pct_last_notified = 30; /* == SoL: no message */
    ctx.col1 = &ds;
    ctx.col1_ok = true;
    turn_run_year_end_chrome(&ctx, &out);
    if (ds.head.rival_nation_slot_2 == 3) {
      fprintf(
        stderr,
        "year-end D want slot_2 refreshed off defeated nation 3, still 3\n"
      );
      return 1;
    }
    if (ds.head.rival_nation_slot_1 != 2) {
      fprintf(
        stderr,
        "year-end D refresh should not disturb still-valid slot_1, got %d\n",
        (int)ds.head.rival_nation_slot_1
      );
      return 1;
    }
    ctx.col1 = NULL;
    ctx.col1_ok = false;
    fprintf(stderr, "year-end D stale slot_2 refresh ok\n");
  }

  /*
   * FUN_4962_0606 profession tally from colonists + units.
   */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->nation_id = 0;
    col->building_in_production = -1;
    col->colonists[0].active = true;
    col->colonists[0].profession = COLONIZE_PROF_STATESMAN;
    col->colonists[1].active = true;
    col->colonists[1].profession = COLONIZE_PROF_FREE_COLONIST;
    col->colonists[1].field_job = COLONIZE_JOB_FARMER;
    col->colonist_count = 2;
    col->population = 2;
    pool.colony_count = 1;

    ColonizeUnitPool units;
    memset(&units, 0, sizeof(units));
    units_reset(&units);
    units.type_count = 1;
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Scout");
    const int uid = units_spawn(&units, 0, 1, 1);
    ColonizeUnit* u = units_get(&units, uid);
    if (u) {
      units_set_nation(u, 0);
      u->profession = COLONIZE_PROF_STATESMAN;
    }

    uint8_t hist[32];
    turn_tally_professions(&pool, &units, 0, hist);
    if (hist[COLONIZE_PROF_STATESMAN] != 2) {
      fprintf(stderr, "tally statesman want 2 got %u\n", (unsigned)hist[COLONIZE_PROF_STATESMAN]);
      return 1;
    }
    if (hist[COLONIZE_PROF_FREE_COLONIST] != 1) {
      fprintf(stderr, "tally free want 1 got %u\n", (unsigned)hist[COLONIZE_PROF_FREE_COLONIST]);
      return 1;
    }
    fprintf(stderr, "profession tally ok\n");
  }

  /* Live census peel: colony counts + unit/combat tallies. */
  {
    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.stuff.colony_counts[0] = 99; /* stale */
    col1.stuff.land_combat_strength[0] = 999; /* stale */
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeColony* c = &pool.colonies[0];
    memset(c, 0, sizeof(*c));
    c->active = true;
    c->nation_id = 0;
    c->colonist_count = 3;
    pool.colony_count = 1;

    ColonizeUnitPool units;
    memset(&units, 0, sizeof(units));
    units_reset(&units);
    units.type_count = 2;
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Colonists");
    units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    snprintf(units.types[1].name, sizeof(units.types[1].name), "Soldiers");
    units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
    units.types[1].attack = 2;
    units.types[1].defense = 2;
    const int sid = units_spawn(&units, 1, 2, 2);
    ColonizeUnit* su = units_get(&units, sid);
    if (su) {
      units_set_nation(su, 0);
    }

    col1_stuff_census_refresh_colony_counts(&col1.stuff, &pool, &units);
    if (col1.stuff.colony_counts[0] != 1 || col1.stuff.colony_pop_totals[0] != 3) {
      fprintf(
        stderr,
        "census refresh counts=%u pop=%u want 1/3\n",
        (unsigned)col1.stuff.colony_counts[0],
        (unsigned)col1.stuff.colony_pop_totals[0]
      );
      return 1;
    }
    if (col1.stuff.all_unit_counts[0] != 1 || col1.stuff.land_combat_strength[0] != 4 ||
        col1.stuff.unit_type_counts[0][1] != 1) {
      fprintf(
        stderr,
        "census unit tallies units=%u str=%u type1=%u want 1/4/1\n",
        (unsigned)col1.stuff.all_unit_counts[0],
        (unsigned)col1.stuff.land_combat_strength[0],
        (unsigned)col1.stuff.unit_type_counts[0][1]
      );
      return 1;
    }
    fprintf(stderr, "census colony_counts refresh ok\n");
  }

  /*
   * Phase P: human warehouse spoilage → Europe status + SPOIL1–4 section pick.
   */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->nation_id = 0;
    col->building_in_production = -1;
    snprintf(col->name, sizeof(col->name), "Roanoke");
    col->warehouse_level = 0;
    col->stock[COLONIZE_CARGO_TOBACCO] = 150;
    col->stock[COLONIZE_CARGO_FOOD] = 20;
    col->colonists[0].active = true;
    col->colonist_count = 1;
    col->population = 1;
    pool.colony_count = 1;

    EuropeScreen eu;
    memset(&eu, 0, sizeof(eu));
    eu.cargo_count = COLONIZE_CARGO_COUNT;
    for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
      eu.cargo[i].bid = 1;
    }
    snprintf(eu.cargo[COLONIZE_CARGO_TOBACCO].name, sizeof(eu.cargo[0].name), "Tobacco");
    AiPopupState pops;
    ai_popup_init(&pops);
    ColonizeMsgCatalog game_txt;
    assets_msg_init(&game_txt);
    (void)assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT");

    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, NULL, &eu, 0, &prod, &pops, &game_txt, NULL);
    if (col->stock[COLONIZE_CARGO_TOBACCO] != 100) {
      fprintf(stderr, "spoilage clamp tobacco=%d want 100\n", col->stock[COLONIZE_CARGO_TOBACCO]);
      assets_msg_free(&game_txt);
      return 1;
    }
    if (strstr(eu.status, "spoiled") == NULL || strstr(eu.status, "Tobacco") == NULL) {
      fprintf(stderr, "spoilage status want Tobacco spoiled got '%s'\n", eu.status);
      assets_msg_free(&game_txt);
      return 1;
    }
    if (pops.queue_count < 1) {
      fprintf(stderr, "spoilage: expected SPOIL1 popup\n");
      assets_msg_free(&game_txt);
      return 1;
    }
    if (strstr(pops.queue[0].body, "Roanoke") == NULL ||
        (strstr(pops.queue[0].body, "Tobacco") == NULL &&
         strstr(pops.queue[0].body, "thrown away") == NULL) ||
        strstr(pops.queue[0].body, "warehouse") == NULL) {
      fprintf(stderr, "spoilage SPOIL1 body weak: '%s'\n", pops.queue[0].body);
      assets_msg_free(&game_txt);
      return 1;
    }
    assets_msg_free(&game_txt);
    fprintf(stderr, "warehouse spoilage status ok\n");
  }

  /* Phase P: multi-type spoil → @SPOIL2. */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->nation_id = 0;
    col->building_in_production = -1;
    snprintf(col->name, sizeof(col->name), "Roanoke");
    col->warehouse_level = 0;
    col->stock[COLONIZE_CARGO_TOBACCO] = 150;
    col->stock[COLONIZE_CARGO_SUGAR] = 140;
    col->stock[COLONIZE_CARGO_FOOD] = 20;
    col->colonists[0].active = true;
    col->colonist_count = 1;
    col->population = 1;
    pool.colony_count = 1;

    EuropeScreen eu;
    memset(&eu, 0, sizeof(eu));
    eu.cargo_count = COLONIZE_CARGO_COUNT;
    for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
      eu.cargo[i].bid = 1;
    }
    snprintf(eu.cargo[COLONIZE_CARGO_TOBACCO].name, sizeof(eu.cargo[0].name), "Tobacco");
    snprintf(eu.cargo[COLONIZE_CARGO_SUGAR].name, sizeof(eu.cargo[0].name), "Sugar");
    AiPopupState pops;
    ai_popup_init(&pops);
    ColonizeMsgCatalog game_txt;
    assets_msg_init(&game_txt);
    (void)assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT");

    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, NULL, &eu, 0, &prod, &pops, &game_txt, NULL);
    if (pops.queue_count < 1 || strstr(pops.queue[0].body, "Some of our cargo") == NULL) {
      fprintf(
        stderr,
        "spoilage SPOIL2 want 'Some of our cargo' q=%d body='%s'\n",
        pops.queue_count,
        pops.queue_count > 0 ? pops.queue[0].body : ""
      );
      assets_msg_free(&game_txt);
      return 1;
    }
    if (strstr(pops.queue[0].body, "warehouse") == NULL) {
      fprintf(stderr, "spoilage SPOIL2 want warehouse tip got '%s'\n", pops.queue[0].body);
      assets_msg_free(&game_txt);
      return 1;
    }
    assets_msg_free(&game_txt);
    fprintf(stderr, "warehouse spoilage SPOIL2 ok\n");
  }

  /* Phase P: expanded warehouse single → @SPOIL3 (no larger-warehouse tip). */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->nation_id = 0;
    col->building_in_production = -1;
    snprintf(col->name, sizeof(col->name), "Roanoke");
    col->warehouse_level = 2; /* cap 300 */
    col->stock[COLONIZE_CARGO_TOBACCO] = 350;
    col->stock[COLONIZE_CARGO_FOOD] = 20;
    col->colonists[0].active = true;
    col->colonist_count = 1;
    col->population = 1;
    pool.colony_count = 1;

    EuropeScreen eu;
    memset(&eu, 0, sizeof(eu));
    eu.cargo_count = COLONIZE_CARGO_COUNT;
    for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
      eu.cargo[i].bid = 1;
    }
    snprintf(eu.cargo[COLONIZE_CARGO_TOBACCO].name, sizeof(eu.cargo[0].name), "Tobacco");
    AiPopupState pops;
    ai_popup_init(&pops);
    ColonizeMsgCatalog game_txt;
    assets_msg_init(&game_txt);
    (void)assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT");

    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, NULL, &eu, 0, &prod, &pops, &game_txt, NULL);
    if (col->stock[COLONIZE_CARGO_TOBACCO] != 300) {
      fprintf(stderr, "spoilage SPOIL3 clamp tobacco=%d want 300\n", col->stock[COLONIZE_CARGO_TOBACCO]);
      assets_msg_free(&game_txt);
      return 1;
    }
    if (pops.queue_count < 1) {
      fprintf(stderr, "spoilage: expected SPOIL3 popup\n");
      assets_msg_free(&game_txt);
      return 1;
    }
    if (strstr(pops.queue[0].body, "Tobacco") == NULL ||
        strstr(pops.queue[0].body, "thrown away") == NULL) {
      fprintf(stderr, "spoilage SPOIL3 body weak: '%s'\n", pops.queue[0].body);
      assets_msg_free(&game_txt);
      return 1;
    }
    if (strstr(pops.queue[0].body, "larger") != NULL) {
      fprintf(stderr, "spoilage SPOIL3 must omit warehouse tip got '%s'\n", pops.queue[0].body);
      assets_msg_free(&game_txt);
      return 1;
    }
    assets_msg_free(&game_txt);
    fprintf(stderr, "warehouse spoilage SPOIL3 ok\n");
  }

  if (unit_century_cargoready() != 0) {
    return 1;
  }

  if (unit_eot_fog_reveal() != 0) {
    return 1;
  }

  /*
   * Ship-build ready (00f2): types 0x0d..0x12 + bit7; +1/+2 turns_worked;
   * clear bit7 at threshold (type.defense = DOS 0x5235 / NAMES combat).
   */
  {
    ColonizeUnitPool units;
    memset(&units, 0, sizeof(units));
    units_reset(&units);
    snprintf(units.types[0xd].name, sizeof(units.types[0xd].name), "Caravel");
    units.types[0xd].movement = 4;
    units.types[0xd].defense = 4; /* NAMES combat stand-in (Caravel real=2) */
    units.types[0xd].domain = COLONIZE_UNIT_DOMAIN_SEA;
    units.type_count = 0x0e;

    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    ColonizeColony* col = &colonies.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->x = 10;
    col->y = 10;
    col->nation_id = 0;

    const int id = units_spawn_allow_stack(&units, 0x0d, 10, 10);
    ColonizeUnit* u = units_get(&units, id);
    if (!u) {
      fprintf(stderr, "ship-build spawn failed\n");
      return 1;
    }
    units_set_nation(u, 0);
    u->col1_unknown15 = 0x80;
    u->turns_worked = 0;

    char status[128];
    status[0] = '\0';
    int want_eu = 0;
    /* On colony: +2/tick → need 2 ticks to reach threshold 4. */
    (void)units_tick_ship_build_ready(&units, &colonies, 0, 0, status, sizeof(status), &want_eu);
    if ((u->col1_unknown15 & 0x80u) == 0 || u->turns_worked != 2) {
      fprintf(
        stderr,
        "ship-build mid: bit=%u tw=%d want bit set tw=2\n",
        (unsigned)(u->col1_unknown15 & 0x80u),
        u->turns_worked
      );
      return 1;
    }
    (void)units_tick_ship_build_ready(&units, &colonies, 0, 0, status, sizeof(status), &want_eu);
    if ((u->col1_unknown15 & 0x80u) != 0 || u->turns_worked < 4) {
      fprintf(
        stderr,
        "ship-build done: bit=%u tw=%d want clear tw≥4\n",
        (unsigned)(u->col1_unknown15 & 0x80u),
        u->turns_worked
      );
      return 1;
    }
    if (strstr(status, "complete") == NULL) {
      fprintf(stderr, "ship-build status want complete got '%s'\n", status);
      return 1;
    }
    if (want_eu != 0) {
      fprintf(stderr, "ship-build on colony should not request Europe\n");
      return 1;
    }
    /* Real Caravel combat=2 completes in one colony tick. */
    u->col1_unknown15 = 0x80;
    u->turns_worked = 0;
    units.types[0xd].defense = 2;
    status[0] = '\0';
    (void)units_tick_ship_build_ready(&units, &colonies, 0, 0, status, sizeof(status), &want_eu);
    if ((u->col1_unknown15 & 0x80u) != 0 || u->turns_worked != 2) {
      fprintf(
        stderr,
        "ship-build combat2: bit=%u tw=%d want clear tw=2\n",
        (unsigned)(u->col1_unknown15 & 0x80u),
        u->turns_worked
      );
      return 1;
    }
    fprintf(stderr, "ship-build ready ok\n");
  }

  /* Phase K thin: project queued, no hammers → Europe status. */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->nation_id = 0;
    col->building_in_production = 0; /* any queued project */
    col->colonists[0].active = true;
    col->colonists[0].profession = UNITS_JOB_COLONIST;
    col->colonist_count = 1;
    col->population = 1;
    col->stock[COLONIZE_CARGO_FOOD] = 20;

    EuropeScreen eu;
    memset(&eu, 0, sizeof(eu));
    eu.cargo_count = COLONIZE_CARGO_COUNT;
    for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
      eu.cargo[i].bid = 1;
    }

    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, NULL, &eu, 0, &prod, NULL, NULL, NULL);
    if (strstr(eu.status, "hammers") == NULL) {
      fprintf(stderr, "build advisory K want hammers status got '%s'\n", eu.status);
      return 1;
    }
    fprintf(stderr, "build advisory K ok\n");

    /* K tools crumb: hammers banked + this-tick carpenter flow, tools short. */
    pool.building_type_count = 2;
    snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Carpenter's Shop");
    pool.building_types[0].hammers = 0;
    pool.building_types[0].tools_cost = 0;
    snprintf(pool.building_types[1].name, sizeof(pool.building_types[1].name), "Printing Press");
    pool.building_types[1].hammers = 10;
    pool.building_types[1].tools_cost = 4;
    col->building_in_production = 1;
    col->has_building[0] = true;
    col->has_building[1] = false;
    col->hammers = 10;
    col->stock[COLONIZE_CARGO_TOOLS] = 0;
    col->stock[COLONIZE_CARGO_LUMBER] = 20;
    col->stock[COLONIZE_CARGO_FOOD] = 50; /* avoid Food shortage overwriting */
    col->colonists[0].building_type = 0;
    col->colonists[0].profession = COLONIZE_PROF_CARPENTER;
    eu.status[0] = '\0';
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, NULL, &eu, 0, &prod, NULL, NULL, NULL);
    if (strstr(eu.status, "tools") == NULL) {
      fprintf(stderr, "build advisory K tools want status got '%s'\n", eu.status);
      return 1;
    }
    fprintf(stderr, "build advisory K tools ok\n");

    /* 5384&0x20 set → suppress hammers K. */
    ColonizeCol1Save kcol;
    memset(&kcol, 0, sizeof(kcol));
    kcol.head.colony_report_options.report_raw_materials_shortages = 1;
    col->building_in_production = 0;
    col->stock[COLONIZE_CARGO_LUMBER] = 0;
    col->colonists[0].building_type = -1;
    col->colonists[0].profession = UNITS_JOB_COLONIST;
    eu.status[0] = '\0';
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, &kcol, &eu, 0, &prod, NULL, NULL, NULL);
    if (strstr(eu.status, "hammers") != NULL) {
      fprintf(stderr, "5384 gate want suppress hammers got '%s'\n", eu.status);
      return 1;
    }
    /* K craft crumbs: Weaver with empty cotton (suppress food-shortage crumb).
     * 2026-08-24 fix: the K "ran out of X" gate now uses actual craft demand
     * (colony_craft_demand_mask — someone staffed producing a positive
     * tier-scaled input requirement), not "the building exists by name",
     * matching DOS FUN_15eb_0bd4/0b96's demand scratch word — so the
     * colonist must actually be assigned to the Weaver's House for the
     * @COTTON crumb to fire (an unstaffed building has zero demand in DOS,
     * and used to incorrectly nag "Need cotton." regardless of staffing). */
    ColonizeCol1Save cloth_col;
    memset(&cloth_col, 0, sizeof(cloth_col));
    cloth_col.head.colony_report_options.report_food_shortages = 1;
    snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Weaver's House");
    col->has_building[0] = true;
    col->building_in_production = -1;
    col->stock[COLONIZE_CARGO_COTTON] = 0;
    col->stock[COLONIZE_CARGO_SUGAR] = 5;
    col->stock[COLONIZE_CARGO_TOBACCO] = 5;
    col->stock[COLONIZE_CARGO_FURS] = 5;
    col->stock[COLONIZE_CARGO_FOOD] = 20;
    col->stock[COLONIZE_CARGO_LUMBER] = 5;
    col->stock[COLONIZE_CARGO_ORE] = 5;
    col->colonists[0].building_type = 0;
    col->colonists[0].profession = COLONIZE_PROF_WEAVER;
    eu.status[0] = '\0';
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, &cloth_col, &eu, 0, &prod, NULL, NULL, NULL);
    if (strstr(eu.status, "cotton") == NULL) {
      fprintf(stderr, "build advisory K cotton want status got '%s'\n", eu.status);
      return 1;
    }
    fprintf(stderr, "build advisory K cotton ok\n");

    /* Phase K @LUMBER / @ORE / @TOOLS chrome. */
    ColonizeMsgCatalog game_txt;
    assets_msg_init(&game_txt);
    (void)assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT");
    AiPopupState pops;
    ai_popup_init(&pops);
    ColonizeCol1Save food_gate;
    memset(&food_gate, 0, sizeof(food_gate));
    food_gate.head.colony_report_options.report_food_shortages = 1;

    snprintf(col->name, sizeof(col->name), "Boston");
    snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Carpenter's Shop");
    col->has_building[0] = true;
    /* Invent +1 lumber then carpenter hammers burn it so stock ends at 0. */
    col->building_in_production = 0;
    col->hammers = 0;
    col->stock[COLONIZE_CARGO_LUMBER] = 0;
    col->stock[COLONIZE_CARGO_ORE] = 5;
    col->stock[COLONIZE_CARGO_FOOD] = 40;
    col->stock[COLONIZE_CARGO_TOOLS] = 5;
    col->stock[COLONIZE_CARGO_MUSKETS] = 5;
    col->colonists[0].building_type = 0;
    col->colonists[0].profession = COLONIZE_PROF_CARPENTER;
    eu.status[0] = '\0';
    ai_popup_clear(&pops);
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, &food_gate, &eu, 0, &prod, &pops, &game_txt, NULL);
    if (strstr(eu.status, "lumber") == NULL) {
      fprintf(stderr, "K LUMBER status want lumber got '%s'\n", eu.status);
      assets_msg_free(&game_txt);
      return 1;
    }
    if (pops.queue_count < 1 ||
        (strstr(pops.queue[0].body, "lumber") == NULL &&
         strstr(pops.queue[0].body, "Boston") == NULL)) {
      fprintf(
        stderr,
        "K LUMBER popup weak q=%d body='%s'\n",
        pops.queue_count,
        pops.queue_count > 0 ? pops.queue[0].body : ""
      );
      assets_msg_free(&game_txt);
      return 1;
    }
    fprintf(stderr, "Phase K LUMBER chrome ok\n");

    /* Phase K LUMBER negative: unstaffed Carpenter's Shop + 0 lumber must NOT
     * nag "Need lumber." — 2026-08-24 fix applies the same real-staffed-
     * demand gate (colony_prod_colony_hammers' out_lumber_use) already used
     * for the other five K goods above; lumber isn't a colony_craft.c recipe
     * so it can't reuse colony_craft_demand_mask directly, but the principle
     * (an unstaffed building has zero demand in DOS) is the same. */
    col->building_in_production = -1;
    col->colonists[0].building_type = -1;
    col->colonists[0].profession = UNITS_JOB_COLONIST;
    col->stock[COLONIZE_CARGO_LUMBER] = 0;
    col->stock[COLONIZE_CARGO_ORE] = 5;
    col->stock[COLONIZE_CARGO_SUGAR] = 5;
    col->stock[COLONIZE_CARGO_TOBACCO] = 5;
    col->stock[COLONIZE_CARGO_COTTON] = 5;
    col->stock[COLONIZE_CARGO_FURS] = 5;
    col->stock[COLONIZE_CARGO_TOOLS] = 5;
    col->stock[COLONIZE_CARGO_MUSKETS] = 5;
    col->stock[COLONIZE_CARGO_FOOD] = 40;
    eu.status[0] = '\0';
    ai_popup_clear(&pops);
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, &food_gate, &eu, 0, &prod, &pops, &game_txt, NULL);
    if (strstr(eu.status, "lumber") != NULL) {
      fprintf(stderr, "K LUMBER unstaffed want silence got '%s'\n", eu.status);
      assets_msg_free(&game_txt);
      return 1;
    }
    fprintf(stderr, "Phase K LUMBER unstaffed-silence ok\n");

    snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Blacksmith's House");
    col->building_in_production = -1;
    /* Stays staffed (building_type=0) — 2026-08-24: K's "ran out of X" gate
     * needs real craft demand now, not just the building existing. */
    col->colonists[0].building_type = 0;
    col->colonists[0].profession = COLONIZE_PROF_BLACKSMITH;
    col->stock[COLONIZE_CARGO_LUMBER] = 5;
    col->stock[COLONIZE_CARGO_ORE] = 0;
    eu.status[0] = '\0';
    ai_popup_clear(&pops);
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, &food_gate, &eu, 0, &prod, &pops, &game_txt, NULL);
    if (strstr(eu.status, "ore") == NULL) {
      fprintf(stderr, "K ORE status want ore got '%s'\n", eu.status);
      assets_msg_free(&game_txt);
      return 1;
    }
    if (pops.queue_count < 1 ||
        (strstr(pops.queue[0].body, "ore") == NULL && strstr(pops.queue[0].body, "Boston") == NULL)) {
      fprintf(
        stderr,
        "K ORE popup weak q=%d body='%s'\n",
        pops.queue_count,
        pops.queue_count > 0 ? pops.queue[0].body : ""
      );
      assets_msg_free(&game_txt);
      return 1;
    }
    fprintf(stderr, "Phase K ORE chrome ok\n");

    snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Armory");
    col->colonists[0].profession = COLONIZE_PROF_GUNSMITH;
    col->stock[COLONIZE_CARGO_ORE] = 5;
    col->stock[COLONIZE_CARGO_TOOLS] = 0;
    col->stock[COLONIZE_CARGO_MUSKETS] = 0;
    eu.status[0] = '\0';
    ai_popup_clear(&pops);
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, &food_gate, &eu, 0, &prod, &pops, &game_txt, NULL);
    if (strstr(eu.status, "tools") == NULL) {
      fprintf(stderr, "K TOOLS status want tools got '%s'\n", eu.status);
      assets_msg_free(&game_txt);
      return 1;
    }
    if (pops.queue_count < 1 ||
        (strstr(pops.queue[0].body, "tools") == NULL &&
         strstr(pops.queue[0].body, "Boston") == NULL)) {
      fprintf(
        stderr,
        "K TOOLS popup weak q=%d body='%s'\n",
        pops.queue_count,
        pops.queue_count > 0 ? pops.queue[0].body : ""
      );
      assets_msg_free(&game_txt);
      return 1;
    }
    assets_msg_free(&game_txt);
    fprintf(stderr, "Phase K TOOLS chrome ok\n");

    /* Phase K @COTTON / @TOBACCO craft-raw chrome. */
    (void)assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT");
    snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Weaver's House");
    col->building_in_production = -1;
    col->colonists[0].building_type = 0;
    col->colonists[0].profession = COLONIZE_PROF_WEAVER;
    col->stock[COLONIZE_CARGO_COTTON] = 0;
    col->stock[COLONIZE_CARGO_SUGAR] = 5;
    col->stock[COLONIZE_CARGO_TOBACCO] = 5;
    col->stock[COLONIZE_CARGO_FURS] = 5;
    col->stock[COLONIZE_CARGO_LUMBER] = 5;
    col->stock[COLONIZE_CARGO_ORE] = 5;
    col->stock[COLONIZE_CARGO_TOOLS] = 5;
    col->stock[COLONIZE_CARGO_MUSKETS] = 5;
    eu.status[0] = '\0';
    ai_popup_clear(&pops);
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, &food_gate, &eu, 0, &prod, &pops, &game_txt, NULL);
    if (strstr(eu.status, "cotton") == NULL) {
      fprintf(stderr, "K COTTON status want cotton got '%s'\n", eu.status);
      assets_msg_free(&game_txt);
      return 1;
    }
    if (pops.queue_count < 1 ||
        (strstr(pops.queue[0].body, "cotton") == NULL &&
         strstr(pops.queue[0].body, "Boston") == NULL)) {
      fprintf(
        stderr,
        "K COTTON popup weak q=%d body='%s'\n",
        pops.queue_count,
        pops.queue_count > 0 ? pops.queue[0].body : ""
      );
      assets_msg_free(&game_txt);
      return 1;
    }
    fprintf(stderr, "Phase K COTTON chrome ok\n");

    snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Tobacconist's House");
    col->colonists[0].profession = COLONIZE_PROF_TOBACCONIST;
    col->stock[COLONIZE_CARGO_COTTON] = 5;
    col->stock[COLONIZE_CARGO_TOBACCO] = 0;
    eu.status[0] = '\0';
    ai_popup_clear(&pops);
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production(&pool, NULL, &food_gate, &eu, 0, &prod, &pops, &game_txt, NULL);
    if (strstr(eu.status, "tobacco") == NULL) {
      fprintf(stderr, "K TOBACCO status want tobacco got '%s'\n", eu.status);
      assets_msg_free(&game_txt);
      return 1;
    }
    if (pops.queue_count < 1 ||
        (strstr(pops.queue[0].body, "tobacco") == NULL &&
         strstr(pops.queue[0].body, "Boston") == NULL)) {
      fprintf(
        stderr,
        "K TOBACCO popup weak q=%d body='%s'\n",
        pops.queue_count,
        pops.queue_count > 0 ? pops.queue[0].body : ""
      );
      assets_msg_free(&game_txt);
      return 1;
    }
    assets_msg_free(&game_txt);
    fprintf(stderr, "Phase K TOBACCO chrome ok\n");
  }

  fprintf(stderr, "turn tests ok\n");
  diag_shutdown();
  return 0;
}
