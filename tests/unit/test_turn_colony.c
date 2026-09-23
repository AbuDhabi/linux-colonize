/* Slice of the former tests/unit/test_turn.c (split by feature 2026-09-23):
 * colony EOT beats: century cargo-ready, hammers/lumber, fog reveal, tools need, refresh vs pioneer work. */
#include "test_turn_common.h"

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
  colonies_set_occupancy_map(NULL);
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
  turn_run_colony_production_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(&pool), .map=(ColonizeWorldMap*)(NULL), .col1=(ColonizeCol1Save*)(&tipcol), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL), .europe=(EuropeScreen*)(&eu)}, 0, &prod, &pops, &game_txt);
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
  turn_run_colony_production_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(&pool), .map=(ColonizeWorldMap*)(NULL), .col1=(ColonizeCol1Save*)(&tipcol), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL), .europe=(EuropeScreen*)(&eu)}, 0, &prod, &pops, &game_txt);
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
  turn_run_colony_production_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(&pool), .map=(ColonizeWorldMap*)(NULL), .col1=(ColonizeCol1Save*)(&tipcol), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL), .europe=(EuropeScreen*)(&eu)}, 0, &prod, &pops, &game_txt);
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
  turn_run_colony_production_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(&pool), .map=(ColonizeWorldMap*)(NULL), .col1=(ColonizeCol1Save*)(&tipcol), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL), .europe=(EuropeScreen*)(&eu)}, 0, &prod, &pops, &game_txt);
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

/*
 * bugs.md #466: "hammer capacity > lumber income, lumber fell one turn then
 * returned to maximum the next". Every tick banks hammers and debits lumber
 * 1:1, in every season — the Spring/Autumn calendar is display only (the
 * old Autumn-freeze gate rested on a real-DOS pair with no carpenter staffed
 * anywhere). Lumber must fall on both consecutive turns, pre- and post-1600.
 */
static int unit_hammers_lumber_two_turns(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Carpenter's Shop");
  pool.building_type_count = 1;

  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Timberton");
  col->building_in_production = -1; /* bank hammers, no completion noise */
  col->has_building[0] = true;
  col->stock[COLONIZE_CARGO_FOOD] = 200;
  col->stock[COLONIZE_CARGO_LUMBER] = 40;
  col->colonists[0].active = true;
  col->colonists[0].building_type = 0;
  col->colonists[0].profession = COLONIZE_PROF_CARPENTER;
  col->colonists[0].field_job = -1;
  for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES_MAX; ++t) {
    col->tiles[t] = -1;
  }
  col->colonist_count = 1;
  col->population = 1;
  pool.colony_count = 1;

  ColonizeCol1Save save;
  memset(&save, 0, sizeof(save));
  save.head.year = 1550; /* pre-1600: every turn is a Spring tick */
  save.head.autumn = 0;

  /* Lumber income (2/turn) < carpenter capacity (6/turn). */
  const int income = 2;
  int prev = col->stock[COLONIZE_CARGO_LUMBER];
  for (int turn = 0; turn < 2; ++turn) {
    col->stock[COLONIZE_CARGO_LUMBER] += income;
    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production_w(
      &(ColonizeWorld){
        .colonies = &pool, .map = NULL, .col1 = &save, .col1_ok = true,
        .rng = NULL, .europe = NULL},
      0, &prod, NULL, NULL
    );
    const int now = col->stock[COLONIZE_CARGO_LUMBER];
    if (now >= prev) {
      fprintf(
        stderr, "#466 pre-1600 turn %d lumber must fall: %d -> %d\n", turn, prev, now
      );
      return 1;
    }
    prev = now;
  }
  if (col->hammers != 12) {
    fprintf(stderr, "#466 pre-1600 hammers want 12 got %d\n", col->hammers);
    return 1;
  }

  /* Post-1600: both the Spring and the Autumn tick spend. */
  save.head.year = 1680;
  col->hammers = 0;
  col->stock[COLONIZE_CARGO_LUMBER] = 40;
  for (int turn = 0; turn < 2; ++turn) {
    save.head.autumn = (uint16_t)turn; /* 0 = Spring tick, 1 = Autumn tick */
    const int before = col->stock[COLONIZE_CARGO_LUMBER];
    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production_w(
      &(ColonizeWorld){
        .colonies = &pool, .map = NULL, .col1 = &save, .col1_ok = true,
        .rng = NULL, .europe = NULL},
      0, &prod, NULL, NULL
    );
    const int now = col->stock[COLONIZE_CARGO_LUMBER];
    if (now != before - 6) {
      fprintf(
        stderr, "#466 post-1600 %s lumber want %d got %d\n",
        turn == 0 ? "Spring" : "Autumn", before - 6, now
      );
      return 1;
    }
  }
  if (col->hammers != 12) {
    fprintf(stderr, "#466 post-1600 hammers want 12 got %d\n", col->hammers);
    return 1;
  }
  fprintf(stderr, "#466 hammers/lumber two-turn drain ok\n");
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
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_set_occupancy_map(NULL);
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
  colonies_set_occupancy_map(NULL);
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
  for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES_MAX; ++t) {
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
  turn_run_colony_production_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(&pool), .map=(ColonizeWorldMap*)(NULL), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL), .europe=(EuropeScreen*)(&eu)}, 0, &prod, &pops, &game_txt);
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
  colonies_set_occupancy_map(NULL);
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
  for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES_MAX; ++t) {
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
  turn_run_colony_production_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(&pool), .map=(ColonizeWorldMap*)(NULL), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL), .europe=(EuropeScreen*)(&eu)}, 0, &prod, &pops, &game_txt);
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

/*
 * bugs.md #626: the per-nation move refresh restores the allotment and does
 * NOT run the pioneer work bodies. DOS runs them from the lasting-order
 * dispatcher FUN_2b5a_3ae6 (raw 46414) when the unit comes up in the human
 * activation rotation (raw 46873) -- so a human pioneer is ticked once per
 * turn by game_select_next_unit_awaiting_orders, and an AI unit parked on
 * order 9 is never ticked at all.
 */
static int refresh_does_not_tick_pioneer_work(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "#626: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "#626: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int pioneer = units_find_type(&pool, "Pioneers");
  assets_msg_free(&names);
  if (pioneer < 0) {
    fprintf(stderr, "#626: no Pioneers type\n");
    return 1;
  }

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "#626: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = 2; /* plains */
  }
  map.terrain[3 * map.width + 3] = 10; /* mixed forest: clear-forest is valid */

  units_set_occupancy_map(NULL);
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);

  const int hid = units_spawn(&pool, pioneer, 3, 3);
  const int aid = units_spawn(&pool, pioneer, 5, 5);
  ColonizeUnit* hu = units_get(&pool, hid);
  ColonizeUnit* au = units_get(&pool, aid);
  if (!hu || !au) {
    fprintf(stderr, "#626: pioneer spawn failed\n");
    map_free(&map);
    return 1;
  }
  hu->nation_id = 0;
  hu->tools = 100;
  hu->moves = 0;
  hu->col1_counter16 = 0;
  hu->orders = UNITS_ORDER_CLEAR_PLOW;
  au->nation_id = 1;
  au->tools = 100;
  au->moves = 0;
  au->col1_counter16 = 0;
  au->orders = UNITS_ORDER_BUILD_ROAD;

  const ColonizeWorld w = {
    .units = &pool, .colonies = &colonies, .map = &map, .col1 = NULL, .col1_ok = false
  };
  turn_refresh_moves_for_nation_w(&w, 0, NULL, NULL);
  turn_refresh_moves_for_nation_w(&w, 1, NULL, NULL);

  int rc = 0;
  /*
   * bugs.md #713: the moves-spent byte is cleared by a single DOS day-top
   * pass over EVERY unit of EVERY nation (FUN_4d56_1b3a raw 6355-6357), not
   * by the per-nation refresh. The refresh must leave it alone; the day-top
   * helper must clear both nations at once.
   */
  hu->mp_spent_turn = 1;
  au->mp_spent_turn = 1;
  turn_refresh_moves_for_nation_w(&w, 0, NULL, NULL);
  if (hu->mp_spent_turn != 1 || au->mp_spent_turn != 1) {
    fprintf(stderr, "#713: the per-nation refresh must not clear the spent byte\n");
    rc = 1;
  }
  turn_clear_mp_spent_all_nations(&pool);
  if (hu->mp_spent_turn != 0 || au->mp_spent_turn != 0 ||
      hu->aboard_moves != -1 || au->aboard_moves != -1) {
    fprintf(stderr, "#713: the day-top pass must clear every nation's spent byte\n");
    rc = 1;
  }
  if (hu->col1_counter16 != 0 || hu->orders != UNITS_ORDER_CLEAR_PLOW) {
    fprintf(
      stderr, "#626: refresh ticked the human pioneer (counter=%u orders=%d)\n",
      (unsigned)hu->col1_counter16, (int)hu->orders
    );
    rc = 1;
  }
  if (hu->moves != units_max_mp(&pool, hid)) {
    fprintf(stderr, "#626: refresh must still restore the allotment (%d)\n", hu->moves);
    rc = 1;
  }
  if (au->col1_counter16 != 0 || au->orders != UNITS_ORDER_BUILD_ROAD ||
      map_tile_has_road(&map, 5, 5)) {
    fprintf(stderr, "#626: an AI unit's order-9 park must never be advanced\n");
    rc = 1;
  }
  /* Precondition for the new tick site: the ascending-id rotation still
   * stops on the order-8 unit (DOS raw 42296-42300 classes {5,6,8,9} as
   * "lasting" -- active, but never waiting for input). */
  pool.selected_id = -1;
  hu->moves = units_max_mp(&pool, hid);
  if (!turn_select_next_unit(&pool, 0) || pool.selected_id != hid) {
    fprintf(stderr, "#626: rotation skipped the working pioneer\n");
    rc = 1;
  }
  map_free(&map);
  return rc;
}

static const TestCase k_cases[] = {
    {"unit_century_cargoready", unit_century_cargoready},
    {"unit_hammers_lumber_two_turns", unit_hammers_lumber_two_turns},
    {"unit_eot_fog_reveal", unit_eot_fog_reveal},
    {"unit_needtools0", unit_needtools0},
    {"unit_needtools", unit_needtools},
    {"refresh_does_not_tick_pioneer_work", refresh_does_not_tick_pioneer_work},
};
TEST_MAIN(k_cases)
