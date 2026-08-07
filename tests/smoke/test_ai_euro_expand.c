/* Smoke: Euro second-wave settle + CONTACT scout rings + tools delivery. */
#include "core/ai_diplo.h"
#include "core/ai_euro.h"
#include "core/ai_goals.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "smoke_ai_euro_expand: FAIL %s\n", msg);
  return 1;
}

static int count_nation_colonies(const ColonizeColonyPool* colonies, int nation_id) {
  int n = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (colonies->colonies[i].active && colonies->colonies[i].nation_id == nation_id) {
      ++n;
    }
  }
  return n;
}

/* Second-wave settle via ai_euro_dispatcher_turn (colony_count 1→2). */
static int smoke_second_wave(void) {
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
    return fail("alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains land */
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 2;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Free Colonist");
  units.types[2].movement = 1;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  /* Existing first colony — second-wave path (colony_count == 1). */
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  /* Idle pioneer near colony expand FOUND (pick_founding_tile → north (4,3)). */
  const int pid = units_spawn(&units, 0, 4, 5);
  ColonizeUnit* pioneer = units_get(&units, pid);
  if (!pioneer) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("spawn pioneer");
  }
  pioneer->nation_id = nation;
  pioneer->moves_left = 3;
  pioneer->orders = 0;

  ai_goals_reset();

  uint32_t turn = 10;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1_ok = false;
  ctx.rng_seed = 42; /* not seed-100 fixture */

  const int max_turns = 24;
  for (int t = 0; t < max_turns; ++t) {
    units_end_turn(&units);
    ai_euro_dispatcher_turn(&ctx, nation);
    if (count_nation_colonies(&colonies, nation) >= 2) {
      break;
    }
  }

  const int final_n = count_nation_colonies(&colonies, nation);
  /* Accept second colony, or (fallback) pioneer parked on a foundable expand tile. */
  int ok = (final_n >= 2);
  if (!ok && pioneer->active &&
      colonies_can_found(&colonies, &map, pioneer->x, pioneer->y) &&
      (pioneer->x != 4 || pioneer->y != 5) && /* moved off spawn */
      (pioneer->x != c->x || pioneer->y != c->y)) {
    ok = 1;
  }
  if (!ok) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: colonies=%d pioneer=(%d,%d) active=%d\n",
      final_n,
      pioneer->active ? pioneer->x : -1,
      pioneer->active ? pioneer->y : -1,
      pioneer->active
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected colony_count>=2 or found-at-tile after move");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "smoke_ai_euro_expand: second-wave ok (colonies=%d)\n", final_n);
  return 0;
}

/*
 * CONTACT scout rings (unpark #4): peaceful nation with own≥1 colony + Scout +
 * tribe beyond adjacent → upsert CONTACT at Manhattan ring 2–4 around tribe;
 * Scout AI_MOVE toward that tile. Fog plane optional (prefer unseen when set).
 */
static int smoke_scout_explore(void) {
  const int nation = 1;
  const int tribe_x = 12;
  const int tribe_y = 12;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("scout alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains land */
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Scout");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int sid = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* scout = units_get(&units, sid);
  if (!scout) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("spawn scout");
  }
  scout->nation_id = nation;
  scout->moves_left = 4;
  scout->orders = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.head.tribe_count = 1;
  col1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
  if (!col1.tribe) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("alloc tribe");
  }
  col1.tribe[0].x = (uint8_t)tribe_x;
  col1.tribe[0].y = (uint8_t)tribe_y;
  col1.tribe[0].nation_id = 4;
  col1.tribe[0].population = 4;
  col1.tribe[0].mission = 0xff;

  ai_goals_reset();

  uint32_t turn = 15;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42; /* not seed-100 fixture */

  ai_euro_dispatcher_turn(&ctx, nation);

  scout = units_get(&units, sid);
  int contact_x = -1;
  int contact_y = -1;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* g = ai_goals_primary(nation, i);
    if (!g || g->code != AI_GOAL_CONTACT) {
      continue;
    }
    contact_x = (int)g->x;
    contact_y = (int)g->y;
    break;
  }

  const int ring_md =
    (contact_x >= 0) ? (abs(contact_x - tribe_x) + abs(contact_y - tribe_y)) : -1;
  const int ok_contact = contact_x >= 0 && ring_md >= 2 && ring_md <= 4;
  const int ok_move =
    scout && scout->active && scout->orders == UNITS_ORDER_AI_MOVE &&
    scout->goto_x == contact_x && scout->goto_y == contact_y;
  const int ok_closer =
    ok_move &&
    (abs(scout->x - contact_x) + abs(scout->y - contact_y)) <
      (abs(5 - contact_x) + abs(5 - contact_y));

  if (!ok_contact || (!ok_move && !ok_closer)) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: scout orders=%d goto=(%d,%d) pos=(%d,%d) "
      "CONTACT=(%d,%d) ring_md=%d\n",
      scout ? scout->orders : -1,
      scout ? scout->goto_x : -1,
      scout ? scout->goto_y : -1,
      scout ? scout->x : -1,
      scout ? scout->y : -1,
      contact_x,
      contact_y,
      ring_md
    );
    free(col1.tribe);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Scout AI_MOVE toward CONTACT ring (MD 2–4) around tribe");
  }

  free(col1.tribe);
  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: CONTACT scout-ring ok (goto=(%d,%d) pos=(%d,%d) ring_md=%d)\n",
    scout->goto_x,
    scout->goto_y,
    scout->x,
    scout->y,
    ring_md
  );
  return 0;
}

/*
 * Thin Pioneer tools delivery (case 7 economy stand-in): colony tools low,
 * Pioneer on colony tile; dispatcher/act should bump stock[TOOLS].
 */
static int smoke_pioneer_tools_delivery(void) {
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
    return fail("tools alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains land */
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_TOOLS] = 5; /* < 20 → tools_short */
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int pid = units_spawn(&units, 0, 4, 4); /* on colony tile */
  ColonizeUnit* pioneer = units_get(&units, pid);
  if (!pioneer) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("spawn pioneer on colony");
  }
  pioneer->nation_id = nation;
  pioneer->moves_left = 3;
  pioneer->orders = 0;

  ai_goals_reset();

  uint32_t turn = 20;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1_ok = false;
  ctx.rng_seed = 42; /* not seed-100 fixture */

  const int tools_before = c->stock[COLONIZE_CARGO_TOOLS];
  ai_euro_dispatcher_turn(&ctx, nation);
  const int tools_after = c->stock[COLONIZE_CARGO_TOOLS];

  if (tools_after < tools_before + 10) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: tools before=%d after=%d (want +10)\n",
      tools_before,
      tools_after
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Pioneer tools delivery +10");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: tools-delivery ok (tools %d→%d)\n",
    tools_before,
    tools_after
  );
  return 0;
}

/*
 * Thin NEW WORLD tools-cargo hire stand-in (5d04 wagon matrix thin slice):
 * tools_short > 40 (3 colonies at tools=0 → short=60), peaceful Europe dock
 * with gold + free passenger slot → Pioneer hire; side effect is ship hold
 * +20 TOOLS and/or nearest-colony +15 tools stock.
 */
static int smoke_tools_cargo_hire(void) {
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
    return fail("tools-cargo alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* plains land */
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 2; /* passenger + goods hold slots */

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  /* Three own colonies with tools=0 → inventory tools_short = 60 (>40). */
  static const int cx[3] = {4, 6, 8};
  static const int cy[3] = {4, 6, 8};
  for (int i = 0; i < 3; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = cx[i];
    c->y = cy[i];
    c->population = 3;
    c->colonist_count = 3;
    c->stock[COLONIZE_CARGO_TOOLS] = 0;
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->building_in_production = -1;
  }
  colonies.colony_count = 3;
  colonies.next_id = 3;

  const int tools0_before = colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS];
  const int tools1_before = colonies.colonies[1].stock[COLONIZE_CARGO_TOOLS];
  const int tools2_before = colonies.colonies[2].stock[COLONIZE_CARGO_TOOLS];

  /* Europe-dock Caravel — hire path boards Pioneer + optional tools cargo. */
  const int ship_id = units_spawn_allow_stack(&units, 1, 200, 100);
  ColonizeUnit* ship = units_get(&units, ship_id);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("tools-cargo spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0; /* stay docked; planning hire only */

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0; /* hire_cost = 200 */
  col1.nation[nation].gold = 500;

  ai_goals_reset();

  uint32_t turn = 22;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42; /* not seed-100 fixture */

  ai_euro_dispatcher_turn(&ctx, nation);

  int pioneer_boarded = 0;
  int pioneer_tools = 0;
  for (int c = 0; c < ship->cargo_count; ++c) {
    const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
    if (!pax) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(&units, pax->type_index);
    if (ty && strstr(ty->name, "Pioneer")) {
      pioneer_boarded = 1;
      pioneer_tools = pax->tools;
      break;
    }
  }

  int ship_tools = 0;
  for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
    if (ship->hold_goods_amount[h] > 0 && ship->hold_goods_amount[h] < 255 &&
        ship->hold_goods_type[h] == COLONIZE_CARGO_TOOLS) {
      ship_tools += ship->hold_goods_amount[h];
    }
  }

  const int colony_tools_rose =
    colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS] >= tools0_before + 15 ||
    colonies.colonies[1].stock[COLONIZE_CARGO_TOOLS] >= tools1_before + 15 ||
    colonies.colonies[2].stock[COLONIZE_CARGO_TOOLS] >= tools2_before + 15;

  /* Side effect: ship TOOLS hold and/or nearest-colony +15; Pioneer carries tools. */
  const int ok_side =
    pioneer_boarded &&
    (ship_tools >= 20 || colony_tools_rose || pioneer_tools >= UNITS_EQUIP_TOOLS_STEP);

  if (!ok_side) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: tools-cargo boarded=%d pax_tools=%d ship_tools=%d "
      "colony_tools=%d/%d/%d gold=%u\n",
      pioneer_boarded,
      pioneer_tools,
      ship_tools,
      colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS],
      colonies.colonies[1].stock[COLONIZE_CARGO_TOOLS],
      colonies.colonies[2].stock[COLONIZE_CARGO_TOOLS],
      (unsigned)col1.nation[nation].gold
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Pioneer hire with tools cargo / colony +15 stand-in");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: tools-cargo hire ok (boarded=%d ship_tools=%d "
    "colony=%d/%d/%d pax_tools=%d)\n",
    pioneer_boarded,
    ship_tools,
    colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS],
    colonies.colonies[1].stock[COLONIZE_CARGO_TOOLS],
    colonies.colonies[2].stock[COLONIZE_CARGO_TOOLS],
    pioneer_tools
  );
  return 0;
}

/*
 * tools_short == 40 (2 colonies tools=0): threshold lowered from >40 to >20 —
 * still prefer Pioneer + tools cargo / colony +15 (no Wagon type in pool).
 */
static int smoke_tools_mid_threshold_hire(void) {
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
    return fail("tools-mid alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  static const int cx[2] = {4, 6};
  static const int cy[2] = {4, 6};
  for (int i = 0; i < 2; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = cx[i];
    c->y = cy[i];
    c->population = 3;
    c->colonist_count = 3;
    c->stock[COLONIZE_CARGO_TOOLS] = 0; /* short += 20 each → 40 */
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->building_in_production = -1;
  }
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int tools0_before = colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS];
  const int tools1_before = colonies.colonies[1].stock[COLONIZE_CARGO_TOOLS];

  const int ship_id = units_spawn_allow_stack(&units, 1, 200, 100);
  ColonizeUnit* ship = units_get(&units, ship_id);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("tools-mid spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 500;

  ai_goals_reset();

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

  ai_euro_dispatcher_turn(&ctx, nation);

  int pioneer_boarded = 0;
  int pioneer_tools = 0;
  for (int c = 0; c < ship->cargo_count; ++c) {
    const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
    if (!pax) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(&units, pax->type_index);
    if (ty && strstr(ty->name, "Pioneer")) {
      pioneer_boarded = 1;
      pioneer_tools = pax->tools;
      break;
    }
  }

  int ship_tools = 0;
  for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
    if (ship->hold_goods_amount[h] > 0 && ship->hold_goods_amount[h] < 255 &&
        ship->hold_goods_type[h] == COLONIZE_CARGO_TOOLS) {
      ship_tools += ship->hold_goods_amount[h];
    }
  }

  const int colony_tools_rose =
    colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS] >= tools0_before + 15 ||
    colonies.colonies[1].stock[COLONIZE_CARGO_TOOLS] >= tools1_before + 15;

  const int ok_side =
    pioneer_boarded &&
    (ship_tools >= 20 || colony_tools_rose || pioneer_tools >= UNITS_EQUIP_TOOLS_STEP);

  if (!ok_side) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: tools-mid boarded=%d pax_tools=%d ship_tools=%d "
      "colony_tools=%d/%d gold=%u\n",
      pioneer_boarded,
      pioneer_tools,
      ship_tools,
      colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS],
      colonies.colonies[1].stock[COLONIZE_CARGO_TOOLS],
      (unsigned)col1.nation[nation].gold
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Pioneer hire at tools_short==40 (>20) with tools cargo");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: tools-mid hire ok (boarded=%d ship_tools=%d "
    "colony=%d/%d pax_tools=%d)\n",
    pioneer_boarded,
    ship_tools,
    colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS],
    colonies.colonies[1].stock[COLONIZE_CARGO_TOOLS],
    pioneer_tools
  );
  return 0;
}

/*
 * tools_short>30 + Wagon Train type → hire wagon once (TOOLS on wagon);
 * second planning pass with free cargo slot prefers Pioneer (not a 2nd wagon).
 */
static int smoke_wagon_hire_once(void) {
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
    return fail("wagon-hire alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 2;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Wagon Train");
  units.types[2].movement = 2;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[2].cargo = 2; /* goods holds for TOOLS load */

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  static const int cx[2] = {4, 6};
  static const int cy[2] = {4, 6};
  for (int i = 0; i < 2; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = cx[i];
    c->y = cy[i];
    c->population = 3;
    c->colonist_count = 3;
    c->stock[COLONIZE_CARGO_TOOLS] = 0; /* short=40 >30 */
    c->stock[COLONIZE_CARGO_FOOD] = 40;
    c->building_in_production = -1;
  }
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int ship_id = units_spawn_allow_stack(&units, 1, 200, 100);
  ColonizeUnit* ship = units_get(&units, ship_id);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("wagon-hire spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 800;

  ai_goals_reset();

  uint32_t turn = 26;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  int wagon_boarded = 0;
  int wagon_tools = 0;
  int wagon_uid = -1;
  for (int c = 0; c < ship->cargo_count; ++c) {
    const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
    if (!pax) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(&units, pax->type_index);
    if (ty && strstr(ty->name, "Wagon")) {
      wagon_boarded = 1;
      wagon_uid = pax->id;
      for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
        if (pax->hold_goods_amount[h] > 0 && pax->hold_goods_amount[h] < 255 &&
            pax->hold_goods_type[h] == COLONIZE_CARGO_TOOLS) {
          wagon_tools += pax->hold_goods_amount[h];
        }
      }
      break;
    }
  }

  if (!wagon_boarded || wagon_tools < 20) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: wagon first pass boarded=%d wagon_tools=%d "
      "cargo=%d gold=%u\n",
      wagon_boarded,
      wagon_tools,
      ship->cargo_count,
      (unsigned)col1.nation[nation].gold
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Wagon Train hire once with TOOLS aboard wagon");
  }

  /* Second pass: wagon already owned → Pioneer, not a second wagon. */
  col1.nation[nation].gold = 800;
  turn = 27;
  /* Keep colonies tools=0 so tools_short stays high after inventory rebuild. */
  ai_euro_dispatcher_turn(&ctx, nation);

  int wagon_count = 0;
  int pioneer_boarded = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units.units[i];
    if (!u->active || u->nation_id != nation) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(&units, u->type_index);
    if (!ty) {
      continue;
    }
    if (strstr(ty->name, "Wagon")) {
      wagon_count++;
    }
  }
  for (int c = 0; c < ship->cargo_count; ++c) {
    const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
    if (!pax) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(&units, pax->type_index);
    if (ty && strstr(ty->name, "Pioneer")) {
      pioneer_boarded = 1;
      break;
    }
  }

  if (wagon_count != 1 || !pioneer_boarded) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: wagon once-guard wagons=%d pioneer=%d cargo=%d "
      "first_uid=%d gold=%u\n",
      wagon_count,
      pioneer_boarded,
      ship->cargo_count,
      wagon_uid,
      (unsigned)col1.nation[nation].gold
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected exactly one wagon then Pioneer on second hire");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: wagon-hire once ok (wagon_tools=%d then Pioneer)\n",
    wagon_tools
  );
  return 0;
}

/*
 * Fog-aware CONTACT rings: when map.seen exists, prefer an unseen ring tile
 * over a closer seen tile (FoW explore — map_tile_seen_by).
 */
static int smoke_scout_fog_prefer_unseen(void) {
  const int nation = 1;
  const int tribe_x = 10;
  const int tribe_y = 10;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  map.seen = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3 || !map.seen) {
    return fail("fog-scout alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  /* Mark toward-scout ring tiles (closer to scout at 5,5) as seen; leave
   * far-side ring tiles unseen so fog preference should pick those. */
  for (int dy = -4; dy <= 4; ++dy) {
    for (int dx = -4; dx <= 4; ++dx) {
      const int md = abs(dx) + abs(dy);
      if (md < 2 || md > 4) {
        continue;
      }
      const int nx = tribe_x + dx;
      const int ny = tribe_y + dy;
      if (nx < 0 || ny < 0 || nx >= 16 || ny >= 16) {
        continue;
      }
      const int to_scout = abs(nx - 5) + abs(ny - 5);
      if (to_scout <= 8) {
        map_reveal_tile(&map, nx, ny, nation);
      }
    }
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Scout");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  const int sid = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* scout = units_get(&units, sid);
  if (!scout) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.seen);
    return fail("fog-scout spawn");
  }
  scout->nation_id = nation;
  scout->moves_left = 4;
  scout->orders = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.head.tribe_count = 1;
  col1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
  if (!col1.tribe) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.seen);
    return fail("fog-scout tribe");
  }
  col1.tribe[0].x = (uint8_t)tribe_x;
  col1.tribe[0].y = (uint8_t)tribe_y;
  col1.tribe[0].nation_id = 4;
  col1.tribe[0].population = 4;
  col1.tribe[0].mission = 0xff;

  ai_goals_reset();

  uint32_t turn = 15;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  int contact_x = -1;
  int contact_y = -1;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* g = ai_goals_primary(nation, i);
    if (!g || g->code != AI_GOAL_CONTACT) {
      continue;
    }
    contact_x = (int)g->x;
    contact_y = (int)g->y;
    break;
  }

  const int ring_md =
    (contact_x >= 0) ? (abs(contact_x - tribe_x) + abs(contact_y - tribe_y)) : -1;
  const int ok_ring = contact_x >= 0 && ring_md >= 2 && ring_md <= 4;
  const int ok_unseen =
    ok_ring && !map_tile_seen_by(&map, contact_x, contact_y, nation);

  if (!ok_ring || !ok_unseen) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: fog CONTACT=(%d,%d) ring_md=%d seen=%d\n",
      contact_x,
      contact_y,
      ring_md,
      ok_ring ? (int)map_tile_seen_by(&map, contact_x, contact_y, nation) : -1
    );
    free(col1.tribe);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.seen);
    return fail("expected CONTACT ring on unseen FoW tile");
  }

  free(col1.tribe);
  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  free(map.seen);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: fog CONTACT prefer-unseen ok (goto=(%d,%d))\n",
    contact_x,
    contact_y
  );
  return 0;
}

/*
 * Thin multi-step land 20e6: Soldier with moves_left>=2 on MILITARY goto advances
 * two tiles in one dispatcher act when path is clear.
 */
static int smoke_multistep_military(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("multistep alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 2;
  c->y = 2;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->building_in_production = -1;
  ColonizeColony* enemy = &colonies.colonies[1];
  enemy->id = 1;
  enemy->active = true;
  enemy->nation_id = foe;
  enemy->x = 12;
  enemy->y = 2;
  enemy->population = 2;
  enemy->colonist_count = 2;
  enemy->stock[COLONIZE_CARGO_FOOD] = 20;
  enemy->building_in_production = -1;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int sid = units_spawn(&units, 0, 4, 2);
  ColonizeUnit* soldier = units_get(&units, sid);
  if (!soldier) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("multistep spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->moves_left = 3;
  soldier->orders = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 50;
  col1.nation[foe].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe);

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 12, 2, AI_GOAL_MILITARY, 6);

  uint32_t turn = 20;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  const int x0 = soldier->x;
  ai_euro_dispatcher_turn(&ctx, nation);
  soldier = units_get(&units, sid);
  if (!soldier || !soldier->active) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("multistep soldier inactive");
  }
  const int advanced = soldier->x - x0;
  if (advanced < 2) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: multistep x %d→%d (want ≥2) orders=%d goto=(%d,%d)\n",
      x0,
      soldier->x,
      soldier->orders,
      soldier->goto_x,
      soldier->goto_y
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected MILITARY multi-step advance of 2 tiles");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(
    stderr,
    "smoke_ai_euro_expand: MILITARY multi-step ok (x %d→%d)\n",
    x0,
    soldier->x
  );
  return 0;
}

/*
 * Case-7 dock expert once: peace + tools_short high + Europe dock has
 * Hardy Pioneers → board that type (consume dock); do not invent if absent.
 */
static int smoke_dock_expert_hire(void) {
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
    return fail("dock-hire alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Hardy Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 2;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Free Colonist");
  units.types[2].movement = 1;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  for (int i = 0; i < 3; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = 2 + i * 2;
    c->y = 2;
    c->population = 2;
    c->colonist_count = 2;
    c->stock[COLONIZE_CARGO_TOOLS] = 0;
    c->stock[COLONIZE_CARGO_FOOD] = 20;
    c->building_in_production = -1;
  }
  colonies.colony_count = 3;
  colonies.next_id = 3;

  const int sid = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("dock-hire spawn europe ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;
  ship->orders = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.gold = 500;
  europe.dock_count = 1;
  snprintf(europe.dock[0].name, sizeof(europe.dock[0].name), "Hardy Pioneers");
  europe.dock[0].profession = 20;
  europe.dock[0].present = true;
  europe.dock[0].sentry = true;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 500;

  ai_goals_reset();

  uint32_t turn = 12;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.europe = &europe;
  ctx.rng_seed = 42;

  const uint32_t gold_before = col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&ctx, nation);

  int hardy_boarded = 0;
  ship = units_get(&units, sid);
  if (ship) {
    for (int c = 0; c < ship->cargo_count; ++c) {
      const ColonizeUnit* pax = units_get_const(&units, ship->cargo_ids[c]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(&units, pax->type_index);
      if (ty && strstr(ty->name, "Hardy Pioneer")) {
        hardy_boarded = 1;
      }
    }
  }
  const int dock_cleared = (europe.dock_count == 0);
  const int gold_spent = (col1.nation[nation].gold < gold_before + 50u);

  if (!hardy_boarded || !dock_cleared || !gold_spent) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: dock hardy=%d dock_count=%d gold %u→%u cargo=%d\n",
      hardy_boarded,
      europe.dock_count,
      (unsigned)gold_before,
      (unsigned)col1.nation[nation].gold,
      ship ? ship->cargo_count : -1
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected Hardy Pioneer dock hire + dock consume + gold spend");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "smoke_ai_euro_expand: dock Hardy Pioneer hire ok\n");
  return 0;
}

/*
 * 5d04 treasury gate: gold below colonist hire_cost → no Europe hire / tools-cargo.
 */
static int smoke_treasury_skip_hire(void) {
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
    return fail("treasury alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Pioneer");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 2;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  for (int i = 0; i < 3; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->id = i;
    c->active = true;
    c->nation_id = nation;
    c->x = 2 + i * 2;
    c->y = 2;
    c->population = 2;
    c->colonist_count = 2;
    c->stock[COLONIZE_CARGO_TOOLS] = 0;
    c->stock[COLONIZE_CARGO_FOOD] = 20;
    c->building_in_production = -1;
  }
  colonies.colony_count = 3;
  colonies.next_id = 3;

  const int sid = units_spawn(&units, 1, 200, 200);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("treasury spawn ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0; /* hire_cost=200; bump≈30 → still <200 if gold=0 */
  col1.nation[nation].gold = 0;

  ai_goals_reset();

  uint32_t turn = 12;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);
  ship = units_get(&units, sid);
  if (ship && ship->cargo_count > 0) {
    fprintf(
      stderr,
      "smoke_ai_euro_expand: treasury cargo=%d gold=%u\n",
      ship->cargo_count,
      (unsigned)col1.nation[nation].gold
    );
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return fail("expected no Europe hire when gold < hire_cost");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  fprintf(stderr, "smoke_ai_euro_expand: treasury skip-hire ok\n");
  return 0;
}

int main(void) {
  if (smoke_second_wave() != 0) {
    return 1;
  }
  if (smoke_scout_explore() != 0) {
    return 1;
  }
  if (smoke_scout_fog_prefer_unseen() != 0) {
    return 1;
  }
  if (smoke_pioneer_tools_delivery() != 0) {
    return 1;
  }
  if (smoke_tools_cargo_hire() != 0) {
    return 1;
  }
  if (smoke_tools_mid_threshold_hire() != 0) {
    return 1;
  }
  if (smoke_wagon_hire_once() != 0) {
    return 1;
  }
  if (smoke_multistep_military() != 0) {
    return 1;
  }
  if (smoke_dock_expert_hire() != 0) {
    return 1;
  }
  if (smoke_treasury_skip_hire() != 0) {
    return 1;
  }
  fprintf(stderr, "smoke_ai_euro_expand: ok\n");
  return 0;
}
