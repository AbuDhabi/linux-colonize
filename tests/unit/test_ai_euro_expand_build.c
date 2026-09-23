/* Slice of the former tests/unit/test_ai_euro_expand.c (split by feature 2026-09-23):
 * construction labor and building preference ladder (stockade..capitol). */
#include "test_ai_euro_expand_common.h"

static int unit_build_ai_flags_wants_construction(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("build-ai-flags alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Free Colonist");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = -1; /* no named queue — bit alone */
  c->build_ai_flags = COLONIZE_BUILD_AI_WANTS_CONSTRUCTION;
  c->labor_shortage = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* col = units_get(&units, uid);
  if (!col) {
    fx_map_free(&map);
    return fail("build-ai-flags spawn");
  }
  col->nation_id = nation;
  col->orders = 0;
  col->moves = 1 * UNITS_MP_PER_TILE;

  ai_goals_reset();
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 62;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  const int pop_before = c->population;
  ai_euro_dispatcher_turn(&ctx, nation);

  col = units_get(&units, uid);
  c = &colonies.colonies[0];
  const int joined = (col == NULL || !col->active) && c->population == pop_before + 1;
  if (!joined) {
    fprintf(stderr,
      "unit_ai_euro_expand: build-ai joined=%d pop=%d→%d flags=0x%02x labor=%u active=%d\n",
      joined, pop_before, c->population, (unsigned)c->build_ai_flags,
      (unsigned)c->labor_shortage, col && col->active);
    fx_map_free(&map);
    return fail("expected LABOR join from build_ai_flags wants_construction");
  }

  /* clear_construction drops bit7 */
  c->build_ai_flags = COLONIZE_BUILD_AI_WANTS_CONSTRUCTION;
  c->building_in_production = 0;
  colonies_clear_construction(&colonies, c->id);
  if ((c->build_ai_flags & COLONIZE_BUILD_AI_WANTS_CONSTRUCTION) != 0 ||
      c->building_in_production != -1) {
    fx_map_free(&map);
    return fail("expected clear_construction to drop wants_construction bit");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: build_ai_flags wants_construction ok\n");
  return 0;
}

static int unit_construction_labor_stockade(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("construction alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_type_count = 1;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40; /* no tools-delivery lure */
  c->building_in_production = 0; /* Stockade */
  c->hammers = 10;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* pioneer = units_get(&units, uid);
  if (!pioneer) {
    fx_map_free(&map);
    return fail("construction spawn pioneer");
  }
  pioneer->nation_id = nation;
  pioneer->orders = 0;
  pioneer->moves = 3 * UNITS_MP_PER_TILE;

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 12, 12, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 24;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  const int pop0 = c->population;
  ai_euro_dispatcher_turn(&ctx, nation);

  pioneer = units_get(&units, uid);
  const int joined = (pioneer == NULL || !pioneer->active) && c->population > pop0;
  const int labor_stay =
    pioneer && pioneer->active && pioneer->x == 4 && pioneer->y == 4 &&
    (pioneer->orders == UNITS_ORDER_AI_MOVE || pioneer->orders == 0) &&
    ((pioneer->goto_x == 4 && pioneer->goto_y == 4) ||
     ai_goals_max_primary_prio(nation, 4, 4, AI_GOAL_LABOR) >= 4);
  const int left_for_found =
    pioneer && pioneer->active && (pioneer->x != 4 || pioneer->y != 4 ||
                                   (pioneer->goto_x == 12 && pioneer->goto_y == 12));

  if (left_for_found || (!joined && !labor_stay)) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: construction joined=%d labor_stay=%d left=%d "
      "pop %d→%d active=%d pos=(%d,%d) goto=(%d,%d) orders=%d labor_prio=%d colonies=%d (c0=(%d,%d) c1=(%d,%d))\n",
      joined,
      labor_stay,
      left_for_found,
      pop0,
      c->population,
      pioneer && pioneer->active,
      pioneer ? pioneer->x : -1,
      pioneer ? pioneer->y : -1,
      pioneer ? pioneer->goto_x : -1,
      pioneer ? pioneer->goto_y : -1,
      pioneer ? pioneer->orders : -1,
      ai_goals_max_primary_prio(nation, 4, 4, AI_GOAL_LABOR),
      colonies.colony_count,
      colonies.colonies[0].x, colonies.colonies[0].y,
      colonies.colony_count > 1 ? colonies.colonies[1].x : -1,
      colonies.colony_count > 1 ? colonies.colonies[1].y : -1
    );
    fx_map_free(&map);
    return fail("expected Pioneer stay/LABOR for Stockade construction");
  }

  fx_map_free(&map);
  fprintf(
    stderr,
    "unit_ai_euro_expand: construction Stockade LABOR ok (joined=%d)\n",
    joined
  );
  return 0;
}

/*
 * Master Carpenter construction LABOR: idle Master Carpenter on colony with
 * incomplete Stockade → stay/join LABOR (Stockade pattern). Cite: euro_unit_act
 * §2e; docs/building_production.md Carpenter→Hammers; Skills Chart.
 */
static int unit_master_carpenter_construction_labor(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("carpenter-labor alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Master Carpenter");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_type_count = 1;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = 0; /* Stockade incomplete */
  c->hammers = 10;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* carpenter = units_get(&units, uid);
  if (!carpenter) {
    fx_map_free(&map);
    return fail("carpenter-labor spawn");
  }
  carpenter->nation_id = nation;
  carpenter->orders = 0;
  carpenter->moves = 3 * UNITS_MP_PER_TILE;
  carpenter->profession = 13; /* @JOB Carpenter */

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 12, 12, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 25;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  const int pop0 = c->population;
  ai_euro_dispatcher_turn(&ctx, nation);

  carpenter = units_get(&units, uid);
  const int joined = (carpenter == NULL || !carpenter->active) && c->population > pop0;
  const int labor_stay =
    carpenter && carpenter->active && carpenter->x == 4 && carpenter->y == 4 &&
    (carpenter->orders == UNITS_ORDER_AI_MOVE || carpenter->orders == 0) &&
    ((carpenter->goto_x == 4 && carpenter->goto_y == 4) ||
     ai_goals_max_primary_prio(nation, 4, 4, AI_GOAL_LABOR) >= 4);
  const int left_for_found =
    carpenter && carpenter->active &&
    (carpenter->x != 4 || carpenter->y != 4 ||
     (carpenter->goto_x == 12 && carpenter->goto_y == 12));

  if (left_for_found || (!joined && !labor_stay)) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: carpenter joined=%d labor_stay=%d left=%d "
      "pop %d→%d active=%d pos=(%d,%d) goto=(%d,%d) orders=%d labor_prio=%d\n",
      joined,
      labor_stay,
      left_for_found,
      pop0,
      c->population,
      carpenter && carpenter->active,
      carpenter ? carpenter->x : -1,
      carpenter ? carpenter->y : -1,
      carpenter ? carpenter->goto_x : -1,
      carpenter ? carpenter->goto_y : -1,
      carpenter ? carpenter->orders : -1,
      ai_goals_max_primary_prio(nation, 4, 4, AI_GOAL_LABOR)
    );
    fx_map_free(&map);
    return fail("expected Master Carpenter stay/LABOR for Stockade construction");
  }

  fx_map_free(&map);
  fprintf(
    stderr,
    "unit_ai_euro_expand: Master Carpenter construction LABOR ok (joined=%d)\n",
    joined
  );
  return 0;
}

/*
 * Expert Lumberjack LABOR: idle Expert Lumberjack on colony with incomplete
 * Warehouse (building type exists) → stay/join LABOR. Field-assign PARKED.
 * Cite: euro_unit_act §2e; docs/building_production.md Lumberjack→Lumber;
 * Colonization.pdf Skills Chart.
 */
static int unit_lumberjack_warehouse_labor(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("lumberjack-labor alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Expert Lumberjack");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Warehouse");
  colonies.building_types[0].hammers = 80;
  colonies.building_type_count = 1;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->building_in_production = 0; /* Warehouse incomplete */
  c->hammers = 10;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* lumber = units_get(&units, uid);
  if (!lumber) {
    fx_map_free(&map);
    return fail("lumberjack-labor spawn");
  }
  lumber->nation_id = nation;
  lumber->orders = 0;
  lumber->moves = 3 * UNITS_MP_PER_TILE;
  lumber->profession = 5; /* @JOB Lumberjack */

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 12, 12, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 20;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 11;

  const int pop0 = c->population;
  ai_euro_dispatcher_turn(&ctx, nation);

  lumber = units_get(&units, uid);
  const int joined = (lumber == NULL || !lumber->active) && c->population > pop0;
  const int labor_stay =
    lumber && lumber->active && lumber->x == 4 && lumber->y == 4 &&
    (lumber->orders == UNITS_ORDER_AI_MOVE || lumber->orders == 0) &&
    ((lumber->goto_x == 4 && lumber->goto_y == 4) ||
     ai_goals_max_primary_prio(nation, 4, 4, AI_GOAL_LABOR) >= 4);
  const int left_for_found =
    lumber && lumber->active &&
    (lumber->x != 4 || lumber->y != 4 ||
     (lumber->goto_x == 12 && lumber->goto_y == 12));

  if (left_for_found || (!joined && !labor_stay)) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: lumberjack joined=%d labor_stay=%d left=%d "
      "active=%d pop %d→%d orders=%d goto=(%d,%d) labor_prio=%d\n",
      joined,
      labor_stay,
      left_for_found,
      lumber ? (int)lumber->active : 0,
      pop0,
      c->population,
      lumber ? lumber->orders : -1,
      lumber ? lumber->goto_x : -1,
      lumber ? lumber->goto_y : -1,
      ai_goals_max_primary_prio(nation, 4, 4, AI_GOAL_LABOR)
    );
    fx_map_free(&map);
    return fail("expected Expert Lumberjack stay/LABOR for Warehouse");
  }

  fx_map_free(&map);
  fprintf(
    stderr,
    "unit_ai_euro_expand: Lumberjack Warehouse LABOR ok (joined=%d)\n",
    joined
  );
  return 0;
}

/*
 * Peace construction pick: idle queue → Stockade before Warehouse/Docks.
 * Cite: fandom Defense Stockade→Fort→Fortress; building_production Stockade 64h.
 */
static int unit_peace_construction_stockade(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("peace-stockade alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains */
  }
  map.terrain[4 * 16 + 3] = 25; /* ocean west → coastal (Docks also buildable) */

  ColonizeUnitPool units;
  fx_units_init(&units);

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Warehouse");
  colonies.building_types[1].hammers = 80;
  colonies.building_types[1].min_population = 1;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Docks");
  colonies.building_types[2].hammers = 52;
  colonies.building_types[2].min_population = 1;
  colonies.building_type_count = 3;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = false;
  c->has_building[1] = false;
  c->has_building[2] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 30;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 21;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 0) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: peace construct bip=%d (want Stockade=0)\n",
      c->building_in_production
    );
    fx_map_free(&map);
    return fail("expected idle colony to prefer Stockade over Warehouse/Docks");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: peace construction Stockade prefer ok\n");
  return 0;
}

/*
 * Peace construction: Stockade owned → Fort before Warehouse/Docks.
 * Cite: fandom Defense Stockade→Fort→Fortress; building_production Fort 120h.
 */
static int unit_peace_construction_fort(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("peace-fort alloc map");
  }
  map.terrain[4 * 16 + 3] = 25;

  ColonizeUnitPool units;
  fx_units_init(&units);

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Fort");
  colonies.building_types[1].hammers = 120;
  colonies.building_types[1].min_population = 4;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Warehouse");
  colonies.building_types[2].hammers = 80;
  colonies.building_types[2].min_population = 1;
  snprintf(colonies.building_types[3].name, sizeof(colonies.building_types[3].name), "Docks");
  colonies.building_types[3].hammers = 52;
  colonies.building_types[3].min_population = 1;
  colonies.building_type_count = 4;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 4;
  c->colonist_count = 4;
  for (int i = 0; i < 4; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 100;
  c->has_building[0] = true; /* Stockade */
  c->has_building[1] = false;
  c->has_building[2] = false;
  c->has_building[3] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 31;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 22;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 1) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: fort prefer bip=%d (want Fort=1)\n",
      c->building_in_production
    );
    fx_map_free(&map);
    return fail("expected Stockade colony to prefer Fort over Warehouse/Docks");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: peace construction Fort prefer ok\n");
  return 0;
}

/*
 * Peace Schoolhouse prefer: Stockade+Church+Press owned, pop≥4 → Schoolhouse.
 * Cite: building_production Schoolhouse; euro_unit_act Education prefer.
 */
static int unit_peace_schoolhouse_prefer(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("peace-school alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Church");
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 3;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Printing Press");
  colonies.building_types[2].hammers = 52;
  colonies.building_types[2].min_population = 1;
  snprintf(colonies.building_types[3].name, sizeof(colonies.building_types[3].name), "Schoolhouse");
  colonies.building_types[3].hammers = 64;
  colonies.building_types[3].min_population = 4;
  colonies.building_type_count = 4;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 4;
  c->colonist_count = 4;
  for (int i = 0; i < 4; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = true;
  c->has_building[3] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 34;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 25;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 3) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: peace Schoolhouse bip=%d (want Schoolhouse=3)\n",
      c->building_in_production
    );
    fx_map_free(&map);
    return fail("expected idle educated colony to prefer Schoolhouse");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: peace Schoolhouse prefer ok\n");
  return 0;
}

/*
 * Peace Cathedral prefer: Church owned, pop≥8 → Cathedral.
 */
static int unit_peace_cathedral_prefer(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("peace-cathedral alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Church");
  colonies.building_types[1].hammers = 52;
  colonies.building_types[1].min_population = 3;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Cathedral");
  colonies.building_types[2].hammers = 176;
  colonies.building_types[2].min_population = 8;
  colonies.building_type_count = 3;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 8;
  c->colonist_count = 8;
  for (int i = 0; i < 8; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 38;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 29;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 2) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: peace Cathedral bip=%d (want Cathedral=2)\n",
      c->building_in_production
    );
    fx_map_free(&map);
    return fail("expected idle Church colony to prefer Cathedral");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: peace Cathedral prefer ok\n");
  return 0;
}

/*
 * Iron Works prefer: Adam Smith + Blacksmith's Shop → Iron Works.
 */
static int unit_iron_works_prefer(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("iron-works alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(
    colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Blacksmith's Shop"
  );
  colonies.building_types[1].hammers = 64;
  colonies.building_types[1].min_population = 1;
  snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Iron Works");
  colonies.building_types[2].hammers = 240;
  colonies.building_types[2].min_population = 8;
  colonies.building_type_count = 3;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 8;
  c->colonist_count = 8;
  for (int i = 0; i < 8; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;
  c->has_building[1] = true;
  c->has_building[2] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  col1.head.founding_father[FF_ADAM_SMITH] = (int8_t)nation;
  col1.nation[nation].founding_fathers[0] |= 1u;

  uint32_t turn = 43;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 34;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production != 2) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Iron Works bip=%d (want Iron Works=2)\n",
      c->building_in_production
    );
    fx_map_free(&map);
    return fail("expected AdamSmith+Shop colony to prefer Iron Works");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: Iron Works prefer ok\n");
  return 0;
}

/*
 * Capitol is never a construction project: DOS FUN_15eb_3650 zeroes
 * @BUILDING 0x1e for every colony, so a fortified colony with hammers and a
 * Capitol row in the table still picks nothing.
 */
static int unit_capitol_prefer(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("capitol alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_types[0].hammers = 64;
  colonies.building_types[0].min_population = 3;
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Capitol");
  colonies.building_types[1].hammers = 80;
  colonies.building_types[1].min_population = 1;
  colonies.building_type_count = 2;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;
  c->has_building[1] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 46;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 37;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production == 1) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Capitol bip=%d (want anything but Capitol=1)\n",
      c->building_in_production
    );
    fx_map_free(&map);
    return fail("Capitol is unbuildable in DOS; AI must never start one");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: Capitol never built ok\n");
  return 0;
}

/*
 * Capitol Expansion is unreachable too — it sits behind the Capitol as a
 * prerequisite, and DOS never lets a colony own one.
 */
static int unit_capitol_expansion_prefer(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("capitol-exp alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Capitol");
  colonies.building_types[0].hammers = 80;
  colonies.building_types[0].min_population = 1;
  snprintf(
    colonies.building_types[1].name,
    sizeof(colonies.building_types[1].name),
    "Capitol Expansion"
  );
  colonies.building_types[1].hammers = 80;
  colonies.building_types[1].min_population = 1;
  colonies.building_type_count = 2;

  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  for (int i = 0; i < 3; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = -1;
  }
  c->stock[COLONIZE_CARGO_FOOD] = 80;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->has_building[0] = true;
  c->has_building[1] = false;
  c->building_in_production = -1;
  c->hammers = 0;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 1;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 47;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 37;

  ai_euro_dispatcher_turn(&ctx, nation);

  if (c->building_in_production == 1) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: Capitol Expansion bip=%d (want anything but Expansion=1)\n",
      c->building_in_production
    );
    fx_map_free(&map);
    return fail("Capitol Expansion is unbuildable in DOS; AI must never start one");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: Capitol Expansion never built ok\n");
  return 0;
}

static const TestCase k_cases[] = {
    {"unit_build_ai_flags_wants_construction", unit_build_ai_flags_wants_construction},
    {"unit_construction_labor_stockade", unit_construction_labor_stockade},
    {"unit_master_carpenter_construction_labor", unit_master_carpenter_construction_labor},
    {"unit_lumberjack_warehouse_labor", unit_lumberjack_warehouse_labor},
    {"unit_peace_construction_stockade", unit_peace_construction_stockade},
    {"unit_peace_construction_fort", unit_peace_construction_fort},
    {"unit_peace_schoolhouse_prefer", unit_peace_schoolhouse_prefer},
    {"unit_peace_cathedral_prefer", unit_peace_cathedral_prefer},
    {"unit_iron_works_prefer", unit_iron_works_prefer},
    {"unit_capitol_prefer", unit_capitol_prefer},
    {"unit_capitol_expansion_prefer", unit_capitol_expansion_prefer},
};
TEST_MAIN(k_cases)
